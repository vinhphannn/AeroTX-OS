/**
 * hal_input.c - Hardware Abstraction Layer Implementation
 * ========================================================
 * Chỉ phụ thuộc vào ESP-IDF driver ADC và GPIO.
 * KHÔNG include bất kỳ logic nào từ tầng trên.
 */

#include "hal_input.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

static const char *TAG = "HAL_INPUT";

// --- MUX Pin Definitions (CD74HC4067) ---
#define MUX_S0_PIN   25  // Moved from 17 to free up UART2 TX for ELRS
#define MUX_S1_PIN   22
#define MUX_S2_PIN   33
#define MUX_S3_PIN   26
#define ADC_CHANNEL  ADC_CHANNEL_6  // GPIO34 -> ADC1_CH6

// --- MUX Channel Mapping ---
// GHI CHÚ WIRING: Đây là số kênh vật lý trên board. Không thay đổi
// trừ khi bạn hàn lại dây.
#define MUX_CH_THROTTLE    1
#define MUX_CH_YAW         0
#define MUX_CH_PITCH       3
#define MUX_CH_ROLL        2
#define MUX_CH_P1          4
#define MUX_CH_P2          5
// BUG FIX: Wiring vật lý ch6=FlightMode(gửi PX4), ch7=SysMode(FLIGHT/MENU/SIM)
// Code cũ rc_input: CH_SW_MODE=6 -> mode (PX4), CH_SW_SPEED=7 -> speed (sys)
#define MUX_CH_SW_FLIGHT   6   // 3-pos: Flight Mode gửi PX4 (Stabilize/Alt/Loiter)
#define MUX_CH_SW_MODE     7   // 3-pos: System Mode (FLIGHT/MENU/SIM)
#define MUX_CH_SW_ARM      8
#define MUX_CH_SW_BEEPER   9
#define MUX_CH_SW_LED      10
#define MUX_CH_SW_AUX      11
#define MUX_CH_VBAT_TX     13

static adc_oneshot_unit_handle_t s_adc_handle = NULL;

// Chọn kênh MUX và chuẩn bị đọc (Settle cực ngắn vì board TX01 dùng dây ngắn)
static inline void mux_select(uint8_t ch)
{
    gpio_set_level(MUX_S0_PIN, (ch >> 0) & 0x01);
    gpio_set_level(MUX_S1_PIN, (ch >> 1) & 0x01);
    gpio_set_level(MUX_S2_PIN, (ch >> 2) & 0x01);
    gpio_set_level(MUX_S3_PIN, (ch >> 3) & 0x01);
    esp_rom_delay_us(10);  // 10µs là đủ cho MUX CD74HC4067
}

// Đọc giá trị ADC thô với settle time tùy chỉnh
static uint16_t read_mux_raw(uint8_t mux_ch, uint32_t settle_us)
{
    mux_select(mux_ch);
    esp_rom_delay_us(settle_us);
    int val = 0;
    uint64_t start = esp_timer_get_time();
    adc_oneshot_read(s_adc_handle, ADC_CHANNEL, &val);
    uint64_t dur = esp_timer_get_time() - start;
    if (dur > 1000) {
        ESP_LOGW(TAG, "Slow ADC read: %lluus on CH %d", dur, mux_ch);
    }
    return (uint16_t)val;
}

// FIX NOISE: Oversample 4x
// Oversample 2x với settle time tùy chỉnh
static uint16_t read_mux_avg(uint8_t mux_ch, uint32_t settle_us)
{
    mux_select(mux_ch);
    esp_rom_delay_us(settle_us);
    int v1=0, v2=0;
    uint64_t start = esp_timer_get_time();
    adc_oneshot_read(s_adc_handle, ADC_CHANNEL, &v1);
    adc_oneshot_read(s_adc_handle, ADC_CHANNEL, &v2);
    uint64_t dur = esp_timer_get_time() - start;
    if (dur > 2000) {
        ESP_LOGW(TAG, "Slow ADC avg read: %lluus on CH %d", dur, mux_ch);
    }
    return (uint16_t)((v1 + v2) / 2);
}

// FIX NOISE: IIR Low-Pass Filter (Exponential Moving Average)
// alpha nhỏ = lọc mạnh (lag nhiều), alpha lớn = lọc ít (phản hồi nhanh)
// alpha=0.75: thời hằng siêu ngắn (lag gần như bằng 0)
#define IIR_ALPHA_SCALED  192  // alpha = 0.75
static uint16_t s_joy_filtered[7] = {2048, 2048, 2048, 2048, 2048, 2048, 2048}; // [0-3]joy, [4-5]pot, [6]vbat

// index: 0-5 joy/pot (alpha=0.25), 6 vbat (alpha=0.01 cực chậm)
static uint16_t apply_iir(uint8_t idx, uint16_t new_val)
{
    uint32_t alpha = (idx == 6) ? 3 : IIR_ALPHA_SCALED; // Pin: siêu chậm, Joy: siêu nhanh
    uint32_t filtered = (alpha * new_val + (256 - alpha) * s_joy_filtered[idx]) / 256;
    s_joy_filtered[idx] = (uint16_t)filtered;
    return s_joy_filtered[idx];
}

void HAL_Input_Init(void)
{
    // Cấu hình pin MUX Select
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << MUX_S0_PIN) | (1ULL << MUX_S1_PIN) |
                        (1ULL << MUX_S2_PIN) | (1ULL << MUX_S3_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&io_conf);

    // Khởi tạo ADC Unit 1
    adc_oneshot_unit_init_cfg_t init_cfg = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&init_cfg, &s_adc_handle);

    // Cấu hình kênh ADC: 12-bit, suy hao 12dB (0-3.3V)
    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten    = ADC_ATTEN_DB_12,
    };
    adc_oneshot_config_channel(s_adc_handle, ADC_CHANNEL, &chan_cfg);

    ESP_LOGI(TAG, "HAL Input Ready. (ADC1_CH6, MUX CD74HC4067)");
}

void HAL_Input_Read(RawInput_t *out)
{
    /* Tổng thời gian mục tiêu: < 1ms/cycle @ 250Hz
     * Mô hình: [MUX settle] + [ADC sample]
     * - joy (4ch): 20us settle + 2x ADC  ~ 4 × (20+100us)  = 480us
     * - pot (2ch): 50us settle + 2x ADC  ~ 2 × (50+100us)  = 300us
     * - sw  (6ch): 20us settle + 1x ADC  ~ 6 × (20+ 50us)  = 420us (switch là digital)
     * - vbat(1ch): 50us settle + 2x ADC  ~ 1 × (50+100us)  = 150us
     * Tổng ~1.35ms (IIR filter bù nhiễu thay thế settle dài) */

    // 1. Joystick: Settle 20us đủ vì trở kháng joystick thấp
    out->joy[0] = apply_iir(0, read_mux_avg(MUX_CH_THROTTLE, 20));
    out->joy[1] = apply_iir(1, read_mux_avg(MUX_CH_YAW,      20));
    out->joy[2] = apply_iir(2, read_mux_avg(MUX_CH_PITCH,    20));
    out->joy[3] = apply_iir(3, read_mux_avg(MUX_CH_ROLL,     20));

    // 2. Biến trở: Settle 50us (giảm từ 100us, IIR bù nhiễu)
    out->pot[0] = apply_iir(4, read_mux_avg(MUX_CH_P1, 50));
    out->pot[1] = apply_iir(5, read_mux_avg(MUX_CH_P2, 50));

    // 3. Công tắc: Chỉ cần 1 lần đọc (digital signal, không cần avg)
    //    Settle 20us: switch có trở kháng thấp, tín hiệu sạch
    out->sw_mode   = read_mux_raw(MUX_CH_SW_MODE,   20);
    out->sw_flight = read_mux_raw(MUX_CH_SW_FLIGHT,  20);
    out->sw_arm    = read_mux_raw(MUX_CH_SW_ARM,     20);
    out->sw_beeper = read_mux_raw(MUX_CH_SW_BEEPER,  20);
    out->sw_led    = read_mux_raw(MUX_CH_SW_LED,     20);
    out->sw_aux    = read_mux_raw(MUX_CH_SW_AUX,     20);

    // 4. Pin TX: Settle 200us để điện áp MUX13 (vbat) xả hết trước chu kỳ kế tiếp
    //    Tránh bleed sang joy[0] (MUX1) ở đầu chu kỳ sau → gây ch5↔ch14 correlation
    out->vbat_raw  = apply_iir(6, read_mux_avg(MUX_CH_VBAT_TX, 200));
}

// ═══════════════════════════════════════════════════════════════════
// DEBUG: Quét toàn bộ 14 kênh MUX theo thứ tự vật lý
// Gọi mỗi 1 giây để xác nhận wiring TRƯỚC khi phát triển sâu.
// ═══════════════════════════════════════════════════════════════════

// Tên hiển thị cho từng kênh MUX vật lý (ch0 → ch13)
static const char* MUX_LABEL[16] = {
    /* 0 */ "YAW      (joy)",
    /* 1 */ "THROTTLE (joy)",
    /* 2 */ "ROLL     (joy)",
    /* 3 */ "PITCH    (joy)",
    /* 4 */ "POT1     (pot)",
    /* 5 */ "POT2     (pot)",
    /* 6 */ "SW_FLIGHT(3way)",
    /* 7 */ "SW_MODE  (3way)",
    /* 8 */ "SW_ARM   (2way)",
    /* 9 */ "SW_BEEPER(2way)",
    /*10 */ "SW_LED   (2way)",
    /*11 */ "SW_AUX   (2way)",
    /*12 */ "UNUSED   (---)",
    /*13 */ "VBAT_TX  (volt)",
};

void HAL_Input_Debug_Scan(void)
{
    ESP_LOGI("MUX_SCAN", "═══ RAW MUX SCAN (ch0→ch13) ═══");
    for (int ch = 0; ch <= 13; ch++) {
        // Settle 100µs cho tất cả kênh, đủ để ổn định
        int raw = 0;
        mux_select(ch);
        esp_rom_delay_us(100);
        adc_oneshot_read(s_adc_handle, ADC_CHANNEL, &raw);

        // Decode ý nghĩa: joy/pot là analog, switch là digital
        const char *decode = "";
        char decode_buf[24] = "";
        if (ch <= 5) {
            // Analog: in giá trị thô
            snprintf(decode_buf, sizeof(decode_buf), "analog=%4d", raw);
            decode = decode_buf;
        } else if (ch == 6 || ch == 7) {
            // 3-way switch
            uint8_t pos = (raw < 1300) ? 0 : (raw < 2700) ? 1 : 2;
            snprintf(decode_buf, sizeof(decode_buf), "3-way=POS%d  (raw=%4d)", pos, raw);
            decode = decode_buf;
        } else if (ch >= 8 && ch <= 11) {
            // 2-way switch: < 2000 = ON
            snprintf(decode_buf, sizeof(decode_buf), "2-way=%s (raw=%4d)",
                     raw < 2000 ? "ON " : "OFF", raw);
            decode = decode_buf;
        } else if (ch == 13) {
            // VBAT
            float volt = (float)raw * (3.3f / 4095.0f) * 3.57f;
            snprintf(decode_buf, sizeof(decode_buf), "volt=%.2fV (raw=%4d)", volt, raw);
            decode = decode_buf;
        } else {
            snprintf(decode_buf, sizeof(decode_buf), "raw=%4d", raw);
            decode = decode_buf;
        }

        ESP_LOGI("MUX_SCAN", "  ch%02d │ %-18s │ %s",
                 ch, MUX_LABEL[ch], decode);
    }
    ESP_LOGI("MUX_SCAN", "═══════════════════════════════════");
}
