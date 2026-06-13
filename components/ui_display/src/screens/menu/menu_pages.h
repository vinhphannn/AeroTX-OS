#pragma once
#include "menu_common.h"
#include <stdbool.h>
#include <stdint.h>

extern menu_page_t page_inputs;
extern menu_page_t page_rf_link;
extern menu_page_t page_fc_tune;
extern menu_page_t page_system;
extern menu_page_t page_calibration;

void page_inputs_set_data(uint16_t thr, uint16_t yaw, uint16_t pit, uint16_t rol, bool armed, bool aux);
