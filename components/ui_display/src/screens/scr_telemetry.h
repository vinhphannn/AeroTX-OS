#pragma once
#include "lvgl.h"
#include "ui_types.h"

lv_obj_t* scr_telemetry_create(void);
void scr_telemetry_update(UIData_t *data);
