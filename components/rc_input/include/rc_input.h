#pragma once
#include <stdint.h>
#include <stdbool.h>


// =====================================================
// RC Input - Joystick ADC + Switches
// =====================================================

// --- Multiplexer CD74HC4067 Pins ---
#define MUX_SIG_PIN         34   // SIG (Analog Out) -> ADC1_CH6
#define MUX_S0_PIN          17
#define MUX_S1_PIN          22
#define MUX_S2_PIN          33
#define MUX_S3_PIN          26   // Né chân GPIO15 (Strap pin)

// --- Mux Channels (C0-C15) ---
#define CH_THROTTLE         1   // Swapped: physical wire on C1
#define CH_YAW              0   // Swapped: physical wire on C0
#define CH_PITCH            3   // Swapped: physical wire on C3
#define CH_ROLL             2   // Swapped: physical wire on C2
#define CH_P1               4
#define CH_P2               5
#define CH_SW_MODE          6
#define CH_SW_SPEED         7
#define CH_SW_ARM           8
#define CH_SW_BEEPER        9
#define CH_SW_LED           10
#define CH_SW_AUX           11

#define CH_VDD_REF          12  // Hardware voltage divider (1k-1k) for 3.3V sag compensation
#define CH_VBAT_TX          13  // TX Battery: 2S via 2.2k+1k divider → ESP32 ADC

// --- Cấu trúc dữ liệu RC đã xử lý ---
typedef struct {
    uint16_t throttle;   // 1000-2000
    uint16_t yaw;        // 1000-2000
    uint16_t pitch;      // 1000-2000
    uint16_t roll;       // 1000-2000
    uint16_t p1;         // Kênh AUX 1 (1000-2000)
    uint16_t p2;         // Kênh AUX 2 (1000-2000)
    bool     armed;
    uint8_t  mode;       // 0=Acro, 1=AltHold, 2=Auto
    uint8_t  speed;      // 0=Slow, 1=Normal, 2=Sport
    bool     beeper;
    bool     led;
    bool     aux;
    float    vbat_tx;    // TX Battery Voltage (V), e.g. 7.8V for 2S
} RC_State_t;

// --- API ---
void     RC_Input_Init(void);
void     RC_Input_Calibrate(void);
void     RC_Input_Read(RC_State_t *out);
uint16_t RC_Map(int raw, int in_min, int in_max);
uint8_t  RC_Read3WaySwitch(int adc_pin);
