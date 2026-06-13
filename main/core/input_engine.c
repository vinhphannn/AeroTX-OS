#include "core/sys_state.h"
#include <math.h>
#include <stdlib.h>

// --- Helper: Apply Exponential Curve ---
static uint16_t apply_expo(uint16_t val, uint8_t expo_pct) {
    if (expo_pct == 0) return val;
    float in = (val - 1500) / 500.0f;
    float expo = expo_pct / 100.0f;
    float out = (in * in * in) * expo + in * (1.0f - expo);
    return (uint16_t)(1500 + out * 500);
}

// --- Main Processing Engine ---
void input_process_frame(uint16_t raw_adc[4], uint16_t out_channels[4], CalibData_t *cal, ModelConfig_t *model) {
    for (int i = 0; i < 4; i++) {
        uint16_t val;

        if (cal->valid) {
            if (i == 0) {
                // CH0: THROTTLE (Logic Tuyến tính Min-Max, không có Center)
                if (raw_adc[i] <= cal->min[i]) val = 1000;
                else if (raw_adc[i] >= cal->max[i]) val = 2000;
                else {
                    val = 1000 + (uint32_t)(raw_adc[i] - cal->min[i]) * 1000 / (cal->max[i] - cal->min[i]);
                }

                // Chống nhiễu điểm thấp nhất cho Ga (Idle Safety)
                if (val < 1000 + cal->deadband[i]) val = 1000;
            }
            else {
                // CH1,2,3: ROL/PIT/YAW (Logic Dải kép có Center)
                if (raw_adc[i] < cal->center[i]) {
                    if (raw_adc[i] <= cal->min[i]) val = 1000;
                    else val = 1000 + (uint32_t)(raw_adc[i] - cal->min[i]) * 500 / (cal->center[i] - cal->min[i]);
                } else {
                    if (raw_adc[i] >= cal->max[i]) val = 2000;
                    else val = 1500 + (uint32_t)(raw_adc[i] - cal->center[i]) * 500 / (cal->max[i] - cal->center[i]);
                }

                // Deadband động tại tâm (Dùng số đo được khi Calib)
                if (abs((int)val - 1500) < cal->deadband[i]) val = 1500;
            }
        } else {
            // Uncalibrated Fallback (Safe defaults)
            int raw = (int)raw_adc[i];
            if (i == 0) { // Ga: rộng ra cho an toàn
                val = 1000 + (raw - 400) * 1000 / 3200;
            } else {
                val = 1000 + (raw - 500) * 1000 / 3000;
                if (abs((int)val - 1500) < 30) val = 1500;
            }
        }

        // 3. Giới hạn cứng 1000-2000
        if (val < 1000) val = 1000;
        if (val > 2000) val = 2000;

        // 4. Apply Expo (Thường ga không dùng expo này, nhưng vẫn để cho linh hoạt)
        if (i != 0) val = apply_expo(val, model->expo[i]);

        // 5. Apply Trims & Reversal
        int32_t final_val = (int32_t)val;
        if (i != 0) final_val += model->trims[i]; // Ga thường không dùng Trim

        if (model->reversed[i]) {
            final_val = 3000 - final_val;
        }

        if (final_val < 1000) final_val = 1000;
        if (final_val > 2000) final_val = 2000;

        out_channels[i] = (uint16_t)final_val;
    }
}
