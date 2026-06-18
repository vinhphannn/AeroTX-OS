#include "ui_manager.h"
#include "core/sys_state.h"
#include "porting/lv_port_disp.h"
#include "porting/lv_port_indev.h"
#include "screens/scr_dashboard.h"
#include "screens/scr_telemetry.h"
#include "screens/scr_menu.h"
#include "screens/scr_calibration.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "UI_MGR";
static lv_obj_t *obj_dash, *obj_telem, *obj_menu, *obj_cal;

static void dashboard_click_cb(lv_event_t * e) { lv_scr_load(obj_telem); }
static void telemetry_click_cb(lv_event_t * e) { lv_scr_load(obj_dash); }

static void lv_tick_task(void *arg) { lv_tick_inc(2); }

void ui_manager_init(void) {
    lv_port_disp_init(); // Gọi driver (Trong đó đã có lv_init)
    lv_port_indev_init();

    // Create screens
    obj_dash = scr_dashboard_create();
    obj_telem = scr_telemetry_create();
    obj_menu = scr_menu_create();
    obj_cal  = scr_calibration_create();

    // Sự kiện chuyển màn hình bằng cảm ứng
    lv_obj_add_flag(obj_dash, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(obj_telem, LV_OBJ_FLAG_CLICKABLE);
    
    lv_obj_add_event_cb(obj_dash, dashboard_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(obj_telem, telemetry_click_cb, LV_EVENT_CLICKED, NULL);

    // Start LVGL Tick
    esp_timer_create_args_t tick_args = {.callback = &lv_tick_task, .name = "lvgl_tick"};
    esp_timer_handle_t tick_timer = NULL;
    esp_timer_create(&tick_args, &tick_timer);
    esp_timer_start_periodic(tick_timer, 2000); // 2ms tick

    lv_scr_load(obj_dash);
    ESP_LOGI(TAG, "UI Manager V4.0 Initialized");
}

void ui_manager_update_dashboard(UIData_t *data) {
    if (lv_scr_act() == obj_dash) scr_dashboard_update(data);
    else if (lv_scr_act() == obj_telem) scr_telemetry_update(data);
    else if (lv_scr_act() == obj_cal) scr_calibration_update();
}

void ui_manager_show_menu(void) {
    scr_menu_reset_state();
    lv_scr_load(obj_menu);
}

void ui_manager_hide_menu(void) {
    lv_scr_load(obj_dash);
}

void ui_manager_show_calibration(void) {
    scr_calibration_reset();
    lv_scr_load(obj_cal);
}

void ui_manager_update_sim(bool connected) {
    if (lv_scr_act() != obj_dash) lv_scr_load(obj_dash);
    scr_dashboard_set_sim_status(connected);
}

void ui_manager_restore_normal_status(UIData_t *data) {
    if (lv_scr_act() != obj_dash) lv_scr_load(obj_dash);
    scr_dashboard_restore_status(data);
}

void ui_manager_command(nav_event_t event) {
    if (lv_scr_act() == obj_menu) {
        scr_menu_command(event, ui_manager_hide_menu);
    } else if (lv_scr_act() == obj_cal) {
        if (event == NAV_SELECT) {
            extern esp_err_t storage_save_calib(CalibData_t *data);
            scr_calibration_finish();
            storage_save_calib(&g_state.calib);
            lv_scr_load(obj_menu); // Quay lại menu sau khi Save
        } else if (event == NAV_BACK) {
            lv_scr_load(obj_menu); // Quay lại menu khi Cancel
        }
    }
}

bool ui_manager_is_dashboard_active(void) {
    return (lv_scr_act() == obj_dash);
}

void ui_manager_update_pid(uint8_t part, float v1, float v2, float v3) {
    scr_menu_update_pid(part, v1, v2, v3);
}

uint8_t ui_manager_get_menu_index(void) { return scr_menu_get_index(); }
uint8_t ui_manager_get_pid_field(void) { return scr_menu_get_pid_field(); }

void ui_manager_timer_handler(void) {
    lv_timer_handler();
}

void ui_manager_force_ready(void) {
    lv_port_disp_force_ready();
}

void ui_manager_set_silence(bool silent) {
    lv_port_indev_set_silent(silent);
}
