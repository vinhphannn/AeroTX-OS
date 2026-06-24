/**
 * input_filter.c - Unified Input Filter Layer (Tầng 2)
 * ======================================================
 * Nhận dữ liệu thô từ HAL (RawInput_t) và áp dụng bộ lọc 2 giai đoạn:
 *
 * JOYSTICK / POT (tín hiệu analog):
 *   Giai đoạn 1 (trong HAL): Alpha-Trimmed-5 + Adaptive IIR (deadband=12 ADC)
 *   Giai đoạn 2 (tại đây)  : Median-3 + Adaptive Deadband
 *                             - Tĩnh (|diff| < THRESH): ±15 ADC → chặn rung tuyệt đối
 *                             - Di chuyển chậm        : ±5 ADC  → mượt
 *                             - Di chuyển nhanh       : pass-through (0 delay)
 *
 * SWITCH (tín hiệu digital từ ADC):
 *   Debounce 3 mẫu liên tiếp nhất quán → tránh bounce khi bật/tắt công tắc
 *
 * VBAT (pin TX):
 *   Sliding Window trung bình 8 mẫu → voltage + % pin không nhảy số
 */

#include "input_filter.h"
#include "hal_input.h"
#include <stdlib.h>
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// Cấu hình bộ lọc
// ─────────────────────────────────────────────────────────────────────────────

// Deadband cho joystick (ADC units, sau khi đã qua IIR ở HAL)
// 15 ADC: noise floor thực tế của ESP32 ADC1 sau 2 tầng lọc
#define JOY_DEADBAND_STATIC    15   // Khi joy đứng yên: chặn hoàn toàn
#define JOY_DEADBAND_MOVING    5    // Khi joy đang di chuyển: lọc nhẹ
#define JOY_FAST_THRESH        80   // |diff| > 80 ADC: pass-through (nhanh)

// Deadband cho biến trở (nhạy hơn joy, không cần chặt bằng)
#define POT_DEADBAND           10

// Ngưỡng phân biệt công tắc 3 nấc (ADC raw)
#define SW3_LOW_THRESH         1300
#define SW3_HIGH_THRESH        2700

// Ngưỡng công tắc 2 nấc (ADC raw): < 2000 = ON
#define SW2_ON_THRESH          2000

// Số lần debounce liên tiếp để xác nhận thay đổi switch
#define SW_DEBOUNCE_COUNT      3

// Sliding window cho VBAT
#define VBAT_WINDOW            8

// Hệ số tính voltage (điều chỉnh theo multimeter thực tế)
#define VBAT_SCALE_FACTOR      3.57f

// Range pin 2S LiPo (V)
#define VBAT_MIN_V             7.0f
#define VBAT_MAX_V             8.4f

// ─────────────────────────────────────────────────────────────────────────────
// State nội bộ
// ─────────────────────────────────────────────────────────────────────────────

// Median-3: chỉ cần 3 mẫu → nhanh hơn Median-5 cũ
typedef struct {
    uint16_t buf[3];
    uint8_t  idx;
    bool     ready;  // true sau khi có đủ 3 mẫu
} Median3_t;

static Median3_t  s_joy_med[4];    // median buffer cho 4 trục joy
static Median3_t  s_pot_med[2];    // median buffer cho 2 biến trở
static uint16_t   s_joy_stable[4]; // giá trị ổn định sau deadband
static uint16_t   s_pot_stable[2];
static bool       s_init_done = false;

// Debounce switch 2 nấc
typedef struct {
    bool    confirmed;   // giá trị đã debounce
    bool    pending;     // giá trị đang thử debounce
    uint8_t count;
} Debounce2_t;

// Debounce switch 3 nấc
typedef struct {
    uint8_t confirmed;
    uint8_t pending;
    uint8_t count;
} Debounce3_t;

static Debounce2_t s_db_arm, s_db_beeper, s_db_led, s_db_aux;
static Debounce3_t s_db_flight, s_db_mode;

// Sliding window VBAT
static uint16_t s_vbat_win[VBAT_WINDOW];
static uint8_t  s_vbat_idx = 0;
static uint32_t s_vbat_sum = 0;
static bool     s_vbat_ready = false;

// Output
static FilteredInput_t s_filtered;

// ─────────────────────────────────────────────────────────────────────────────
// Helper: Median của 3 phần tử (sort nhanh, không dùng bubble sort)
// ─────────────────────────────────────────────────────────────────────────────
static inline uint16_t median3(uint16_t a, uint16_t b, uint16_t c)
{
    // Tìm median 3 phần tử không cần sort toàn bộ: max(min(a,b), min(max(a,b),c))
    uint16_t lo = (a < b) ? a : b;
    uint16_t hi = (a < b) ? b : a;
    // median = max(lo, min(hi, c))
    uint16_t mid_hi = (hi < c) ? hi : c;
    return (lo > mid_hi) ? lo : mid_hi;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: Lọc một kênh analog (Median-3 + Adaptive Deadband)
// ─────────────────────────────────────────────────────────────────────────────
static uint16_t filter_analog(Median3_t *m, uint16_t *stable,
                               uint16_t new_raw,
                               uint16_t deadband_static,
                               uint16_t deadband_moving,
                               uint16_t fast_thresh)
{
    // 1. Cập nhật vòng đệm Median-3
    m->buf[m->idx] = new_raw;
    m->idx = (m->idx + 1) % 3;
    if (!m->ready && m->idx == 0) m->ready = true;

    // Nếu chưa đủ 3 mẫu, trả về mẫu hiện tại
    if (!m->ready) {
        *stable = new_raw;
        return new_raw;
    }

    uint16_t med = median3(m->buf[0], m->buf[1], m->buf[2]);

    // 2. Adaptive Deadband
    int16_t diff = (int16_t)med - (int16_t)*stable;
    uint16_t abs_diff = (diff < 0) ? (uint16_t)(-diff) : (uint16_t)diff;

    if (abs_diff >= fast_thresh) {
        // Di chuyển nhanh: pass-through ngay, không delay
        *stable = med;
    } else if (abs_diff > deadband_static) {
        // Di chuyển chậm/vừa: áp deadband moving nhỏ hơn
        if (abs_diff > deadband_moving) {
            *stable = med;
        }
        // else: deadband tĩnh không thỏa → giữ nguyên
    }
    // else: trong vùng deadband tĩnh → giữ nguyên (chặn jitter)

    return *stable;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: Debounce switch 2 nấc
// ─────────────────────────────────────────────────────────────────────────────
static bool debounce2(Debounce2_t *db, bool new_val)
{
    if (new_val == db->pending) {
        if (db->count < SW_DEBOUNCE_COUNT) {
            db->count++;
        }
        if (db->count >= SW_DEBOUNCE_COUNT) {
            db->confirmed = db->pending;
        }
    } else {
        // Giá trị thay đổi → reset đếm
        db->pending = new_val;
        db->count   = 1;
    }
    return db->confirmed;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: Debounce switch 3 nấc
// ─────────────────────────────────────────────────────────────────────────────
static uint8_t debounce3(Debounce3_t *db, uint8_t new_val)
{
    if (new_val == db->pending) {
        if (db->count < SW_DEBOUNCE_COUNT) {
            db->count++;
        }
        if (db->count >= SW_DEBOUNCE_COUNT) {
            db->confirmed = db->pending;
        }
    } else {
        db->pending = new_val;
        db->count   = 1;
    }
    return db->confirmed;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: Decode ADC raw → vị trí switch
// ─────────────────────────────────────────────────────────────────────────────
static inline bool decode_sw2(uint16_t raw)  { return (raw < SW2_ON_THRESH); }
static inline uint8_t decode_sw3(uint16_t raw)
{
    if (raw < SW3_LOW_THRESH)  return 0;
    if (raw < SW3_HIGH_THRESH) return 1;
    return 2;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void input_filter_init(void)
{
    memset(&s_filtered,  0, sizeof(s_filtered));
    memset(s_joy_med,    0, sizeof(s_joy_med));
    memset(s_pot_med,    0, sizeof(s_pot_med));
    memset(s_joy_stable, 0, sizeof(s_joy_stable));
    memset(s_pot_stable, 0, sizeof(s_pot_stable));
    memset(&s_db_arm,    0, sizeof(s_db_arm));
    memset(&s_db_beeper, 0, sizeof(s_db_beeper));
    memset(&s_db_led,    0, sizeof(s_db_led));
    memset(&s_db_aux,    0, sizeof(s_db_aux));
    memset(&s_db_flight, 0, sizeof(s_db_flight));
    memset(&s_db_mode,   0, sizeof(s_db_mode));
    memset(s_vbat_win,   0, sizeof(s_vbat_win));
    s_vbat_sum   = 0;
    s_vbat_idx   = 0;
    s_vbat_ready = false;
    s_init_done  = true;
}

void input_filter_update(const RawInput_t *raw)
{
    if (!s_init_done) input_filter_init();

    // ── 1. Joystick: Median-3 + Adaptive Deadband ────────────────────────────
    for (int i = 0; i < 4; i++) {
        s_filtered.joy[i] = filter_analog(
            &s_joy_med[i], &s_joy_stable[i],
            raw->joy[i],
            JOY_DEADBAND_STATIC,
            JOY_DEADBAND_MOVING,
            JOY_FAST_THRESH
        );
    }

    // ── 2. Biến trở: Median-3 + Deadband ±10 ────────────────────────────────
    for (int i = 0; i < 2; i++) {
        s_filtered.pot[i] = filter_analog(
            &s_pot_med[i], &s_pot_stable[i],
            raw->pot[i],
            POT_DEADBAND, POT_DEADBAND / 2, JOY_FAST_THRESH
        );
    }

    // ── 3. Công tắc: Decode + Debounce ──────────────────────────────────────
    s_filtered.sw_arm     = debounce2(&s_db_arm,     decode_sw2(raw->sw_arm));
    s_filtered.sw_beeper  = debounce2(&s_db_beeper,  decode_sw2(raw->sw_beeper));
    s_filtered.sw_led     = debounce2(&s_db_led,     decode_sw2(raw->sw_led));
    s_filtered.sw_aux     = debounce2(&s_db_aux,     decode_sw2(raw->sw_aux));
    s_filtered.sw_flight  = debounce3(&s_db_flight,  decode_sw3(raw->sw_flight));
    s_filtered.sw_mode    = debounce3(&s_db_mode,    decode_sw3(raw->sw_mode));

    // ── 4. VBAT: Sliding Window 8 mẫu → voltage + % pin ────────────────────
    // Cập nhật sliding window
    s_vbat_sum -= s_vbat_win[s_vbat_idx];
    s_vbat_win[s_vbat_idx] = raw->vbat_raw;
    s_vbat_sum += raw->vbat_raw;
    s_vbat_idx = (s_vbat_idx + 1) % VBAT_WINDOW;
    if (!s_vbat_ready && s_vbat_idx == 0) s_vbat_ready = true;

    // Lấy trung bình (nếu chưa đủ 8 mẫu thì dùng giá trị hiện tại)
    uint32_t vbat_avg = s_vbat_ready
        ? (s_vbat_sum / VBAT_WINDOW)
        : raw->vbat_raw;

    s_filtered.vbat_raw = (uint16_t)vbat_avg;

    // Tính voltage và %
    float v = (float)vbat_avg * (3.3f / 4095.0f) * VBAT_SCALE_FACTOR;
    if (v < 0.5f) v = 0.0f;   // Không cắm pin
    if (v > 15.0f) v = 15.0f; // Clamp bảo vệ
    s_filtered.vbat_v = v;

    // % pin 2S LiPo: 7.0V = 0%, 8.4V = 100%
    float pct = (v - VBAT_MIN_V) / (VBAT_MAX_V - VBAT_MIN_V) * 100.0f;
    if (pct < 0.0f)   pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    s_filtered.vbat_pct = (uint8_t)pct;
}

const FilteredInput_t *input_filter_get(void)
{
    return &s_filtered;
}
