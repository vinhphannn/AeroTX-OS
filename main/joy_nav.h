#pragma once
#include <stdint.h>
#include "rc_input.h"

#include "ui_types.h"

/**
 * @brief Chuyển đổi dữ liệu RC thô thành sự kiện điều hướng Menu
 * @param rc Pointer tới trạng thái RC hiện tại
 * @return nav_event_t Sự kiện được kích hoạt (chỉ kích hoạt 1 lần mỗi khi gạt)
 */
nav_event_t joy_nav_process(RC_State_t *rc);
