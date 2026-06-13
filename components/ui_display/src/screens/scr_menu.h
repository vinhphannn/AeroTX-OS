#pragma once
#include "lvgl.h"
#include "ui_types.h"

lv_obj_t* scr_menu_create(void);
void scr_menu_update_pid(uint8_t part, float v1, float v2, float v3);
void scr_menu_command(nav_event_t event, void (*on_exit)(void));
uint8_t scr_menu_get_index(void);
uint8_t scr_menu_get_pid_field(void);
void scr_menu_reset_state(void);
