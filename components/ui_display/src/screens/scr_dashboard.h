#pragma once
#include "lvgl.h"
#include "ui_types.h"

lv_obj_t* scr_dashboard_create(void);
void scr_dashboard_update(UIData_t *data);
void scr_dashboard_set_sim_status(bool connected);
void scr_dashboard_restore_status(UIData_t *data);
