#pragma once
#include "lvgl.h"

void lv_port_disp_init(void);
void lv_port_disp_set_backlight(uint8_t percent);
void lv_port_disp_force_ready(void);
