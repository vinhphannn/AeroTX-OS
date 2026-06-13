#pragma once
/**
 * hal_input.h - Hardware Abstraction Layer for RC Input
 * =====================================================
 * Tầng này CHỈ đọc phần cứng (ADC, GPIO).
 * Không có logic xử lý, không có Calibration, không có Expo.
 *
 * Nguyên tắc: Nếu bạn thay đổi phần cứng (VD: dùng I2C ADC thay
 * vì MUX), bạn chỉ cần chỉnh file này. Phần còn lại không đổi.
 */

#include <stdint.h>
#include <stdbool.h>

/**
 * RawInput_t - Dữ liệu thô từ phần cứng, CHƯA qua xử lý nào.
 * - joy[]: Giá trị ADC 12-bit (0-4095) từ 4 joystick
 * - pot[]: Giá trị ADC 12-bit từ 2 biến trở phụ
 * - sw_*:  Trạng thái đọc từ ADC của các công tắc vật lý
 * - vbat_raw: ADC raw của mạch đo pin TX
 */
typedef struct {
    uint16_t joy[4];    // [0]=Throttle, [1]=Yaw, [2]=Pitch, [3]=Roll  (ADC 0-4095)
    uint16_t pot[2];    // [0]=P1, [1]=P2  (ADC 0-4095)

    // Switch đọc dưới dạng ADC raw để tầng trên quyết định ngưỡng
    uint16_t sw_arm;
    uint16_t sw_mode;    // 3-pos: FLIGHT/MENU/SIM
    uint16_t sw_flight;  // 3-pos: Flight Mode (gửi PX4)
    uint16_t sw_beeper;
    uint16_t sw_led;
    uint16_t sw_aux;

    uint16_t vbat_raw;   // ADC raw của pin TX (qua voltage divider)
} RawInput_t;

/**
 * HAL_Input_Init() - Khởi tạo ADC và GPIO của MUX một lần duy nhất khi boot.
 * Gọi từ main.c, TRƯỚC khi tạo task.
 */
void HAL_Input_Init(void);
void HAL_Input_Read(RawInput_t *out);

/**
 * HAL_Input_Debug_Scan() - Quét toàn bộ 14 kênh MUX (ch0-13) theo thứ tự vật lý.
 * In raw ADC qua ESP_LOGI. Gọi định kỳ để xác nhận wiring.
 * KHÔNG gọi trong production, chỉ dùng khi debug.
 */
void HAL_Input_Debug_Scan(void);
