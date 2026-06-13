#pragma once
#include "ui_types.h"

// --- Public API ---
void Display_Init(void);
void Display_SetBacklight(uint8_t percent);
void Display_UpdateDashboard(UIData_t *data);
void Display_UpdateTimer(void);

// --- Menu Controls ---
void Display_ShowMenu(void);
void Display_HideMenu(void);
void Display_UpdateSimStatus(bool connected);
void Display_RestoreNormalStatus(UIData_t *data);
void Display_MenuCommand(nav_event_t event);
bool Display_IsDashboardActive(void);
void Display_UpdatePID(uint8_t part, float v1, float v2, float v3);
uint8_t Display_GetMenuIndex(void);
uint8_t Display_GetPIDFieldIndex(void);
void Display_ForceReady(void);
void Display_SetSilence(bool silent);
