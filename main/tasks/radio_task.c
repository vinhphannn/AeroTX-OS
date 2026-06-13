/**
 * radio_task.c - TX01 Radio Control Task (ELRS / CRSF Version)
 * ==========================================================
 * Chuyển đổi từ nRF24 sang ExpressLRS (CRSF Protocol).
 * Tốc độ baud: 420,000 bps.
 * Chu kỳ gửi: 4ms (250Hz) cố định.
 */

#include "core/sys_state.h"
#include "core/crsf.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "RADIO_TASK_ELRS";

void radio_task(void *pvParameters)
{
    // Snapshot channels hợp lệ gần nhất để Failsafe
    SystemState_t last_valid = {0};
    last_valid.channels[0]  = 1000;  // Throttle thấp nhất
    for (int i = 1; i < MAX_CHANNELS; i++) last_valid.channels[i] = 1500;
    
    // Khởi tạo UART cho CRSF
    crsf_init();
    ESP_LOGI(TAG, "CRSF UART Initialized at 420kbps.");

    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG, "ELRS Radio Task Started (250Hz)");

    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        SystemState_t snap;
        if (xSemaphoreTake(sys_mutex, pdMS_TO_TICKS(2)) == pdTRUE) {
            snap = g_state;
            xSemaphoreGive(sys_mutex);
            
            // Cập nhật bản sao hợp lệ
            if (snap.channels[0] >= 1000 && snap.channels[0] <= 2000) {
                last_valid = snap;
            }
        } else {
            // Failsafe: Dùng lại dữ liệu cũ nếu mutex bị kẹt
            snap = last_valid;
        }

        // Luôn bật nguồn nếu không ở chế độ Sim
        if (snap.sys_mode != 2) {
            crsf_set_power(true);
        }

        // B2: Quản lý trạng thái hệ thống
        if (snap.sys_mode == 2) {
            // Chế độ Sim: tắt radio hoàn toàn
            crsf_set_power(false);
            vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(4));
            continue;
        }

        // B3: Đóng gói và gửi CRSF RC Channels
        // ─────────────────────────────────────────────────────────
        // Layout cố định (KHÔNG thay đổi thứ tự mà không cập nhật QGC):
        //   Ch1-4  : Throttle / Yaw / Pitch / Roll  (joystick)
        //   Ch5-8  : 4 Công tắc 2 nấc (ARM, Beeper, LED, AUX)
        //   Ch9-10 : 2 Công tắc 3 nấc (Flight Mode, Sys Mode)
        //   Ch11-12: 2 Biến trở (P1, P2)
        //   Ch13-16: 1500 cố định (neutral, không nhiễu)
        // ─────────────────────────────────────────────────────────
        uint16_t crsf_channels[16];

        // QUAN TRỌNG: Khởi tạo TOÀN BỘ array = 1500 trước
        // Tránh giá trị rác stack tại ch13-16 gây nhiễu
        for (int i = 0; i < 16; i++) crsf_channels[i] = 1500;

        // Ch1-4: Joy (channels[0-3]) — đã đúng thứ tự từ mixer
        crsf_channels[0] = snap.channels[0];  // Ch1: Throttle
        crsf_channels[1] = snap.channels[1];  // Ch2: Yaw
        crsf_channels[2] = snap.channels[2];  // Ch3: Pitch
        crsf_channels[3] = snap.channels[3];  // Ch4: Roll

        // Ch5-8: 4 công tắc 2 nấc (channels[4-7])
        crsf_channels[4] = snap.channels[4];  // Ch5: ARM
        crsf_channels[5] = snap.channels[5];  // Ch6: Beeper
        crsf_channels[6] = snap.channels[6];  // Ch7: LED
        crsf_channels[7] = snap.channels[7];  // Ch8: AUX

        // Ch9-10: 2 công tắc 3 nấc (channels[8-9])
        crsf_channels[8] = snap.channels[8];  // Ch9: Flight Mode (Stab/Alt/Loiter)
        crsf_channels[9] = snap.channels[9];  // Ch10: Sys Mode (Flight/Menu/Sim)

        // Ch11-12: 2 biến trở (channels[10-11])
        crsf_channels[10] = snap.channels[10]; // Ch11: P1
        crsf_channels[11] = snap.channels[11]; // Ch12: P2

        // Ch13-16: đã được set 1500 ở trên, không cần làm gì thêm

        crsf_send_channels(crsf_channels);


        // B4: Log định kỳ (2 giây/lần)
        static uint32_t last_log = 0;
        uint32_t now = xTaskGetTickCount();
        if (now - last_log > pdMS_TO_TICKS(2000)) {
            ESP_LOGI(TAG, "[CRSF] Sending 250Hz | Thr: %d | Armed: %d", 
                     snap.channels[0], (snap.channels[5] > 1500));
            last_log = now;
        }

        // B5: Ngủ bù thời gian - Đảm bảo 250Hz
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(4));
    }
}
