#include "lv_port_indev.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define PIN_TOUCH_CS    32
#define PIN_LCD_MISO    19
#define PIN_LCD_MOSI    23
#define PIN_LCD_SCK     18

static const char *TAG = "TOUCH";
static spi_device_handle_t touch_spi;
bool touch_is_pressed = false;

extern SemaphoreHandle_t vspi_bus_free; // Lấy từ lv_port_disp.c

static void touch_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static bool touch_get_xy(int16_t *x, int16_t *y);

void lv_port_indev_init(void) {
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 2 * 1000 * 1000, // 2MHz cho cảm ứng
        .mode = 0,
        .spics_io_num = PIN_TOUCH_CS,
        .queue_size = 7,
    };

    // Phải dùng chung SPI3_HOST với LCD
    esp_err_t ret = spi_bus_add_device(SPI3_HOST, &devcfg, &touch_spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add touch device");
        return;
    }

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read;
    lv_indev_drv_register(&indev_drv);
}

static bool s_touch_silent = false;
void lv_port_indev_set_silent(bool silent) { s_touch_silent = silent; }

static void touch_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data) {
    if (s_touch_silent) {
        data->state = LV_INDEV_STATE_REL;
        touch_is_pressed = false;
        return;
    }

    int16_t x, y;
    bool success = false;

    // TRANH CHẤP BUS: Chỉ đọc cảm ứng khi Bus đang rảnh
    if (xSemaphoreTake(vspi_bus_free, pdMS_TO_TICKS(10)) == pdTRUE) {
        success = touch_get_xy(&x, &y);
        xSemaphoreGive(vspi_bus_free);
    }

    if (success) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = x;
        data->point.y = y;
        touch_is_pressed = true;
    } else {
        data->state = LV_INDEV_STATE_REL;
        touch_is_pressed = false;
    }
}

static uint16_t tp_read_raw(uint8_t cmd) {
    uint8_t tx_data[3] = {cmd, 0x00, 0x00};
    uint8_t rx_data[3] = {0};
    spi_transaction_t t = {
        .length = 24, .tx_buffer = tx_data, .rx_buffer = rx_data,
    };
    spi_device_polling_transmit(touch_spi, &t);
    return ((uint16_t)rx_data[1] << 8 | rx_data[2]) >> 3;
}

static bool touch_get_xy(int16_t *x, int16_t *y) {
    uint16_t raw_x = tp_read_raw(0xD0);
    uint16_t raw_y = tp_read_raw(0x90);

    if (raw_x < 150 || raw_x > 4000 || raw_y < 150 || raw_y > 4000) return false;

    // Hiệu chuẩn cho màn hình 320x240 lật ngang (V2.0 FINAL Alignment)
    int16_t cal_x = (raw_y - 350) * 320 / (3950 - 350);
    int16_t cal_y = (raw_x - 200) * 240 / (3900 - 200);

    *x = cal_x;
    *y = 239 - cal_y;

    if (*x < 0) *x = 0;
    if (*x >= 320) *x = 319;
    if (*y < 0) *y = 0;
    if (*y >= 240) *y = 239;

    return true;
}
