#include "ui_display.h"
#include "core/ui_manager.h"
#include "porting/lv_port_disp.h"
#include <esp_log.h>
static const char *TAG = "UI_TRACE";

void Display_Init(void) {
    ui_manager_init();
}

void Display_SetBacklight(uint8_t percent) {
    lv_port_disp_set_backlight(percent);
}

void Display_UpdateDashboard(UIData_t *data) {
    ui_manager_update_dashboard(data);
}

void Display_UpdateTimer(void) {
    ui_manager_timer_handler();
}

void Display_SetSilence(bool silent) {
    ESP_LOGI(TAG, "Display Silence: %s", silent ? "ON" : "OFF");
    ui_manager_set_silence(silent);
}

void Display_ShowMenu(void) {
    ESP_LOGI(TAG, "Enter ShowMenu");
    ui_manager_show_menu();
}

void Display_HideMenu(void) {
    ESP_LOGI(TAG, "Enter HideMenu");
    ui_manager_hide_menu();
}

void Display_UpdateSimStatus(bool connected) {
    ui_manager_update_sim(connected);
}

void Display_RestoreNormalStatus(UIData_t *data) {
    ui_manager_restore_normal_status(data);
}

void Display_MenuCommand(nav_event_t event) {
    if (event != NAV_NONE) ESP_LOGI(TAG, "Menu Cmd: %d", event);
    ui_manager_command(event);
}

bool Display_IsDashboardActive(void) {
    return ui_manager_is_dashboard_active();
}

void Display_UpdatePID(uint8_t part, float v1, float v2, float v3) {
    ui_manager_update_pid(part, v1, v2, v3);
}

uint8_t Display_GetMenuIndex(void) {
    return ui_manager_get_menu_index();
}

uint8_t Display_GetPIDFieldIndex(void) {
    return ui_manager_get_pid_field();
}

void Display_ForceReady(void) {
    ESP_LOGW(TAG, "!!! HARDWARE FORCE READY TRIGGERED !!!");
    ui_manager_force_ready();
}
