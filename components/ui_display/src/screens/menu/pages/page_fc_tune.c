#include "menu_pages.h"
#include <stdio.h>

static lv_obj_t *cont = NULL;
static lv_obj_t *list_pids;
static float pids[3][3] = {{0,0,0}, {0,0,0}, {0,0,0}};
static float pids_old[3][3] = {{-1,-1,-1}, {-1,-1,-1}, {-1,-1,-1}};
static int8_t cursor_idx = 0;
static int8_t last_cursor = -1;

static lv_obj_t* page_fc_tune_create(lv_obj_t *parent) {
    cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(cont, 0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);

    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "FC PID TUNING");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00FFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    list_pids = lv_list_create(cont);
    lv_obj_set_size(list_pids, 280, 140);
    lv_obj_align(list_pids, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_bg_color(list_pids, lv_color_hex(0x0A0A0A), 0);
    lv_obj_set_style_border_width(list_pids, 0, 0);

    const char* axes[] = {"ROLL", "PITCH", "YAW"};
    for(int i=0; i<3; i++) {
        lv_obj_t *btn = lv_list_add_btn(list_pids, NULL, axes[i]);
        lv_obj_set_style_bg_opa(btn, 0, 0); 
        lv_obj_set_style_outline_width(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_text_color(btn, lv_color_hex(0xCCCCCC), 0); // Brighter grey
        lv_obj_set_style_radius(btn, 0, 0);
    }
    
    return cont;
}

static void page_fc_tune_update(void) {
    if (!list_pids) return;
    const char* axes[] = {"ROLL", "PITCH", "YAW"};
    bool cursor_moved = (cursor_idx != last_cursor);
    
    for(int i=0; i<3; i++) {
        lv_obj_t *btn = lv_obj_get_child(list_pids, i);
        
        // --- 1. Content Refresh (Text) ---
        bool content_dirty = (pids[i][0] != pids_old[i][0] || 
                             pids[i][1] != pids_old[i][1] || 
                             pids[i][2] != pids_old[i][2]);
        
        if (content_dirty) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%s | P:%.1f I:%.2f D:%.2f", axes[i], pids[i][0], pids[i][1], pids[i][2]);
            lv_obj_t *label = NULL;
            for(int j = 0; j < lv_obj_get_child_cnt(btn); j++) {
                lv_obj_t *child = lv_obj_get_child(btn, j);
                if(lv_obj_check_type(child, &lv_label_class)) {
                    label = child;
                    break;
                }
            }
            if(label) lv_label_set_text(label, buf);
            
            pids_old[i][0] = pids[i][0];
            pids_old[i][1] = pids[i][1];
            pids_old[i][2] = pids[i][2];
        }

        // --- 2. Focus Refresh (Styles) ---
        if (cursor_moved) {
            if (i == cursor_idx) {
                // Marker Style: 6px cyan border on the left
                lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_LEFT, 0);
                lv_obj_set_style_border_width(btn, 6, 0);
                lv_obj_set_style_border_color(btn, lv_color_hex(0x00FFFF), 0);
                lv_obj_set_style_bg_opa(btn, 0, 0); 

                for(int j=0; j<lv_obj_get_child_cnt(btn); j++) {
                    lv_obj_t *child = lv_obj_get_child(btn, j);
                    if(lv_obj_check_type(child, &lv_label_class)) {
                        lv_obj_set_style_text_color(child, lv_color_hex(0xFFFFFF), 0);
                        break;
                    }
                }
            } else if (i == last_cursor || last_cursor == -1) {
                lv_obj_set_style_border_width(btn, 0, 0);
                lv_obj_set_style_bg_opa(btn, 0, 0);
                    for(int j=0; j<lv_obj_get_child_cnt(btn); j++) {
                        lv_obj_t *child = lv_obj_get_child(btn, j);
                        if(lv_obj_check_type(child, &lv_label_class)) {
                            lv_obj_set_style_text_color(child, lv_color_hex(0xCCCCCC), 0);
                            break;
                        }
                    }
            }
        }
    }
    last_cursor = cursor_idx;
}

static void page_fc_tune_command(nav_event_t event) {
    if (event == NAV_UP) {
        cursor_idx = (cursor_idx - 1 + 3) % 3;
    } else if (event == NAV_DOWN) {
        cursor_idx = (cursor_idx + 1) % 3;
    }
}

menu_page_t page_fc_tune = {
    .title = "FC Tuning (PID)",
    .create = page_fc_tune_create,
    .update = page_fc_tune_update,
    .command = page_fc_tune_command,
    .destroy = NULL
};
