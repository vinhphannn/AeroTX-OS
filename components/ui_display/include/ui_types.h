#pragma once
#include <stdint.h>
#include <stdbool.h>

// --- Tham số Telemetry cho Dashboard ---
typedef struct {
    uint16_t throttle, roll, pitch, yaw;
    bool     armed, poshold, aux1, aux2;
    uint8_t  mode;      // Flight Mode (SW-B)
    uint8_t  sys_state; // System Mode (SW-A)
    uint32_t pkts;
    float    vbat_drone; // Drone Telemetry (Float for 12.6V)
    float    vbat_tx;    // TX Battery
    uint8_t  vbat_tx_pct;
    uint8_t  rssi;
    bool     is_connected;
    uint16_t p1, p2;
    bool     pid_updated;
} UIData_t;

typedef enum {
    NAV_NONE = 0,
    NAV_UP,
    NAV_DOWN,
    NAV_SELECT,
    NAV_BACK,
    NAV_INC,
    NAV_DEC
} nav_event_t;
