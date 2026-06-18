/**
 * sensor_task.c - Input Processing Pipeline
 * ==========================================
 * Task này chạy ở 250Hz trên Core 0.
 *
 * Pipeline rõ ràng 3 bước:
 *   [1] HAL_Input_Read()        -> đọc phần cứng, lấy RawInput_t
 *   [2] mixer_process_frame()   -> Calib + Expo + Trim = channels[]
 *   [3] sys_mutex update        -> ghi kết quả vào g_state
 *
 * sensor_task KHÔNG gửi Queue nữa. radio_task tự đọc g_state qua mutex.
 */

#include "core/sys_state.h"
#include "core/sys_config.h"
#include "core/hal_input.h"
#include "core/timing_test.h"   // <<< TEST: đo thời gian thực thi
#include "esp_log.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "SENSOR_TASK";

// -----------------------------------------------------------------------
// MIXER - Tầng xử lý tín hiệu (Calibration + Expo + Trim + Reverse)
// Đây là hàm module độc lập, không phụ thuộc vào task hay global state.
// Dễ test riêng lẻ, dễ thay thế thuật toán sau này.
// -----------------------------------------------------------------------

// Ngưỡng phân biệt công tắc 3 nấc từ ADC raw (0-4095)
// Chia đều 3 vùng: 0-1365 = nấc 0, 1365-2730 = nấc 1, >2730 = nấc 2
#define SW3WAY_LOW_THRESH   1300
#define SW3WAY_HIGH_THRESH  2700

static inline uint8_t decode_3way(uint16_t raw)
{
    if (raw < SW3WAY_LOW_THRESH)  return 0;
    if (raw < SW3WAY_HIGH_THRESH) return 1;
    return 2;
}

// Công tắc 2 nấc: < 2000 = ON, >= 2000 = OFF (tụ pull-down)
static inline bool decode_2way(uint16_t raw)
{
    return (raw < 2000);
}

// Áp dụng Exponential Curve lên một trục (không áp dụng cho Throttle)
static uint16_t apply_expo(uint16_t val_us, uint8_t expo_pct)
{
    if (expo_pct == 0) return val_us;
    float in   = (val_us - 1500) / 500.0f;    // Normalize về [-1, 1]
    float expo = expo_pct / 100.0f;
    // Công thức expo chuẩn: out = expo*in^3 + (1-expo)*in
    float out  = (in * in * in) * expo + in * (1.0f - expo);
    int result = 1500 + (int)(out * 500.0f);
    if (result < 1000) result = 1000;
    if (result > 2000) result = 2000;
    return (uint16_t)result;
}

/**
 * mixer_process_frame() - Biến đổi RawInput_t thành 12 RC channels (1000-2000µs)
 *
 * @param raw    Dữ liệu thô từ HAL
 * @param model  Model config hiện tại (Expo, Trim, Rates, Reverse)
 * @param cal    Calibration data (Min/Max/Center/Deadband cho từng trục)
 * @param state  Con trỏ đến g_state để ghi kết quả (channels + meta)
 */
static void mixer_process_frame(const RawInput_t *raw,
                                const ModelConfig_t *model,
                                const CalibData_t *cal,
                                SystemState_t *state)
{
    // ----------------------------------------
    // BƯỚC 1: Xử lý 4 trục Joystick (CH1-CH4)
    // joy[0]=Throttle, joy[1]=Yaw, joy[2]=Pitch, joy[3]=Roll
    // ----------------------------------------
    for (int i = 0; i < 4; i++) {
        uint16_t val;
        uint16_t adc = raw->joy[i];

        if (cal->valid) {
            if (i == 0) {
                // THROTTLE: Không có vùng center, tuyến tính Min-Max
                if (adc <= cal->min[i])      val = 1000;
                else if (adc >= cal->max[i]) val = 2000;
                else {
                    val = 1000 + (uint32_t)(adc - cal->min[i]) * 1000
                          / (cal->max[i] - cal->min[i]);
                }
                // Giữ chân ga ở 1000 nếu nằm trong vùng nhiễu thấp nhất
                if (val < 1000 + cal->deadband[i]) val = 1000;
            } else {
                // ROLL/PITCH/YAW: Có vùng center, tuyến tính 2 phía
                if (adc < cal->center[i]) {
                    if (adc <= cal->min[i]) val = 1000;
                    else val = 1000 + (uint32_t)(adc - cal->min[i]) * 500
                               / (cal->center[i] - cal->min[i]);
                } else {
                    if (adc >= cal->max[i]) val = 2000;
                    else val = 1500 + (uint32_t)(adc - cal->center[i]) * 500
                               / (cal->max[i] - cal->center[i]);
                }
                // Deadband động tại tâm
                if ((int)val > 1500 - (int)cal->deadband[i] &&
                    (int)val < 1500 + (int)cal->deadband[i]) val = 1500;
            }
        } else {
            // Fallback an toàn khi chưa calibrate
            if (i == 0) val = 1000 + (uint32_t)(adc > 400 ? adc - 400 : 0) * 1000 / 3200;
            else {
                val = 1000 + (uint32_t)(adc > 500 ? adc - 500 : 0) * 1000 / 3000;
                if (abs((int)val - 1500) < 30) val = 1500;
            }
        }

        // Clamp cứng
        if (val < 1000) val = 1000;
        if (val > 2000) val = 2000;

        // Áp dụng Expo (không áp dụng cho Throttle)
        if (i != 0) val = apply_expo(val, model->expo[i]);

        // Áp dụng Trim (không áp dụng cho Throttle)
        if (i != 0) {
            int trimmed = (int)val + model->trims[i];
            if (trimmed < 1000) trimmed = 1000;
            if (trimmed > 2000) trimmed = 2000;
            val = (uint16_t)trimmed;
        }

        // Đảo chiều nếu cần
        if (model->reversed[i]) val = 3000 - val;

        state->channels[i] = val;
    }

    // ═══════════════════════════════════════════════════════════════
    // BƯỚC 2: Công tắc + Biến trở → Channel mapping
    // ═══════════════════════════════════════════════════════════════
    //  Ch5  (idx 4): SW 2-nấc #1 — ARM
    //  Ch6  (idx 5): SW 2-nấc #2 — Beeper
    //  Ch7  (idx 6): SW 2-nấc #3 — LED
    //  Ch8  (idx 7): SW 2-nấc #4 — AUX
    //  Ch9  (idx 8): SW 3-nấc   — Flight Mode (Stabilize/AltHold/Loiter)
    //  Ch10 (idx 9): SW 3-nấc   — System Mode local (Flight/Menu/Sim)
    //  Ch11 (idx10): Biến trở P1
    //  Ch12 (idx11): Biến trở P2
    // ═══════════════════════════════════════════════════════════════

    // ── 4 Công tắc 2 nấc ────────────────────────────────────────
    bool armed = decode_2way(raw->sw_arm);
    state->is_armed    = armed;
    state->channels[4] = armed ? 2000 : 1000;                        // Ch5: ARM
    state->channels[5] = decode_2way(raw->sw_beeper) ? 2000 : 1000;  // Ch6: Beeper
    state->channels[6] = decode_2way(raw->sw_led)    ? 2000 : 1000;  // Ch7: LED
    state->channels[7] = decode_2way(raw->sw_aux)    ? 2000 : 1000;  // Ch8: AUX

    // ── 2 Công tắc 3 nấc ─────────────────────────────────────────
    uint8_t flight_mode = decode_3way(raw->sw_flight);
    state->channels[8] = 1000 + (uint16_t)(flight_mode * 500);        // Ch9: Flight Mode

    uint8_t sys_mode = decode_3way(raw->sw_mode);
    state->sys_mode    = sys_mode;
    state->channels[9] = 1000 + (uint16_t)(sys_mode * 500);           // Ch10: Sys Mode

    // ── 2 Biến trở xoay ──────────────────────────────────────────
    state->channels[10] = 1000 + (uint32_t)raw->pot[0] * 1000 / 4095; // Ch11: P1
    state->channels[11] = 1000 + (uint32_t)raw->pot[1] * 1000 / 4095; // Ch12: P2



    // ----------------------------------------
    // BƯỚC 4: Meta-data hệ thống
    // ----------------------------------------
    // Điện áp TX: ADC raw → Volt thực.
    // Tỉ lệ 3.48f phụ thuộc vào cầu phân áp thực tế (VD: 2.2k + 1k).
    // NẾU HIỂN THỊ SAI: Hãy đo vbat bằng đồng hồ và chỉnh lại số 3.48f này.
    // Tỉ lệ 3.57f đã được hiệu chỉnh theo multimeter của người dùng (6.98V)
    state->tx_vbat = (float)raw->vbat_raw * (3.3f / 4095.0f) * 3.57f;
    if (state->tx_vbat < 0.5f) state->tx_vbat = 0.0f;
    if (state->tx_vbat > 15.0f) state->tx_vbat = 15.0f;

    // Tính % pin (Giả định dùng pin 2S LiPo: 7.0V là 0%, 8.4V là 100%)
    float pct = (state->tx_vbat - 7.0f) / (8.4f - 7.0f) * 100.0f;
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;
    state->tx_vbat_pct = (uint8_t)pct;

    // Lưu raw ADC để màn hình Calibration Wizard dùng
    for (int i = 0; i < 4; i++) state->raw_joy[i] = raw->joy[i];
}

// -----------------------------------------------------------------------
// TASK ENTRY POINT
// -----------------------------------------------------------------------
void sensor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Sensor Task V5.0 (HAL + Mixer Pipeline) Started");

    // Đăng ký các điểm đo thời gian (budget thực tế của từng bước)
    int id_hal   = TIMING_REGISTER("hal_read",  2500); // Budget: 2.5ms
    int id_mixer = TIMING_REGISTER("mixer",      500); // Budget: 0.5ms
    int id_cycle = TIMING_REGISTER("sensor_cyc",4000); // Budget: 4ms (toàn chu kỳ)

    RawInput_t raw = {0};
    TickType_t last_wake = xTaskGetTickCount();
    uint32_t   frame = 0;
    uint32_t   sensor_miss_count = 0;

    while (1) {
        TIMING_START(id_cycle);
        frame++;

        // BƯỚC 1: Đọc phần cứng (HAL)
        // Đo thời gian đọc thực tế để chống treo TWDT
        uint64_t hal_start = esp_timer_get_time();
        TIMING_START(id_hal);
        HAL_Input_Read(&raw);
        TIMING_END(id_hal);
        uint64_t hal_elapsed = esp_timer_get_time() - hal_start;

        // BƯỚC 2 + 3: Mixer + Ghi vào g_state
        // Tối ưu: Chỉ giữ mutex khi sao chép kết quả, không giữ khi tính toán toán học
        TIMING_START(id_mixer);
        
        // 1. Snapshot các cấu hình cần thiết để mixer chạy (Model, Calib)
        // Dùng timeout 1ms để chờ nếu radio_task đang copy
        if (xSemaphoreTake(sys_mutex, pdMS_TO_TICKS(1)) == pdTRUE) {
            SystemState_t snap = g_state; // Snapshot nhanh
            xSemaphoreGive(sys_mutex);

            // 2. Tính toán mixer trên bản snapshot (không chiếm mutex)
            mixer_process_frame(&raw, &snap.active_model, &snap.calib, &snap);

            // 3. Cập nhật kết quả vào g_state (chiếm mutex cực ngắn)
            if (xSemaphoreTake(sys_mutex, pdMS_TO_TICKS(1)) == pdTRUE) {
                memcpy(g_state.channels, snap.channels, sizeof(g_state.channels));
                g_state.sys_mode   = snap.sys_mode;
                g_state.is_armed   = snap.is_armed;
                g_state.tx_vbat    = snap.tx_vbat;
                g_state.tx_vbat_pct = snap.tx_vbat_pct;
                memcpy(g_state.raw_joy, snap.raw_joy, sizeof(g_state.raw_joy));
                
                // Cập nhật timestamp cho luồng Latency Debug
                g_state.trace.frame_id = frame;
                g_state.trace.ts_sampled = (uint32_t)esp_timer_get_time();
                
                xSemaphoreGive(sys_mutex);
            } else {
                sensor_miss_count++;
            }
        } else {
            sensor_miss_count++;
        }
        TIMING_END(id_mixer);
        TIMING_END(id_cycle);

        // Diagnostic: in report mỗi 1 giây (250 frame × 4ms)
        if (frame % 250 == 0) {
#if LOG_TIMING_REPORT
            ESP_LOGI(TAG, "[DIAG] sensor_miss=%lu / %lu frames (%.1f%%)",
                     sensor_miss_count, frame,
                     (float)sensor_miss_count * 100.0f / frame);
            TIMING_REPORT();
#endif
            // ── DEBUG MUX SCAN: Xác nhận wiring vật lý ──────────────────
#if LOG_MUX_SCAN_RAW
            HAL_Input_Debug_Scan();
#endif
        }

        // Chạy đúng 250Hz, vTaskDelayUntil bù thời gian đọc ADC.
        // NẾU chu kỳ đã bị trễ quá 3.5ms (do ADC treo), vTaskDelay(1) cưỡng bức 
        // để nhường CPU cho IDLE task trên Core 0, tránh TWDT Reset.
        if (hal_elapsed > 3500) {
            static uint32_t last_warn = 0;
            if (frame - last_warn > 250) { // Giới hạn log 1s/lần
                ESP_LOGW(TAG, "TWDT Safety: HAL took %lluus, forcing yield.", hal_elapsed);
                last_warn = frame;
            }
            vTaskDelay(1);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(4));
    }
}
