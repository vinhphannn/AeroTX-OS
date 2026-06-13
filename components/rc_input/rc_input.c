#include "rc_input.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "RC_INPUT";
static adc_oneshot_unit_handle_t adc1_handle;

static void mux_select(uint8_t ch) {
    gpio_set_level(MUX_S0_PIN, (ch >> 0) & 0x01);
    gpio_set_level(MUX_S1_PIN, (ch >> 1) & 0x01);
    gpio_set_level(MUX_S2_PIN, (ch >> 2) & 0x01);
    gpio_set_level(MUX_S3_PIN, (ch >> 3) & 0x01);
    esp_rom_delay_us(5); // [v266] 5µs đủ cho MUX settle
}

static int read_mux_raw(uint8_t ch) {
    mux_select(ch);
    int v1, v2;
    adc_oneshot_read(adc1_handle, ADC_CHANNEL_6, &v1);
    adc_oneshot_read(adc1_handle, ADC_CHANNEL_6, &v2);
    return (v1 + v2) / 2; // [v266] Multisampling 2 lần để giảm noise/spike
}

void RC_Input_Init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << MUX_S0_PIN) | (1ULL << MUX_S1_PIN) |
                        (1ULL << MUX_S2_PIN) | (1ULL << MUX_S3_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);

    adc_oneshot_unit_init_cfg_t init_config1 = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&init_config1, &adc1_handle);
    adc_oneshot_chan_cfg_t config = { .bitwidth = ADC_BITWIDTH_12, .atten = ADC_ATTEN_DB_12 };
    adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_6, &config);
    ESP_LOGI(TAG, "RC Input V4.0 (Raw Mode) Ready.");
}

void RC_Input_Calibrate(void) {
    // Để trống: Calib sẽ được xử lý ở tầng Input Engine và lưu NVS
    ESP_LOGI(TAG, "Calib Triggered (Legacy Call)");
}

static uint8_t read_3way(int raw) {
    if (raw < 1300) return 0;
    if (raw < 2600) return 1;
    return 2;
}

uint16_t RC_Map(int raw, int in_min, int in_max) {
    if (in_max == in_min) return 1500;
    return 1000 + ((raw - in_min) * 1000) / (in_max - in_min);
}

uint8_t RC_Read3WaySwitch(int adc_raw) {
    return read_3way(adc_raw);
}

void RC_Input_Read(RC_State_t *out) {
    // 1. Joysticks (Raw ADC 0-4095)
    out->throttle = read_mux_raw(CH_THROTTLE);
    out->yaw      = read_mux_raw(CH_YAW);
    taskYIELD(); // [v266] Cho IDLE task feed watchdog

    out->pitch    = read_mux_raw(CH_PITCH);
    out->roll     = read_mux_raw(CH_ROLL);
    taskYIELD();

    // 2. Aux Pots
    out->p1 = read_mux_raw(CH_P1);
    out->p2 = read_mux_raw(CH_P2);
    taskYIELD();

    // 3. 3-Way Switches
    out->mode  = read_3way(read_mux_raw(CH_SW_MODE));
    out->speed = read_3way(read_mux_raw(CH_SW_SPEED));

    // 4. 2-Way Switches
    out->armed  = (read_mux_raw(CH_SW_ARM) < 2000);
    out->beeper = (read_mux_raw(CH_SW_BEEPER) < 2000);
    out->led    = (read_mux_raw(CH_SW_LED) < 2000);
    out->aux    = (read_mux_raw(CH_SW_AUX) < 2000);
    taskYIELD();

    // 5. System Sensors
    out->vbat_tx = read_mux_raw(CH_VBAT_TX) * (3.3f / 4095.0f) * 3.48f;
}
