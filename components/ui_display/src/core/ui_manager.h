#pragma once
#include "ui_types.h"

void ui_manager_init(void);
void ui_manager_update_dashboard(UIData_t *data);
void ui_manager_show_menu(void);
void ui_manager_hide_menu(void);
void ui_manager_show_calibration(void);
void ui_manager_update_sim(bool connected);
void ui_manager_restore_normal_status(UIData_t *data);
void ui_manager_command(nav_event_t event);
bool ui_manager_is_dashboard_active(void);
void ui_manager_update_pid(uint8_t part, float v1, float v2, float v3);
uint8_t ui_manager_get_menu_index(void);
uint8_t ui_manager_get_pid_field(void);
void ui_manager_timer_handler(void);
void ui_manager_force_ready(void);
void ui_manager_set_silence(bool silent);
