/**
 * ui_task.c - Display & UI Management Task
 * ==========================================
 * Chạy trên Core 1, 60Hz (16ms/frame) - siêu mượt cho màn hình.
 *
 * v5.0 FIX:
 *   - vTaskDelay(30ms)           → vTaskDelayUntil(33ms) cố định
 *   - xSemaphoreTake(mutex, 5ms) → timeout=0, không chờ mutex
 *   - Nếu không lấy mutex được  → giữ frame cũ, render tiếp
 *
 * Nguyên tắc RTOS: UI task KHÔNG ĐƯỢC làm chậm Core 0 (sensor + radio).
 * UI có thể skip frame, nhưng RC và Radio thì không.
 */

#include "core/sys_state.h"
#include "core/sys_config.h"
#include "ui_display.h"
#include "ble_hid.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "UI_TASK";

// Ngưỡng thay đổi tối thiểu để trigger render (tránh LVGL render thừa)
#define CHAN_UPDATE_THRESHOLD  3     // µs
#define VBAT_UPDATE_THRESHOLD  0.1f  // Volt (Tăng lên để lọc nhiễu nhẹ)

void ui_task(void *pvParameters)
{
    ESP_LOGI(TAG, "UI Task V5.0 Started (30Hz, non-blocking mutex)");

    SystemState_t local_state = {0};
    UIData_t      ui_data     = {0};

    // Biến theo dõi thay đổi để tránh render thừa
    uint8_t  last_mode        = 99;   // Giá trị không hợp lệ để force render lần đầu
    uint16_t last_chans[MAX_CHANNELS] = {0};
    bool     last_connected   = false;
    bool     last_armed       = false;
    float    last_vbat_drone  = 0.0f;
    float    last_vbat_tx     = 0.0f;
    uint32_t last_telem_ms    = 0;
    bool     last_ble_conn    = false;

    // Diagnostic: đếm số frame bị skip do mutex bận
    uint32_t ui_miss_count    = 0;
    uint32_t ui_frame_count   = 0;

    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        ui_frame_count++;

        // ---------------------------------------------------------------
        // B1: Snapshot g_state - KHÔNG chờ mutex
        //     Nếu sensor/radio đang giữ mutex → dùng local_state từ frame trước
        //     UI có thể hiện dữ liệu trễ 33ms - hoàn toàn chấp nhận được
        // ---------------------------------------------------------------
        if (xSemaphoreTake(sys_mutex, pdMS_TO_TICKS(2)) == pdTRUE) {
            local_state = g_state;
            xSemaphoreGive(sys_mutex);
        } else {
            ui_miss_count++;
            // Tiếp tục render với local_state cũ - không skip frame
        }

        // ---------------------------------------------------------------
        // B2: Xử lý chuyển Mode (FLIGHT / MENU / SIM)
        //     Chỉ chạy khi mode thực sự thay đổi
        // ---------------------------------------------------------------
        if (local_state.sys_mode != last_mode) {
            switch (local_state.sys_mode) {
            case 0: // FLIGHT
                Display_HideMenu();
                Display_SetSilence(false);
                memset(last_chans, 0, sizeof(last_chans)); // Force re-render dashboard
                break;
            case 1: // MENU
                Display_ShowMenu();
                break;
            case 2: // SIM
                Display_HideMenu();
                Display_SetSilence(true);
                memset(last_chans, 0, sizeof(last_chans)); // Force re-render để cập nhật chữ SIM
                break;
            default:
                break;
            }
            last_mode = local_state.sys_mode;
#if LOG_SYS_MODE_CHANGES
            ESP_LOGI(TAG, "Mode changed → %d", local_state.sys_mode);
#endif
        }

        // ---------------------------------------------------------------
        // B3: Cập nhật Dashboard (Mode FLIGHT, MENU, hoặc SIM)
        // ---------------------------------------------------------------
        if (local_state.sys_mode == 0 || local_state.sys_mode == 1 || local_state.sys_mode == 2) {

            // Kiểm tra có gì thay đổi đáng kể không
            bool need_render = false;

            // Kiểm tra channels joystick (bỏ qua nhiễu nhỏ hơn threshold)
            for (int i = 0; i < 9; i++) {
                if (abs((int)local_state.channels[i] - (int)last_chans[i]) > CHAN_UPDATE_THRESHOLD) {
                    last_chans[i] = local_state.channels[i];
                    need_render   = true;
                }
            }

            // Menu mode luôn render để thấy jitter joystick trong Calib wizard
            if (local_state.sys_mode == 1) need_render = true;

            // Kiểm tra thay đổi trạng thái quan trọng
            if (local_state.telemetry.link_active != last_connected) {
                last_connected = local_state.telemetry.link_active;
                need_render    = true;
            }
            if (local_state.is_armed != last_armed) {
                last_armed  = local_state.is_armed;
                need_render = true;
            }
            if (local_state.telemetry.vbat_drone - last_vbat_drone > VBAT_UPDATE_THRESHOLD ||
                last_vbat_drone - local_state.telemetry.vbat_drone > VBAT_UPDATE_THRESHOLD) {
                last_vbat_drone = local_state.telemetry.vbat_drone;
                need_render     = true;
            }
            if (local_state.tx_vbat - last_vbat_tx > VBAT_UPDATE_THRESHOLD ||
                last_vbat_tx - local_state.tx_vbat > VBAT_UPDATE_THRESHOLD) {
                last_vbat_tx = local_state.tx_vbat;
                need_render  = true;
            }
            if (local_state.telemetry.last_recv_ms != last_telem_ms) {
                last_telem_ms = local_state.telemetry.last_recv_ms;
                need_render = true;
            }
            if (local_state.sys_mode == 2) {
                bool current_ble = ble_hid_is_connected();
                if (current_ble != last_ble_conn) {
                    last_ble_conn = current_ble;
                    need_render = true;
                }
            }

            if (need_render) {
                // ── Joystick (channels[0-3]) ──────────────────────────
                ui_data.throttle    = local_state.channels[0];  // Ch1
                ui_data.yaw         = local_state.channels[1];  // Ch2
                ui_data.pitch       = local_state.channels[2];  // Ch3
                ui_data.roll        = local_state.channels[3];  // Ch4

                // ── 4 Công tắc 2 nấc (channels[4-7]) ────────────────
                ui_data.armed       = last_armed;                          // Ch5: ARM (cập nhật riêng ở trên)
                ui_data.poshold     = (local_state.channels[5] > 1500);   // Ch6: Beeper
                ui_data.aux1        = (local_state.channels[6] > 1500);   // Ch7: LED
                ui_data.aux2        = (local_state.channels[7] > 1500);   // Ch8: AUX

                // ── 2 Công tắc 3 nấc ─────────────────────────────────
                ui_data.mode        = (local_state.channels[8] - 1000) / 500; // Ch9: Flight Mode (0/1/2)
                ui_data.sys_state   = local_state.sys_mode;                    // Ch10: Sys Mode (từ sys_mode trực tiếp)

                // ── 2 Biến trở ────────────────────────────────────────
                ui_data.p1          = local_state.channels[10]; // Ch11: P1
                ui_data.p2          = local_state.channels[11]; // Ch12: P2

                // ── Telemetry & Connection ─────────────────────────────────────────
                ui_data.vbat_tx     = local_state.tx_vbat;
                ui_data.vbat_tx_pct = local_state.tx_vbat_pct;
                
                if (local_state.sys_mode == 2) {
                    // Chế độ Sim: Trạng thái kết nối lấy từ Bluetooth
                    ui_data.is_connected = ble_hid_is_connected();
                } else {
                    // Chế độ Bay: Trạng thái lấy từ Radio Telemetry
                    ui_data.vbat_drone  = local_state.telemetry.vbat_drone;
                    ui_data.rssi        = local_state.telemetry.rssi;
                    ui_data.is_connected = last_connected;
                }
                
                // Copy Telemetry mở rộng
                ui_data.current_drone = local_state.telemetry.current_drone;
                ui_data.batt_remaining = local_state.telemetry.batt_remaining;
                ui_data.snr = local_state.telemetry.snr;
                ui_data.link_quality = local_state.telemetry.link_quality;
                ui_data.gps_lat = local_state.telemetry.gps_lat;
                ui_data.gps_lon = local_state.telemetry.gps_lon;
                ui_data.gps_alt = local_state.telemetry.gps_alt;
                ui_data.gps_sats = local_state.telemetry.gps_sats;
                ui_data.drone_pitch = local_state.telemetry.pitch;
                ui_data.drone_roll = local_state.telemetry.roll;
                ui_data.drone_yaw = local_state.telemetry.yaw;
                strncpy(ui_data.flight_mode, local_state.telemetry.flight_mode, sizeof(ui_data.flight_mode)-1);

                Display_UpdateDashboard(&ui_data);

                // Latency Debug: In log mỗi khi UI render frame mới (mỗi ~2 giây)
#if LOG_LATENCY_DEBUG
                static uint32_t ui_log_cnt = 0;
                if (ui_log_cnt++ % 120 == 0) {
                    uint32_t render_ts = (uint32_t)esp_timer_get_time();
                    uint32_t lag = render_ts - local_state.trace.ts_sampled;
                    ESP_LOGI("LATENCY_DEBUG", "[UI] Frame %lu | Sensor -> UI Render: %lu us (%.2f ms)", 
                             local_state.trace.frame_id, lag, lag / 1000.0f);
                }
#endif
            }
        }
        // (Đã gộp xử lý hiển thị Sim vào B3)

        // ---------------------------------------------------------------
        // B5: Timer bay - cập nhật mỗi frame (30Hz)
        // ---------------------------------------------------------------
        Display_UpdateTimer();

        // ---------------------------------------------------------------
        // B6 (DIAGNOSTIC): Log định kỳ để kiểm tra sức khỏe hệ thống
        // In ra: miss rate của UI, tổng frame count
        // Nếu miss_rate > 5% → có vấn đề về tranh mutex với Core 0
        // ---------------------------------------------------------------
        if (ui_frame_count % 300 == 0) { // Mỗi 10 giây (300 × 33ms)
            uint32_t miss_pct = (ui_miss_count * 100) / ui_frame_count;
            if (miss_pct > 5) {
                ESP_LOGW(TAG, "[DIAG] UI miss rate: %lu%% (%lu/%lu frames) - Check mutex contention!",
                         miss_pct, ui_miss_count, ui_frame_count);
            } else {
#if LOG_UI_DIAGNOSTICS
                ESP_LOGI(TAG, "[DIAG] UI healthy: miss=%lu%% | frames=%lu",
                         miss_pct, ui_frame_count);
#endif
            }
        }

        // ---------------------------------------------------------------
        // B7: Chạy đúng 60Hz (16ms/frame)
        // vTaskDelayUntil tự bù thời gian xử lý LVGL render
        // ---------------------------------------------------------------
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(16));
    }
}
