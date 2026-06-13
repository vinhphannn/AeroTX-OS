#include "scr_calibration.h"
#include "core/sys_state.h"
#include "ui_display.h"
#include <stdio.h>
#include <stdlib.h>

static lv_obj_t *scr_cal;
static lv_obj_t *bars[4];
static lv_obj_t *label_min_max[4];
static lv_obj_t *label_center[4];
static lv_obj_t *label_instruct;
static lv_obj_t *label_status;
static lv_obj_t *btn_finish;
static lv_obj_t *btn_cancel;

typedef struct {
    uint16_t min[4];
    uint16_t max[4];
    uint16_t center[4];
    uint16_t jitter_min[4];
    uint16_t jitter_max[4];
} session_cal_t;

static session_cal_t s_cal;

static void set_text_sharp(lv_obj_t *obj, lv_color_t color, const lv_font_t *font) {
    lv_obj_set_style_text_color(obj, color, 0);
    if (font) lv_obj_set_style_text_font(obj, font, 0);
}

static void finish_event_cb(lv_event_t * e) {
    extern void ui_manager_command(nav_event_t event);
    ui_manager_command(NAV_SELECT);
}

static void cancel_event_cb(lv_event_t * e) {
    extern void ui_manager_command(nav_event_t event);
    ui_manager_command(NAV_BACK);
}

lv_obj_t* scr_calibration_create(void) {
    scr_cal = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_cal, lv_color_hex(0x000000), 0);

    lv_obj_t *header = lv_obj_create(scr_cal);
    lv_obj_set_size(header, 320, 35);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(header, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    btn_cancel = lv_btn_create(header);
    lv_obj_set_size(btn_cancel, 60, 25);
    lv_obj_align(btn_cancel, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x333333), 0);
    lv_obj_add_event_cb(btn_cancel, cancel_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cll = lv_label_create(btn_cancel);
    lv_label_set_text(cll, "CANCEL");
    lv_obj_center(cll);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "SMART CALIBRATION");
    set_text_sharp(title, lv_color_hex(0x00FFFF), &lv_font_montserrat_14);
    lv_obj_align(title, LV_ALIGN_CENTER, 25, 0);

    label_instruct = lv_label_create(scr_cal);
    lv_label_set_text(label_instruct, "1. Sweep gimbals to EXTREMES\n2. Lower THR, Center Others & Wait");
    lv_obj_set_style_text_align(label_instruct, LV_TEXT_ALIGN_CENTER, 0);
    set_text_sharp(label_instruct, lv_color_hex(0xAAAAAA), &lv_font_montserrat_14);
    lv_obj_align(label_instruct, LV_ALIGN_TOP_MID, 0, 40);

    int start_y = 85;
    const char* names[] = {"THR", "YAW", "PIT", "ROL"};
    lv_color_t colors[] = {lv_color_hex(0x00FF00), lv_color_hex(0x00FFFF), lv_color_hex(0xFF4444), lv_color_hex(0xFFAA00)};

    for(int i=0; i<4; i++) {
        lv_obj_t *lbl = lv_label_create(scr_cal);
        lv_label_set_text(lbl, names[i]);
        set_text_sharp(lbl, lv_color_hex(0xFFFFFF), &lv_font_montserrat_14);
        lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 15, start_y + i*30);

        bars[i] = lv_bar_create(scr_cal);
        lv_obj_set_size(bars[i], 110, 8);
        lv_bar_set_range(bars[i], 0, 4095);
        lv_obj_set_style_bg_color(bars[i], lv_color_hex(0x111111), LV_PART_MAIN);
        lv_obj_set_style_bg_color(bars[i], colors[i], LV_PART_INDICATOR);
        lv_obj_align(bars[i], LV_ALIGN_TOP_LEFT, 55, start_y + i*30 + 5);

        label_min_max[i] = lv_label_create(scr_cal);
        set_text_sharp(label_min_max[i], lv_color_hex(0x888888), &lv_font_montserrat_14);
        lv_obj_align(label_min_max[i], LV_ALIGN_TOP_LEFT, 175, start_y + i*30);

        label_center[i] = lv_label_create(scr_cal);
        set_text_sharp(label_center[i], lv_color_hex(0xFFFF00), &lv_font_montserrat_14);
        lv_obj_align(label_center[i], LV_ALIGN_TOP_RIGHT, -10, start_y + i*30);
    }

    label_status = lv_label_create(scr_cal);
    lv_label_set_text(label_status, "TRACKING JITTER...");
    set_text_sharp(label_status, lv_color_hex(0x00FFFF), &lv_font_montserrat_14);
    lv_obj_align(label_status, LV_ALIGN_BOTTOM_MID, 0, -50);

    btn_finish = lv_btn_create(scr_cal);
    lv_obj_set_size(btn_finish, 200, 35);
    lv_obj_align(btn_finish, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_color(btn_finish, lv_color_hex(0x00AAAA), 0);
    lv_obj_add_event_cb(btn_finish, finish_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_lab = lv_label_create(btn_finish);
    lv_label_set_text(btn_lab, "SAVE CALIBRATION");
    lv_obj_center(btn_lab);

    scr_calibration_reset();
    return scr_cal;
}

void scr_calibration_reset(void) {
    for(int i=0; i<4; i++) {
        s_cal.min[i] = 4095;
        s_cal.max[i] = 0;
        s_cal.center[i] = 2048;
        s_cal.jitter_min[i] = 4095;
        s_cal.jitter_max[i] = 0;
    }
}

void scr_calibration_update(void) {
    SystemState_t snap;
    if (xSemaphoreTake(sys_mutex, 0) == pdTRUE) {
        snap = g_state;
        xSemaphoreGive(sys_mutex);
    } else return;

    for(int i=0; i<4; i++) {
        uint16_t v = snap.raw_joy[i];
        if (v < s_cal.min[i]) s_cal.min[i] = v;
        if (v > s_cal.max[i]) s_cal.max[i] = v;

        // Bắt nhiễu cho DB:
        // Nếu là trục i > 0 (R,P,Y), bắt nhiễu quanh điểm Center
        // Nếu là trục i = 0 (THR), bắt nhiễu bất cứ khi nào gậy đứng yên (jitter)
        if (abs((int)v - (int)s_cal.center[i]) < 50) {
             if (v < s_cal.jitter_min[i]) s_cal.jitter_min[i] = v;
             if (v > s_cal.jitter_max[i]) s_cal.jitter_max[i] = v;
        } else {
             s_cal.jitter_min[i] = v;
             s_cal.jitter_max[i] = v;
        }
        s_cal.center[i] = v;

        lv_bar_set_value(bars[i], v, LV_ANIM_OFF);
        lv_label_set_text_fmt(label_min_max[i], "%u-%u", s_cal.min[i], s_cal.max[i]);
        uint16_t db = (s_cal.jitter_max[i] > s_cal.jitter_min[i]) ? (s_cal.jitter_max[i] - s_cal.jitter_min[i]) : 0;
        lv_label_set_text_fmt(label_center[i], "DB:%d", db);
    }
}

void scr_calibration_finish(void) {
    lv_label_set_text(label_status, "ROM WRITE SUCCESS");
    lv_obj_set_style_text_color(label_status, lv_color_hex(0x00FF00), 0);

    if (xSemaphoreTake(sys_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for(int i=0; i<4; i++) {
            g_state.calib.min[i] = s_cal.min[i];
            g_state.calib.max[i] = s_cal.max[i];
            g_state.calib.center[i] = s_cal.center[i];
            uint16_t jitter_range = (s_cal.jitter_max[i] > s_cal.jitter_min[i]) ? (s_cal.jitter_max[i] - s_cal.jitter_min[i]) : 2;
            g_state.calib.deadband[i] = jitter_range + 3;
        }
        g_state.calib.valid = true;
        xSemaphoreGive(sys_mutex);
    }
}
