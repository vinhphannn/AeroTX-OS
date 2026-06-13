#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// --- Định nghĩa giới hạn ---
#define MAX_CHANNELS    12
#define MAX_MODELS      10

// --- Cấu trúc Model (Lưu trong NVS) ---
typedef struct {
    uint8_t id;
    char    name[16];
    int16_t trims[4];    // Roll, Pitch, Yaw, Throttle (-200..200)
    uint8_t expo[4];     // 0-100%
    uint8_t rates[4];    // 0-100%
    bool    reversed[4];
} ModelConfig_t;

// --- Cấu trúc Calibration (Lưu trong NVS) ---
typedef struct {
    uint16_t min[4];
    uint16_t max[4];
    uint16_t center[4];
    uint16_t deadband[4]; // Biên độ nhiễu đo được tại tâm
    bool     valid;
} CalibData_t;

// --- Cấu trúc Telemetry (Nhận từ máy bay) ---
typedef struct {
    float    vbat_drone;
    uint8_t  rssi;           // Downlink (máy bay thu)
    uint8_t  link_quality;   // Uplink (TX nhận ACK)
    float    alt;
    bool     link_active;
} TelemetryData_t;

// --- Trạng thái toàn cục (Live State) ---
typedef struct {
    // RC Data
    uint16_t channels[MAX_CHANNELS]; // Processed (1000-2000)
    uint16_t raw_joy[4];             // Raw ADC (0-4095) for calibration
    bool     is_armed;

    // Hardware State
    float    tx_vbat;
    uint8_t  tx_vbat_pct;
    TelemetryData_t telemetry;

    // Active Config
    ModelConfig_t active_model;
    CalibData_t   calib;

    // System Flags
    uint8_t  sys_mode;  // 0: Flight, 1: Menu, 2: Sim
} SystemState_t;

// --- IPC Handles ---
extern QueueHandle_t sensor_queue;
extern SemaphoreHandle_t sys_mutex;
extern SystemState_t g_state;

// --- Task Priorities ---
// CPU0: sensor_task (PRIO 15, real-time) > radio_task (PRIO 12, background TX)
// CPU1: ui_task (PRIO 9) - độc lập, không ảnh hưởng IDLE0
// IDLE task mỗi CPU ở priority 0 - luôn phải được chạy để TWDT không trigger
#define PRIO_SENSOR_TASK    15  // Hạng Cao nhất CPU0 - đọc joystick 250Hz
#define PRIO_RADIO_TASK     12  // CPU0 - chạy sau sensor, send 250Hz
#define PRIO_UI_TASK         9  // CPU1 độc quyền - 30Hz refresh
