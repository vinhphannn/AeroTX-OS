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
    char buf[64]; // Buffer tạm dùng snprintf chuẩn, tránh bug float/double của LVGL

    if (!data->is_connected) {
        lv_label_set_text(label_fm,   "MODE: OFFLINE");
        lv_label_set_text(label_batt, "BATTERY\n-- V\n-- A\n-- %");
        lv_label_set_text(label_rf,   "RF LINK\nRSSI: --\nLQ: --\nSNR: --");
        lv_label_set_text(label_gps,  "GPS\nSats: 0\nAlt: --\nLat: --");
        lv_label_set_text(label_att,  "ATTITUDE\nPt: --\nRl: --\nYw: --");
        return;
    }

    // Flight Mode — đây cũng là "bảng lỗi" của PX4:
    // PX4 encode toàn bộ lỗi/cảnh báo vào chuỗi này
    // VD: "FAILSAFE", "PREFLT FAIL", "AUTO.LAND", "AUTO.RTL"
    if (data->flight_mode[0] != '\0') {
        snprintf(buf, sizeof(buf), "MODE: %s", data->flight_mode);
    } else {
        snprintf(buf, sizeof(buf), "MODE: CONNECTED");
    }
    lv_label_set_text(label_fm, buf);

    // Đổi màu label theo mức độ nghiêm trọng của trạng thái PX4
    const char *fm = data->flight_mode;
    if (strstr(fm, "FAILSAFE") || strstr(fm, "PREFLT") || strstr(fm, "FAIL") || strstr(fm, "N/A")) {
        // ĐỎ: lỗi nghiêm trọng, không thể bay / mất tín hiệu
        lv_obj_set_style_text_color(label_fm, lv_color_hex(0xFF2222), 0);
    } else if (strstr(fm, "RTL") || strstr(fm, "LAND") || strstr(fm, "LOITER") || strstr(fm, "HOLD")) {
        // VÀNG: đang thực hiện maneuver tự động, cần chú ý
        lv_obj_set_style_text_color(label_fm, lv_color_hex(0xFFAA00), 0);
    } else {
        // XANH LÁ: trạng thái bay bình thường
        lv_obj_set_style_text_color(label_fm, lv_color_hex(0x44FF44), 0);
    }

    // Battery — dùng snprintf chuẩn để tránh lv_snprintf float/double mismatch
    // Root cause: C ABI promotes float→double (8B) nhưng lv_snprintf đọc 4B → %d lệch stack
    {
        // Tách riêng từng giá trị ra int để tránh hoàn toàn float trong variadic
        int v_int  = (int)data->vbat_drone;
        int v_dec  = (int)((data->vbat_drone  - v_int) * 10.0f);
        int c_int  = (int)data->current_drone;
        int c_dec  = (int)((data->current_drone - c_int) * 10.0f);
        int batt_pct = (int)(uint8_t)data->batt_remaining; // double-cast để chắc chắn 0-100
        if (v_dec < 0) v_dec = -v_dec;
        if (c_dec < 0) c_dec = -c_dec;
        snprintf(buf, sizeof(buf), "BATTERY\n%d.%d V\n%d.%d A\n%d %%",
                 v_int, v_dec, c_int, c_dec, batt_pct);
    }
    lv_label_set_text(label_batt, buf);

    // RF Link — int8_t/uint8_t, an toàn với %d
    snprintf(buf, sizeof(buf), "RF LINK\nRSSI: %d dBm\nLQ: %d %%\nSNR: %d dB",
             (int)data->rssi, (int)data->link_quality, (int)data->snr);
    lv_label_set_text(label_rf, buf);

    // GPS — float altitude và lat, tách int để tránh LVGL float bug
    {
        int alt_int = (int)data->gps_alt;
        int alt_dec = (int)((data->gps_alt - alt_int) * 10.0f);
        float lat_f = data->gps_lat / 10000000.0f;
        int lat_int = (int)lat_f;
        int lat_dec = (int)((lat_f - lat_int) * 10000.0f);
        if (alt_dec < 0) alt_dec = -alt_dec;
        if (lat_dec < 0) lat_dec = -lat_dec;
        snprintf(buf, sizeof(buf), "GPS\nSats: %d\nAlt: %d.%dm\nLat: %d.%04d",
                 (int)data->gps_sats, alt_int, alt_dec, lat_int, lat_dec);
    }
    lv_label_set_text(label_gps, buf);

    // Attitude — float rad → degree, tách int
    {
        float pt_deg = data->drone_pitch * 57.2958f;
        float rl_deg = data->drone_roll  * 57.2958f;
        float yw_deg = data->drone_yaw   * 57.2958f;
        int pt_i = (int)pt_deg, pt_d = (int)((pt_deg - pt_i) * 10.0f);
        int rl_i = (int)rl_deg, rl_d = (int)((rl_deg - rl_i) * 10.0f);
        int yw_i = (int)yw_deg, yw_d = (int)((yw_deg - yw_i) * 10.0f);
        if (pt_d < 0) pt_d = -pt_d;
        if (rl_d < 0) rl_d = -rl_d;
        if (yw_d < 0) yw_d = -yw_d;
        snprintf(buf, sizeof(buf), "ATTITUDE\nPt: %d.%d\nRl: %d.%d\nYw: %d.%d",
                 pt_i, pt_d, rl_i, rl_d, yw_i, yw_d);
    }
    lv_label_set_text(label_att, buf);
}
