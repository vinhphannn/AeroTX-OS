#include "scr_telemetry.h"
#include <stdio.h>

static lv_obj_t *obj_telem;
static lv_obj_t *label_fm;
static lv_obj_t *label_batt;
static lv_obj_t *label_rf;
static lv_obj_t *label_gps;
static lv_obj_t *label_att;

lv_obj_t* scr_telemetry_create(void) {
    obj_telem = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(obj_telem, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(obj_telem, LV_OBJ_FLAG_SCROLLABLE); // Tắt thanh cuộn toàn màn hình

    // Đã bỏ Header và Tiêu đề "PX4 CRSF TELEMETRY" để tiết kiệm không gian

    // Flight Mode
    label_fm = lv_label_create(obj_telem);
    lv_label_set_text(label_fm, "MODE: UNKNOWN");
    lv_obj_set_style_text_color(label_fm, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_style_text_font(label_fm, &lv_font_montserrat_14, 0);
    // Kéo dòng báo Mode lên sát mép trên để nhường chỗ
    lv_obj_align(label_fm, LV_ALIGN_TOP_MID, 0, 10);

    // Ứng dụng LVGL GRID Layout để chống đè chữ
    static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

    lv_obj_t *cont = lv_obj_create(obj_telem);
    // Đặt chiều cao cố định 180px để bao trọn mọi chữ, không sinh thanh cuộn
    lv_obj_set_size(cont, 300, 180);
    // Đẩy Grid hiển thị số liệu lên trên (Y = 35 thay vì 65)
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_set_style_bg_opa(cont, 0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE); // Chặn luôn thanh cuộn của riêng grid này
    
    // Set Layout thành Grid
    lv_obj_set_layout(cont, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(cont, col_dsc, row_dsc);
    // Tăng khoảng cách giữa 2 dòng từ 15px lên 30px để lấp đầy không trống thừa
    lv_obj_set_style_pad_row(cont, 30, 0);
    
    label_batt = lv_label_create(cont);
    lv_obj_set_style_text_color(label_batt, lv_color_hex(0x44FF44), 0);
    lv_obj_set_grid_cell(label_batt, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_START, 0, 1);

    label_rf = lv_label_create(cont);
    lv_obj_set_style_text_color(label_rf, lv_color_hex(0xFF4444), 0);
    // Align END để căn lề phải cho ô bên phải
    lv_obj_set_grid_cell(label_rf, LV_GRID_ALIGN_END, 1, 1, LV_GRID_ALIGN_START, 0, 1);
    lv_obj_set_style_text_align(label_rf, LV_TEXT_ALIGN_RIGHT, 0);

    label_gps = lv_label_create(cont);
    lv_obj_set_style_text_color(label_gps, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_grid_cell(label_gps, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_START, 1, 1);

    label_att = lv_label_create(cont);
    lv_obj_set_style_text_color(label_att, lv_color_hex(0xFFAA00), 0);
    lv_obj_set_grid_cell(label_att, LV_GRID_ALIGN_END, 1, 1, LV_GRID_ALIGN_START, 1, 1);
    lv_obj_set_style_text_align(label_att, LV_TEXT_ALIGN_RIGHT, 0);
    
    // Tap to return hint
    lv_obj_t *hint = lv_label_create(obj_telem);
    lv_label_set_text(hint, "Tap anywhere to return");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);

    return obj_telem;
}

void scr_telemetry_update(UIData_t *data) {
    if (!data->is_connected) {
        lv_label_set_text(label_fm, "MODE: OFFLINE");
        lv_label_set_text(label_batt, "BATTERY\n-- V\n-- A\n-- %");
        lv_label_set_text(label_rf, "RF LINK\nRSSI: --\nLQ: --\nSNR: --");
        lv_label_set_text(label_gps, "GPS\nSats: 0\nAlt: --\nLat: --");
        lv_label_set_text(label_att, "ATTITUDE\nPt: --\nRl: --\nYw: --");
        return;
    }

    if (data->flight_mode[0] != '\0') {
        lv_label_set_text_fmt(label_fm, "MODE: %s", data->flight_mode);
    } else {
        lv_label_set_text(label_fm, "MODE: CONNECTED");
    }

    lv_label_set_text_fmt(label_batt, "BATTERY\n%.1f V\n%.1f A\n%d %%", 
        data->vbat_drone, data->current_drone, data->batt_remaining);

    lv_label_set_text_fmt(label_rf, "RF LINK\nRSSI: %d dBm\nLQ: %d %%\nSNR: %d dB", 
        data->rssi, data->link_quality, data->snr);

    lv_label_set_text_fmt(label_gps, "GPS\nSats: %d\nAlt: %.1fm\nLat: %.4f", 
        data->gps_sats, data->gps_alt, data->gps_lat / 10000000.0f);

    lv_label_set_text_fmt(label_att, "ATTITUDE\nPt: %.1f\nRl: %.1f\nYw: %.1f", 
        data->drone_pitch * 57.2958f, data->drone_roll * 57.2958f, data->drone_yaw * 57.2958f);
}
