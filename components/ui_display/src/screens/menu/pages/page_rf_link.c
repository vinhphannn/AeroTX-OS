#include "menu_pages.h"
#include <stdio.h>

static lv_obj_t *cont = NULL;
static lv_obj_t *list_rf;
static int8_t cursor_idx = 0;

static lv_obj_t* page_rf_link_create(lv_obj_t *parent) {
    cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(cont, 0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);

    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "RF LINK SETUP");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00FFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    list_rf = lv_list_create(cont);
    lv_obj_set_size(list_rf, 280, 140);
    lv_obj_align(list_rf, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_bg_color(list_rf, lv_color_hex(0x0A0A0A), 0);
    lv_obj_set_style_border_width(list_rf, 0, 0);

    const char* opts[] = {"CRSF CHANNEL", "RF POWER (PA)", "BLE MODE", "BINDING"};
    for(int i=0; i<4; i++) {
        lv_obj_t *btn = lv_list_add_btn(list_rf, NULL, opts[i]);
        lv_obj_set_style_bg_opa(btn, 0, 0);
        lv_obj_set_style_outline_width(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_text_color(btn, lv_color_hex(0xCCCCCC), 0);
        lv_obj_set_style_radius(btn, 0, 0);
    }
    
    return cont;
}

static int8_t last_cursor = -1;

static void page_rf_link_update(void) {
    if (!list_rf) return;
    if (cursor_idx == last_cursor) return;
    
    for(int i=0; i<4; i++) {
        lv_obj_t *btn = lv_obj_get_child(list_rf, i);
        if (i == cursor_idx) {
            lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_LEFT, 0);
            lv_obj_set_style_border_width(btn, 6, 0);
            lv_obj_set_style_border_color(btn, lv_color_hex(0x00FFFF), 0);
            lv_obj_set_style_bg_opa(btn, 0, 0); 
            lv_obj_set_style_text_color(btn, lv_color_hex(0xFFFFFF), 0);
        } else {
            lv_obj_set_style_border_width(btn, 0, 0);
            lv_obj_set_style_bg_opa(btn, 0, 0);
            lv_obj_set_style_text_color(btn, lv_color_hex(0xCCCCCC), 0);
        }
    }
    last_cursor = cursor_idx;
}

static void page_rf_link_command(nav_event_t event) {
    if (event == NAV_UP) {
        cursor_idx = (cursor_idx - 1 + 4) % 4;
    } else if (event == NAV_DOWN) {
        cursor_idx = (cursor_idx + 1) % 4;
    }
}

menu_page_t page_rf_link = {
    .title = "RF Link",
    .create = page_rf_link_create,
    .update = page_rf_link_update,
    .command = page_rf_link_command,
    .destroy = NULL
};
