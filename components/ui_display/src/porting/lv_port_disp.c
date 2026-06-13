#include "lv_port_disp.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "LV_PORT_DISP";

// --- Cấu hình Pin (V2.0 FINAL) ---
#define PIN_LCD_MOSI    23
#define PIN_LCD_SCK     18
#define PIN_LCD_MISO    19
#define PIN_LCD_CS      5
#define PIN_LCD_DC      27
#define PIN_LCD_RST     4
#define PIN_LCD_BLK     -1   // Wired to 3.3V (Always ON)

#define LCD_H_RES       320
#define LCD_V_RES       240

#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL    LEDC_CHANNEL_0
#define LEDC_DUTY_RES   LEDC_TIMER_8_BIT
#define LEDC_FREQUENCY  5000

static esp_lcd_panel_handle_t panel_handle = NULL;
static lv_disp_draw_buf_t disp_buf;
static lv_disp_drv_t disp_drv;

SemaphoreHandle_t vspi_bus_free = NULL;

// Callback ngắt khi truyền màu xong
static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
    // Dùng trực tiếp biến static global để đảm bảo an toàn vùng nhớ trong Interrupt
    signed portBASE_TASK_WOKEN = pdFALSE;
    xSemaphoreGiveFromISR(vspi_bus_free, &portBASE_TASK_WOKEN);
    lv_disp_flush_ready(&disp_drv);
    if (portBASE_TASK_WOKEN) portYIELD_FROM_ISR();
    return false;
}

static uint64_t last_flush_start = 0;

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map) {
    // Kiểm tra Timeout trễ (Recovery)
    if (last_flush_start != 0 && (esp_timer_get_time() - last_flush_start > 300000)) {
        // Nếu kẹt quá 300ms, ép giải phóng để vẽ khung tiếp theo
        xSemaphoreGive(vspi_bus_free);
        lv_disp_flush_ready(drv);
    }
    last_flush_start = esp_timer_get_time();

    // Chờ Bus rảnh
    if (xSemaphoreTake(vspi_bus_free, pdMS_TO_TICKS(100)) != pdTRUE) {
        lv_disp_flush_ready(drv);
        return;
    }

    // Khoảng thở cực ngắn cho phần cứng SPI
    esp_rom_delay_us(50);

    esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_map);
}

void lv_port_disp_set_backlight(uint8_t percent) {
    if (PIN_LCD_BLK == -1) return;
    if (percent > 100) percent = 100;
    uint32_t duty = (percent * ((1 << LEDC_DUTY_RES) - 1)) / 100;
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

void lv_port_disp_init(void) {
    ESP_LOGI(TAG, "Initializing Hardware (SPI 20MHz + Bus Arbitration)...");

    if (vspi_bus_free == NULL) {
        vspi_bus_free = xSemaphoreCreateBinary();
        xSemaphoreGive(vspi_bus_free);
    }

    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_LCD_SCK, .mosi_io_num = PIN_LCD_MOSI, .miso_io_num = PIN_LCD_MISO,
        .max_transfer_sz = LCD_H_RES * 40 * sizeof(uint16_t),
    };
    spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_LCD_DC, .cs_gpio_num = PIN_LCD_CS,
        .pclk_hz = 20 * 1000 * 1000, // Tăng lên 20MHz cho mượt mà
        .lcd_cmd_bits = 8, .lcd_param_bits = 8,
        .spi_mode = 0, .trans_queue_depth = 10, .on_color_trans_done = notify_lvgl_flush_ready,
        .user_ctx = NULL,
    };
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI3_HOST, &io_config, &io_handle);

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_LCD_RST, .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR, .bits_per_pixel = 16,
    };
    esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle);
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_swap_xy(panel_handle, true);
    esp_lcd_panel_mirror(panel_handle, true, true);
    esp_lcd_panel_disp_on_off(panel_handle, true);

    if (PIN_LCD_BLK != -1) {
        ledc_timer_config_t ledc_timer = { .speed_mode = LEDC_MODE, .timer_num = LEDC_TIMER, .duty_resolution = LEDC_DUTY_RES, .freq_hz = LEDC_FREQUENCY, .clk_cfg = LEDC_AUTO_CLK };
        ledc_timer_config(&ledc_timer);
        ledc_channel_config_t ledc_channel = { .speed_mode = LEDC_MODE, .channel = LEDC_CHANNEL, .timer_sel = LEDC_TIMER, .intr_type = LEDC_INTR_DISABLE, .gpio_num = PIN_LCD_BLK, .duty = 0, .hpoint = 0 };
        ledc_channel_config(&ledc_channel);
    }

    lv_init();
    size_t draw_buf_sz = LCD_H_RES * 40; // Tăng buffer lên 40 dòng cho sướng
    lv_color_t *buf1 = heap_caps_malloc(draw_buf_sz * sizeof(lv_color_t), MALLOC_CAP_DMA);
    lv_color_t *buf2 = heap_caps_malloc(draw_buf_sz * sizeof(lv_color_t), MALLOC_CAP_DMA);
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, draw_buf_sz);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_H_RES; disp_drv.ver_res = LCD_V_RES;
    disp_drv.flush_cb = flush_cb; disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    lv_disp_drv_register(&disp_drv);
}

void lv_port_disp_force_ready(void) {
    lv_disp_flush_ready(&disp_drv);
}
