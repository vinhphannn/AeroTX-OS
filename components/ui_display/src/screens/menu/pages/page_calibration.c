#include "menu_common.h"
#include "core/ui_manager.h"

static void btn_start_cb(lv_event_t * e) {
    ui_manager_show_calibration();
}

static lv_obj_t* page_cal_create(lv_obj_t *parent) {
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(cont, 0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *info = lv_label_create(cont);
    lv_label_set_text(info, "Proceed to full-screen\nStick Calibration Wizard?");
    lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(info, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_14, 0);
    lv_obj_set_style_pad_bottom(info, 20, 0);

    lv_obj_t *btn = lv_btn_create(cont);
    lv_obj_set_size(btn, 140, 45);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x00AAAA), 0);
    lv_obj_set_style_radius(btn, 4, 0);

    // Gắn sự kiện cảm ứng cho nút START
    lv_obj_add_event_cb(btn, btn_start_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "START WIZARD");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl);

    return cont;
}

static void page_cal_command(nav_event_t event) {
    if (event == NAV_SELECT) {
        ui_manager_show_calibration();
    }
}

menu_page_t page_calibration = {
    .title = "STICK CALIBRATION",
    .create = page_cal_create,
    .command = page_cal_command,
    .update = NULL,
    .destroy = NULL
};
