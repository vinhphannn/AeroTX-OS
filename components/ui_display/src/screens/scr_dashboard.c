#include "scr_dashboard.h"
#include <stdio.h>

static lv_obj_t *obj_main_cont;
static lv_obj_t *bar_thr, *bar_yaw, *bar_pitch, *bar_roll;
static lv_obj_t *lab_thr_val, *lab_yaw_val, *lab_pit_val, *lab_rol_val;
static lv_obj_t *label_status, *label_vbat_tx, *label_vbat_rx, *label_rssi;
static lv_obj_t *leds_top[4];
static lv_obj_t *leds_bot[2];

static void disable_scroll(lv_obj_t *obj) {
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

static void set_text_sharp(lv_obj_t *obj, lv_color_t color) {
    lv_obj_set_style_text_color(obj, color, 0);
    lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, 0);
}

static lv_obj_t* create_channel_row(lv_obj_t *parent, const char *name, lv_color_t color, lv_obj_t **out_val_lab) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), lv_pct(23));
    lv_obj_set_style_bg_opa(row, 0, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    disable_scroll(row);

    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, name);
    lv_obj_set_width(label, lv_pct(22));
    set_text_sharp(label, lv_color_hex(0xFFFFFF));
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_pad_right(label, 8, 0);

    lv_obj_t *bar = lv_bar_create(row);
    lv_obj_set_flex_grow(bar, 1);
    lv_obj_set_height(bar, 15);
    lv_bar_set_range(bar, 1000, 2000);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x1A1A1A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 2, 0);
    lv_obj_set_style_outline_width(bar, 1, LV_PART_MAIN);
    lv_obj_set_style_outline_color(bar, lv_color_hex(0x444444), 0);

    *out_val_lab = lv_label_create(row);
    lv_label_set_text(*out_val_lab, "1500");
    lv_obj_set_width(*out_val_lab, lv_pct(20));
    set_text_sharp(*out_val_lab, lv_color_hex(0x00FFFF));
    lv_obj_set_style_text_align(*out_val_lab, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_pad_left(*out_val_lab, 8, 0);

    return bar;
}

lv_obj_t* scr_dashboard_create(void) {
    obj_main_cont = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(obj_main_cont, lv_color_hex(0x000000), 0);
    lv_obj_set_flex_flow(obj_main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_hor(obj_main_cont, 15, 0);
    lv_obj_set_style_pad_ver(obj_main_cont, 8, 0);
    lv_obj_set_style_pad_row(obj_main_cont, 4, 0);
    disable_scroll(obj_main_cont);

    lv_obj_t *header = lv_obj_create(obj_main_cont);
    lv_obj_set_size(header, lv_pct(100), 38);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x111111), 0);
    lv_obj_set_style_radius(header, 8, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(header, 12, 0);
    disable_scroll(header);

    lv_obj_t *v_cont = lv_obj_create(header);
    lv_obj_set_size(v_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(v_cont, 0, 0);
    lv_obj_set_style_border_width(v_cont, 0, 0);
    lv_obj_set_flex_flow(v_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(v_cont, 0, 0);
    lv_obj_set_style_pad_gap(v_cont, 8, 0);
    disable_scroll(v_cont);

    label_vbat_tx = lv_label_create(v_cont);
    set_text_sharp(label_vbat_tx, lv_color_hex(0xFFFFFF));

    lv_obj_t *sep = lv_label_create(v_cont);
    lv_label_set_text(sep, "|");
    set_text_sharp(sep, lv_color_hex(0x444444));

    label_vbat_rx = lv_label_create(v_cont);
    set_text_sharp(label_vbat_rx, lv_color_hex(0x44FF44));

    label_rssi = lv_label_create(header);
    set_text_sharp(label_rssi, lv_color_hex(0xFFFF00));

    lv_obj_t *sw_tray = lv_obj_create(obj_main_cont);
    lv_obj_set_size(sw_tray, lv_pct(100), 25);
    lv_obj_set_style_bg_opa(sw_tray, 0, 0);
    lv_obj_set_style_border_width(sw_tray, 0, 0);
    lv_obj_set_flex_flow(sw_tray, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sw_tray, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(sw_tray, 18, 0);
    disable_scroll(sw_tray);

    for(int i=0; i<4; i++) {
        leds_top[i] = lv_obj_create(sw_tray);
        lv_obj_set_size(leds_top[i], 9, 9);
        lv_obj_set_style_radius(leds_top[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(leds_top[i], lv_color_hex(0x222222), 0);
        disable_scroll(leds_top[i]);
    }

    lv_obj_t *body = lv_obj_create(obj_main_cont);
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_width(body, lv_pct(100));
    lv_obj_set_style_bg_opa(body, 0, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    disable_scroll(body);

    bar_thr   = create_channel_row(body, "THR", lv_color_hex(0x00FF00), &lab_thr_val);
    bar_yaw   = create_channel_row(body, "YAW", lv_color_hex(0x00FFFF), &lab_yaw_val);
    bar_pitch = create_channel_row(body, "PIT", lv_color_hex(0xFF4444), &lab_pit_val);
    bar_roll  = create_channel_row(body, "ROL", lv_color_hex(0xFFAA00), &lab_rol_val);

    lv_obj_t *footer = lv_obj_create(obj_main_cont);
    lv_obj_set_size(footer, lv_pct(100), 40);
    lv_obj_set_style_bg_color(footer, lv_color_hex(0x0D0D0D), 0);
    lv_obj_set_style_radius(footer, 5, 0);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(footer, 12, 0);
    disable_scroll(footer);

    label_status = lv_label_create(footer);

    lv_obj_t *mode_led_cont = lv_obj_create(footer);
    lv_obj_set_size(mode_led_cont, 55, 20);
    lv_obj_set_style_bg_opa(mode_led_cont, 0, 0);
    lv_obj_set_style_border_width(mode_led_cont, 0, 0);
    lv_obj_set_flex_flow(mode_led_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(mode_led_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(mode_led_cont, 10, 0);
    disable_scroll(mode_led_cont);

    for(int i=0; i<2; i++) {
        leds_bot[i] = lv_obj_create(mode_led_cont);
        lv_obj_set_size(leds_bot[i], 16, 11);
        lv_obj_set_style_radius(leds_bot[i], 2, 0);
        disable_scroll(leds_bot[i]);
    }

    return obj_main_cont;
}

static void update_active_led(lv_obj_t *led, bool active, lv_color_t color) {
    lv_obj_set_style_bg_color(led, active ? color : lv_color_hex(0x222222), 0);
    lv_obj_set_style_shadow_width(led, active ? 12 : 0, 0);
    lv_obj_set_style_shadow_color(led, color, 0);
}

void scr_dashboard_update(UIData_t *data) {
    if (!data->is_connected) {
        lv_label_set_text(label_status, "STATION READY");
        lv_obj_set_style_text_color(label_status, lv_color_hex(0xAAAAAA), 0);
        lv_label_set_text(label_vbat_rx, "RX: --V");
        lv_label_set_text(label_rssi, "RSSI: 0%");
    } else {
        lv_label_set_text_fmt(label_status, "%s | M%d", data->armed ? "ARMED" : "IDLE", data->mode);
        lv_obj_set_style_text_color(label_status, data->armed ? lv_color_hex(0xFF4444) : lv_color_hex(0x44FF44), 0);

        lv_label_set_text_fmt(label_vbat_rx, "RX: %.1fV", data->vbat_drone);
        lv_label_set_text_fmt(label_rssi, "RSSI: %d%%", data->rssi);
    }

    update_active_led(leds_top[0], data->armed, lv_color_hex(0xFF0000));
    update_active_led(leds_top[1], data->poshold, lv_color_hex(0x00FFFF));
    update_active_led(leds_top[2], data->aux1, lv_color_hex(0x00FF00));
    update_active_led(leds_top[3], data->aux2, lv_color_hex(0xFFFF00));

    lv_color_t flt_cols[] = {lv_color_hex(0x00FF00), lv_color_hex(0x00FFFF), lv_color_hex(0x0000FF)};
    lv_obj_set_style_bg_color(leds_bot[0], flt_cols[data->mode % 3], 0);

    lv_color_t sys_cols[] = {lv_color_hex(0x00FF00), lv_color_hex(0xAAAA00), lv_color_hex(0xFF0000)};
    lv_obj_set_style_bg_color(leds_bot[1], sys_cols[data->sys_state % 3], 0);

    lv_bar_set_value(bar_thr, data->throttle, LV_ANIM_OFF);
    lv_label_set_text_fmt(lab_thr_val, "%u", data->throttle);

    lv_bar_set_value(bar_yaw, data->yaw, LV_ANIM_OFF);
    lv_label_set_text_fmt(lab_yaw_val, "%u", data->yaw);

    lv_bar_set_value(bar_pitch, data->pitch, LV_ANIM_OFF);
    lv_label_set_text_fmt(lab_pit_val, "%u", data->pitch);

    lv_bar_set_value(bar_roll, data->roll, LV_ANIM_OFF);
    lv_label_set_text_fmt(lab_rol_val, "%u", data->roll);

    // FIX: Không dùng %.1f để tránh lỗi in chuỗi dài (Format issues)
    int v_int = (int)data->vbat_tx;
    int v_dec = (int)((data->vbat_tx - v_int) * 10.0f);
    if (v_dec < 0) v_dec = 0;
    lv_label_set_text_fmt(label_vbat_tx, "TX: %d.%dV %d%%", v_int, v_dec, data->vbat_tx_pct);
}

void scr_dashboard_restore_status(UIData_t *data) {
    scr_dashboard_update(data);
}

void scr_dashboard_set_sim_status(bool connected) {
    lv_label_set_text(label_status, connected ? "SIM ACTIVE" : "SIM OFF");
    lv_obj_set_style_text_color(label_status, lv_color_hex(0x00FFFF), 0);
}
