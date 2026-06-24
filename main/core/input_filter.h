#pragma once
/**
 * input_filter.h - Unified Input Filter Layer (Tầng 2)
 * =====================================================
 * Nhận RawInput_t từ HAL → lọc → cung cấp FilteredInput_t cho tất cả consumer.
 *
 * Kiến trúc 3 tầng:
 *   [HAL: hal_input.c]  →  [Filter: input_filter.c]  →  [Consumer: RC / BLE / UI]
 *
 * Bộ lọc:
 *   Joystick : Median-3 + Adaptive Deadband (±15 ADC tĩnh, ±5 khi di chuyển)
 *   Potmeter : Median-3 + Deadband ±10
 *   Switches : Debounce 3 lần liên tiếp (chống contact bounce)
 *   VBAT     : Sliding Window 8 mẫu + clamp → voltage & % pin
 */

#include <stdint.h>
#include <stdbool.h>
#include "hal_input.h"

/**
 * FilteredInput_t - Dữ liệu đã lọc sạch, sẵn sàng cho mọi consumer.
 *
 * joy[], pot[]: đơn vị ADC 0-4095 (đã lọc nhiễu, không còn jitter tĩnh).
 * Tầng mixer (sensor_task) sẽ dùng calib+expo chuyển sang µs 1000-2000.
 * Tầng BLE dùng trực tiếp (map_axis 0→65535).
 * Tầng UI dùng để vẽ bar + vbat.
 */
typedef struct {
    // Joystick 4 trục: 0-4095 filtered
    // [0]=Throttle, [1]=Yaw, [2]=Pitch, [3]=Roll
    uint16_t joy[4];

    // Biến trở 2 kênh: 0-4095 filtered
    // [0]=P1, [1]=P2
    uint16_t pot[2];

    // Công tắc 2 nấc (debounced)
    bool     sw_arm;
    bool     sw_beeper;
    bool     sw_led;
    bool     sw_aux;

    // Công tắc 3 nấc (debounced): 0 / 1 / 2
    uint8_t  sw_flight;  // Flight mode (gửi PX4)
    uint8_t  sw_mode;    // System mode (FLIGHT/MENU/SIM)

    // Pin TX: đã lọc nặng + tính sẵn
    uint16_t vbat_raw;   // ADC trung bình sliding window
    float    vbat_v;     // Voltage thực (V)
    uint8_t  vbat_pct;   // % pin (0-100, không nhảy số)
} FilteredInput_t;

/**
 * input_filter_init() - Khởi tạo state bộ lọc. Gọi 1 lần khi boot.
 */
void input_filter_init(void);

/**
 * input_filter_update() - Cập nhật bộ lọc với mẫu mới từ HAL.
 * Gọi bởi sensor_task mỗi 4ms (250Hz).
 */
void input_filter_update(const RawInput_t *raw);

/**
 * input_filter_get() - Trả về con trỏ đến dữ liệu đã lọc.
 * Thread-safe ở mức đọc (không cần mutex vì là static, cập nhật atomic từ 1 task).
 */
const FilteredInput_t *input_filter_get(void);
