set(srcs
    "ui_display.c"
    "src/porting/lv_port_disp.c"
    "src/porting/lv_port_indev.c"
    "src/core/ui_manager.c"
    "src/screens/scr_dashboard.c"
    "src/screens/scr_menu.h"
    "src/screens/scr_menu.c"
    "src/screens/scr_calibration.c"
    "src/screens/menu/pages/page_inputs.c"
    "src/screens/menu/pages/page_rf_link.c"
    "src/screens/menu/pages/page_fc_tune.c"
    "src/screens/menu/pages/page_system.c"
    "src/screens/menu/pages/page_calibration.c"
)

# Thêm đường dẫn tới main để ui_display có thể đọc được sys_state.h
idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "include" "src"
    PRIV_INCLUDE_DIRS "src/porting" "src/core" "src/screens" "src/screens/menu" "../../main"
    REQUIRES lvgl esp_lcd driver nvs_flash
)
