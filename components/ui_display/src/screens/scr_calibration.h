#pragma once
#include "lvgl.h"

lv_obj_t* scr_calibration_create(void);
void scr_calibration_update(void);
void scr_calibration_reset(void);
void scr_calibration_finish(void);
