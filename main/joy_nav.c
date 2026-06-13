#include "joy_nav.h"

// Ngưỡng kích hoạt (Thresholds)
#define THR_HIGH    1850
#define THR_LOW     1150
#define THR_CENTER_MIN 1400
#define THR_CENTER_MAX 1600

static bool pitch_released = true;
static bool roll_released = true;
static bool yaw_released = true;

nav_event_t joy_nav_process(RC_State_t *rc) {
    // 1. Kiểm tra Pitch (UP/DOWN)
    if (pitch_released) {
        if (rc->pitch > THR_HIGH) { pitch_released = false; return NAV_UP; }
        if (rc->pitch < THR_LOW)  { pitch_released = false; return NAV_DOWN; }
    }
    if (rc->pitch > THR_CENTER_MIN && rc->pitch < THR_CENTER_MAX) pitch_released = true;

    // 2. Kiểm tra Roll (SELECT/BACK) - Dùng để Vào/Ra
    if (roll_released) {
        if (rc->roll > THR_HIGH) { roll_released = false; return NAV_SELECT; }
        if (rc->roll < THR_LOW)  { roll_released = false; return NAV_BACK; }
    }
    if (rc->roll > THR_CENTER_MIN && rc->roll < THR_CENTER_MAX) roll_released = true;

    // 3. Kiểm tra Yaw (INC/DEC) - Dùng để Chỉnh PID
    if (yaw_released) {
        if (rc->yaw > THR_HIGH) { yaw_released = false; return NAV_INC; }
        if (rc->yaw < THR_LOW)  { yaw_released = false; return NAV_DEC; }
    }
    if (rc->yaw > THR_CENTER_MIN && rc->yaw < THR_CENTER_MAX) yaw_released = true;

    return NAV_NONE;
}
