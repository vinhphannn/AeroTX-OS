#include "scr_menu.h"
#include "menu/menu_pages.h"
#include <stdio.h>

static lv_obj_t *scr_menu;
static lv_obj_t *menu_cont;
static lv_obj_t *page_container;
static lv_obj_t *mbox_exit = NULL;
static lv_obj_t *title_label;
static lv_obj_t *btn_back;

static menu_page_t* pages[] = {&page_inputs, &page_rf_link, &page_fc_tune, &page_calibration, &page_system};
static const char* page_descs[] = {
    "Exponential, Trims & Inversion",
    "NRF24 Frequency & Protocol",
    "Adjust PIDs & Flight Filters",
    "Set sticks Min, Max & Neutral",
    "Device Info & LCD Settings"
};

#define PAGE_COUNT (sizeof(pages)/sizeof(pages[0]))

static menu_page_t* active_page = NULL;
static lv_obj_t* cards[PAGE_COUNT];

static void set_text_sharp(lv_obj_t *obj, lv_color_t color, const lv_font_t *font) {
    lv_obj_set_style_text_color(obj, color, 0);
    if (font) lv_obj_set_style_text_font(obj, font, 0);
}

// Cập nhật trạng thái hiển thị nút Back
static void update_header_ui(void) {
    if (active_page == NULL) {
        lv_obj_add_flag(btn_back, LV_OBJ_FLAG_HIDDEN); // Ở menu chính dẹp nút Back
        lv_label_set_text(title_label, "SYSTEM SETTINGS");
        lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 0);
    } else {
        lv_obj_clear_flag(btn_back, LV_OBJ_FLAG_HIDDEN); // Ở trang con thì hiện nút Back
        lv_label_set_text(title_label, active_page->title);
        lv_obj_align(title_label, LV_ALIGN_CENTER, 20, 0);
    }
}

static void card_event_cb(lv_event_t * e) {
    intptr_t idx = (intptr_t)lv_event_get_user_data(e);

    // Nếu là trang Calibration (Index 3), nhảy thẳng vào màn hình Calib toàn cảnh
    if (idx == 3) {
        extern void ui_manager_show_calibration(void);
        ui_manager_show_calibration();
        return;
    }

    if (active_page == NULL) {
        active_page = pages[idx];
        lv_obj_add_flag(menu_cont, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(page_container, LV_OBJ_FLAG_HIDDEN);
        active_page->create(page_container);
        update_header_ui();
    }
}

static void back_btn_event_cb(lv_event_t * e) {
    if (active_page) {
        if (active_page->destroy) active_page->destroy();
        active_page = NULL;
        lv_obj_clean(page_container);
        lv_obj_add_flag(page_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(menu_cont, LV_OBJ_FLAG_HIDDEN);
        update_header_ui();
    }
}

lv_obj_t* scr_menu_create(void) {
    scr_menu = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_menu, lv_color_hex(0x000000), 0);

    lv_obj_t *header = lv_obj_create(scr_menu);
    lv_obj_set_size(header, 320, 40);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(header, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    btn_back = lv_btn_create(header);
    lv_obj_set_size(btn_back, 60, 30);
    lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x222222), 0);
    lv_obj_add_event_cb(btn_back, back_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *bl = lv_label_create(btn_back);
    lv_label_set_text(bl, LV_SYMBOL_LEFT " BACK");
    lv_obj_center(bl);

    title_label = lv_label_create(header);
    set_text_sharp(title_label, lv_color_hex(0x00FFFF), &lv_font_montserrat_14);

    menu_cont = lv_obj_create(scr_menu);
    lv_obj_set_size(menu_cont, 320, 200);
    lv_obj_align(menu_cont, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_bg_opa(menu_cont, 0, 0);
    lv_obj_set_style_border_width(menu_cont, 0, 0);
    lv_obj_set_flex_flow(menu_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(menu_cont, 8, 0);

    for(int i=0; i<PAGE_COUNT; i++) {
        cards[i] = lv_obj_create(menu_cont);
        lv_obj_set_size(cards[i], lv_pct(100), 48);
        lv_obj_set_style_bg_color(cards[i], lv_color_hex(0x0D0D0D), 0);
        lv_obj_set_style_border_color(cards[i], lv_color_hex(0x333333), 0);
        lv_obj_set_style_border_width(cards[i], 1, 0);
        lv_obj_set_style_bg_color(cards[i], lv_color_hex(0x003333), LV_STATE_PRESSED);
        lv_obj_clear_flag(cards[i], LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *ltitle = lv_label_create(cards[i]);
        lv_label_set_text(ltitle, pages[i]->title);
        set_text_sharp(ltitle, lv_color_hex(0xFFFFFF), &lv_font_montserrat_14);
        lv_obj_align(ltitle, LV_ALIGN_TOP_LEFT, 5, -2);

        lv_obj_t *lsub = lv_label_create(cards[i]);
        lv_label_set_text(lsub, page_descs[i]);
        set_text_sharp(lsub, lv_color_hex(0x777777), &lv_font_montserrat_14);
        lv_obj_align(lsub, LV_ALIGN_TOP_LEFT, 5, 16);

        lv_obj_add_event_cb(cards[i], card_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    page_container = lv_obj_create(scr_menu);
    lv_obj_set_size(page_container, 320, 240);
    lv_obj_align(page_container, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(page_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(page_container, 0, 0);
    lv_obj_add_flag(page_container, LV_OBJ_FLAG_HIDDEN);

    scr_menu_reset_state();
    return scr_menu;
}

void scr_menu_reset_state(void) {
    if (active_page && active_page->destroy) active_page->destroy();
    active_page = NULL;
    lv_obj_clean(page_container);
    lv_obj_add_flag(page_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(menu_cont, LV_OBJ_FLAG_HIDDEN);
    update_header_ui();
    if (mbox_exit) { lv_obj_del(mbox_exit); mbox_exit = NULL; }
}

void scr_menu_command(nav_event_t event, void (*on_exit)(void)) {
    if (mbox_exit) {
        if (event == NAV_SELECT) {
             lv_obj_del(mbox_exit); mbox_exit = NULL;
             if (on_exit) on_exit();
             return;
        } else if (event == NAV_BACK) {
             lv_obj_del(mbox_exit); mbox_exit = NULL;
        }
        return;
    }

    if (event == NAV_BACK) {
        if (active_page) {
             back_btn_event_cb(NULL);
        } else {
             // Mở mbox thoát ở menu chính
             static const char * btns[] = {"Stay", "Exit", ""};
             mbox_exit = lv_msgbox_create(lv_layer_top(), "Exit Menu?", "", btns, false);
             lv_obj_set_style_bg_color(mbox_exit, lv_color_hex(0x111111), 0);
             lv_obj_center(mbox_exit);
        }
        return;
    }

    if (active_page && active_page->command) active_page->command(event);
    if (active_page && active_page->update) active_page->update();
}

void scr_menu_update_pid(uint8_t part, float v1, float v2, float v3) {
    if (active_page == &page_fc_tune) active_page->update();
}
uint8_t scr_menu_get_index(void) { return 0; }
uint8_t scr_menu_get_pid_field(void) { return 0; }
