#pragma once
#include "lvgl.h"
#include "ui_types.h"

// Page lifecycle and event handling interface
typedef struct {
    const char *title;
    lv_obj_t* (*create)(lv_obj_t *parent);
    void (*update)(void);
    void (*command)(nav_event_t event);
    void (*destroy)(void);
} menu_page_t;

// Shared styling helper (optional)
static inline void menu_style_header(lv_obj_t *obj) {
    lv_obj_set_style_text_color(obj, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, 0);
}
