#include "menu_pages.h"
#include <stdio.h>

// --- Live axis bars ---
static lv_obj_t *cont = NULL;
static lv_obj_t *bar_thr, *bar_yaw, *bar_pit, *bar_rol;
static lv_obj_t *lbl_thr, *lbl_yaw, *lbl_pit, *lbl_rol;
static lv_obj_t *lbl_arm, *lbl_aux;

static lv_obj_t* make_axis_row(lv_obj_t *parent, const char *name, lv_obj_t **out_bar, lv_obj_t **out_label, int y_pos) {
    lv_obj_t *lbl_name = lv_label_create(parent);
    lv_label_set_text(lbl_name, name);
    lv_obj_set_style_text_color(lbl_name, lv_color_hex(0x888888), 0);
    lv_obj_align(lbl_name, LV_ALIGN_TOP_LEFT, 8, y_pos);

    *out_bar = lv_bar_create(parent);
    lv_obj_set_size(*out_bar, 200, 12);
    lv_obj_align(*out_bar, LV_ALIGN_TOP_LEFT, 40, y_pos + 2);
    lv_bar_set_range(*out_bar, 1000, 2000);
    lv_bar_set_value(*out_bar, 1500, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(*out_bar, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_color(*out_bar, lv_color_hex(0x00FFFF), LV_PART_INDICATOR);
    lv_obj_set_style_radius(*out_bar, 4, 0);

    *out_label = lv_label_create(parent);
    lv_label_set_text(*out_label, "1500");
    lv_obj_set_style_text_color(*out_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(*out_label, LV_ALIGN_TOP_LEFT, 245, y_pos);
    return lbl_name;
}

static lv_obj_t* page_inputs_create(lv_obj_t *parent) {
    cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(cont, 0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);

    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "LIVE INPUTS");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00FFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    // Throttle (no center, range 1000-2000)
    lv_obj_t *lbl_t_name = lv_label_create(cont);
    lv_label_set_text(lbl_t_name, "THR");
    lv_obj_set_style_text_color(lbl_t_name, lv_color_hex(0x888888), 0);
    lv_obj_align(lbl_t_name, LV_ALIGN_TOP_LEFT, 8, 30);
    bar_thr = lv_bar_create(cont);
    lv_obj_set_size(bar_thr, 200, 12);
    lv_obj_align(bar_thr, LV_ALIGN_TOP_LEFT, 40, 32);
    lv_bar_set_range(bar_thr, 1000, 2000);
    lv_bar_set_value(bar_thr, 1000, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_thr, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_color(bar_thr, lv_color_hex(0xFF6600), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_thr, 4, 0);
    lbl_thr = lv_label_create(cont);
    lv_label_set_text(lbl_thr, "1000");
    lv_obj_set_style_text_color(lbl_thr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lbl_thr, LV_ALIGN_TOP_LEFT, 245, 30);

    make_axis_row(cont, "YAW", &bar_yaw, &lbl_yaw, 58);
    make_axis_row(cont, "PIT", &bar_pit, &lbl_pit, 86);
    make_axis_row(cont, "ROL", &bar_rol, &lbl_rol, 114);

    // Switch Status
    lbl_arm = lv_label_create(cont);
    lv_label_set_text(lbl_arm, "ARM: IDLE");
    lv_obj_set_style_text_color(lbl_arm, lv_color_hex(0x555555), 0);
    lv_obj_align(lbl_arm, LV_ALIGN_TOP_LEFT, 8, 148);

    lbl_aux = lv_label_create(cont);
    lv_label_set_text(lbl_aux, "SIM: OFF");
    lv_obj_set_style_text_color(lbl_aux, lv_color_hex(0x555555), 0);
    lv_obj_align(lbl_aux, LV_ALIGN_TOP_LEFT, 160, 148);

    return cont;
}

extern volatile uint16_t g_throttle, g_yaw, g_pitch, g_roll;
extern volatile bool g_armed, g_aux;

static void page_inputs_update(void) {
    if (!cont) return;

    // Get RC state from ui_manager (passed via global or extern)
    // We assume the UI Manager updates these globals from the rc state
    extern const void *ui_get_rc_state(void);
    // Use lv_bar update
    char buf[16];

    // Use the data from the dashboard's last rc_state copy if available
    // For now inline the update using the shared UI data
    // bars will be updated by the parent ui_manager calling update with rc data
}

static void page_inputs_command(nav_event_t event) {
    (void)event;
}

// Public update function called by ui_manager with live data
void page_inputs_set_data(uint16_t thr, uint16_t yaw, uint16_t pit, uint16_t rol, bool armed, bool aux) {
    if (!cont) return;

    char buf[16];
    lv_bar_set_value(bar_thr, thr, LV_ANIM_OFF);
    snprintf(buf, sizeof(buf), "%d", thr);
    lv_label_set_text(lbl_thr, buf);

    lv_bar_set_value(bar_yaw, yaw, LV_ANIM_OFF);
    snprintf(buf, sizeof(buf), "%d", yaw);
    lv_label_set_text(lbl_yaw, buf);

    lv_bar_set_value(bar_pit, pit, LV_ANIM_OFF);
    snprintf(buf, sizeof(buf), "%d", pit);
    lv_label_set_text(lbl_pit, buf);

    lv_bar_set_value(bar_rol, rol, LV_ANIM_OFF);
    snprintf(buf, sizeof(buf), "%d", rol);
    lv_label_set_text(lbl_rol, buf);

    if (armed) {
        lv_label_set_text(lbl_arm, "ARM: ARMED");
        lv_obj_set_style_text_color(lbl_arm, lv_color_hex(0x00FF00), 0);
    } else {
        lv_label_set_text(lbl_arm, "ARM: IDLE");
        lv_obj_set_style_text_color(lbl_arm, lv_color_hex(0x555555), 0);
    }

    if (aux) {
        lv_label_set_text(lbl_aux, "SIM: ON");
        lv_obj_set_style_text_color(lbl_aux, lv_color_hex(0xFF00FF), 0);
    } else {
        lv_label_set_text(lbl_aux, "SIM: OFF");
        lv_obj_set_style_text_color(lbl_aux, lv_color_hex(0x555555), 0);
    }
}

menu_page_t page_inputs = {
    .title   = "Live Inputs",
    .create  = page_inputs_create,
    .update  = page_inputs_update,
    .command = page_inputs_command,
    .destroy = NULL
};
