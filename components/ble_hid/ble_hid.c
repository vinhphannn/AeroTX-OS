#include "ble_hid.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "store/config/ble_store_config.h"

void ble_store_config_init(void);

static const char *TAG = "BLE_NIMBLE_HID";

// --- HID REPORT DESCRIPTOR ---
// 4 Axes, 8 Buttons (9 Bytes Payload + 1 Byte Report ID = 10 Bytes total)
static const uint8_t hid_report_desc[] = {
    0x05, 0x01,        //   Usage Page (Generic Desktop)
    0x09, 0x05,        //   Usage (Gamepad)
    0xA1, 0x01,        //   Collection (Application)
    
    0x85, 0x01,        //   Report ID (1)
    
    0x05, 0x01,        //   Usage Page (Generic Desktop)
    0xA1, 0x00,        //   Collection (Physical)
    0x09, 0x30,        //     Usage (X) -> Roll
    0x09, 0x31,        //     Usage (Y) -> Pitch
    0x09, 0x32,        //     Usage (Z) -> Yaw
    0x09, 0x35,        //     Usage (Rz) -> Throttle
    0x15, 0x00,        //     Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00, // Logical Maximum (65535)
    0x75, 0x10,        //     Report Size (16 bits)
    0x95, 0x04,        //     Report Count (4 axes)
    0x81, 0x02,        //     Input (Data, Variable, Absolute)
    0xC0,              //   End Collection
    
    // 8 Buttons
    0x05, 0x09,        //   Usage Page (Button)
    0x19, 0x01,        //   Usage Minimum (Button 1)
    0x29, 0x08,        //   Usage Maximum (Button 8)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x08,        //   Report Count (8)
    0x81, 0x02,        //   Input (Data, Variable, Absolute)
    
    0xC0               // End Collection
};

typedef struct {
    uint8_t  report_id; // Match 0x85, 0x01
    uint16_t roll;      // Usage 0x30 (X)
    uint16_t pitch;     // Usage 0x31 (Y)
    uint16_t yaw;       // Usage 0x32 (Z)
    uint16_t throttle;  // Usage 0x35 (Rz)
    uint8_t  buttons;   // 8 bits
} __attribute__((packed)) gamepad_report_t;

static volatile bool   s_is_connected = false;
static volatile bool   s_is_subscribed = false;
static uint32_t        s_conn_time = 0;
static uint16_t        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t        s_hid_report_handle;
static bool            s_ble_initialized = false;
static uint8_t         s_battery_level = 100;

// GATT Access Callbacks
static int gatt_svr_chr_access_hid(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg);
static int gatt_svr_chr_access_batt(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg);
static int gatt_svr_chr_access_devinfo(uint16_t conn_handle, uint16_t attr_handle,
                                       struct ble_gatt_access_ctxt *ctxt, void *arg);

static int ble_hid_gap_event(struct ble_gap_event *event, void *arg);

// HID Info (bcdHID, bCountryCode, Flags)
static const uint8_t hid_info[] = {0x11, 0x01, 0x00, 0x01};
static uint8_t hid_control_point = 0;
// Report Reference: Report ID 1, Input Report (1)
static const uint8_t hid_report_ref[] = {0x01, 0x01};

// PnP ID: Vendor ID Source (2=USB), Vendor ID, Product ID, Product Version
static const uint8_t pnp_id[] = {
    0x02, // Vendor ID Source: 0x02 (USB Implementer's Forum)
    0x5E, 0x04, // Vendor ID: 0x045E (Microsoft Corporation)
    0x8E, 0x02, // Product ID: 0x028E (Xbox 360 Controller)
    0x10, 0x01  // Product Version: 0x0110 (1.1.0)
};

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        // Device Information Service
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(0x180A),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = BLE_UUID16_DECLARE(0x2A50), // PnP ID
                .access_cb = gatt_svr_chr_access_devinfo,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
            },
            {
                .uuid = BLE_UUID16_DECLARE(0x2A29), // Manufacturer Name
                .access_cb = gatt_svr_chr_access_devinfo,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
            },
            { 0 }
        }
    },
    {
        // Battery Service
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(0x180F),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = BLE_UUID16_DECLARE(0x2A19),
                .access_cb = gatt_svr_chr_access_batt,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_NOTIFY,
            },
            { 0 }
        }
    },
    {
        // Human Interface Device Service
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(0x1812),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = BLE_UUID16_DECLARE(0x2A4A), // HID Information
                .access_cb = gatt_svr_chr_access_hid,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
            },
            {
                .uuid = BLE_UUID16_DECLARE(0x2A4B), // Report Map
                .access_cb = gatt_svr_chr_access_hid,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
            },
            {
                .uuid = BLE_UUID16_DECLARE(0x2A4C), // HID Control Point
                .access_cb = gatt_svr_chr_access_hid,
                .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                .uuid = BLE_UUID16_DECLARE(0x2A4D), // Report
                .access_cb = gatt_svr_chr_access_hid,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_hid_report_handle,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    {
                        .uuid = BLE_UUID16_DECLARE(0x2908), // Report Reference
                        .access_cb = gatt_svr_chr_access_hid,
                        .att_flags = BLE_ATT_F_READ | BLE_ATT_F_READ_ENC,
                    },
                    { 0 }
                }
            },
            { 0 }
        }
    },
    { 0 }
};

static int gatt_svr_chr_access_devinfo(uint16_t conn_handle, uint16_t attr_handle,
                                       struct ble_gatt_access_ctxt *ctxt, void *arg) {
    uint16_t uuid = ble_uuid_u16(ctxt->chr->uuid);
    if (uuid == 0x2A50) {
        int rc = os_mbuf_append(ctxt->om, pnp_id, sizeof(pnp_id));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    } else if (uuid == 0x2A29) {
        const char *manuf = "Zynh Drone";
        int rc = os_mbuf_append(ctxt->om, manuf, strlen(manuf));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    } else if (uuid == 0x2A29) {
        const char *manuf = "Zynh Drone";
        int rc = os_mbuf_append(ctxt->om, manuf, strlen(manuf));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    } 
    return BLE_ATT_ERR_UNLIKELY;
}

static int gatt_svr_chr_access_batt(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ble_uuid_u16(ctxt->chr->uuid) == 0x2A19) {
        int rc = os_mbuf_append(ctxt->om, &s_battery_level, 1);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static int gatt_svr_chr_access_hid(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg) {
    uint16_t uuid;
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC) {
        uuid = ble_uuid_u16(ctxt->dsc->uuid);
        if (uuid == 0x2908) {
            os_mbuf_append(ctxt->om, hid_report_ref, sizeof(hid_report_ref));
            return 0;
        }
    } else {
        uuid = ble_uuid_u16(ctxt->chr->uuid);
    }

    if (uuid == 0x2A4A) {
        os_mbuf_append(ctxt->om, hid_info, sizeof(hid_info));
        return 0;
    } else if (uuid == 0x2A4B) {
        os_mbuf_append(ctxt->om, hid_report_desc, sizeof(hid_report_desc));
        return 0;
    } else if (uuid == 0x2A4C) {
        struct os_mbuf *om = ctxt->om;
        if (OS_MBUF_PKTLEN(om) == 1) {
            os_mbuf_copydata(om, 0, 1, &hid_control_point);
        }
        return 0;
    } else if (uuid == 0x2A4D) { // Report
        // Read logic (Host poll)
        gamepad_report_t r = {0};
        r.report_id = 1;
        os_mbuf_append(ctxt->om, &r, sizeof(r));
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static void ble_hid_advertise(void) {
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    struct ble_hs_adv_fields rsp_fields;
    int rc;

    memset(&fields, 0, sizeof fields);
    
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    
    // Appearance 0x03C4 (Gamepad) thay vì 0x03C3 (Joystick) theo chuẩn Microsoft!
    fields.appearance = 0x03C4;
    fields.appearance_is_present = 1;

    fields.uuids16 = (ble_uuid16_t[]) {
        BLE_UUID16_INIT(0x1812), // HID
        BLE_UUID16_INIT(0x180F)  // Battery
    };
    fields.num_uuids16 = 2;
    fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error setting adv fields: %d", rc);
    }

    memset(&rsp_fields, 0, sizeof rsp_fields);
    rsp_fields.name = (uint8_t *)"ELRS TX01 Joystick";
    rsp_fields.name_len = strlen("ELRS TX01 Joystick");
    rsp_fields.name_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error setting scan rsp fields: %d", rc);
    }

    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; 
    
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_hid_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error enabling advertising: %d", rc);
    }
}

static int ble_hid_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "BLE CONNECT");
            if (event->connect.status == 0) {
                s_is_connected = true;
                s_is_subscribed = false; // Bắt buộc đợi Host Đăng ký CCCD mới được gửi!
                s_conn_time = xTaskGetTickCount();
                s_conn_handle = event->connect.conn_handle;
            } else {
                ble_hid_advertise();
            }
            break;
            
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "BLE DISCONNECT, rsn %d", event->disconnect.reason);
            s_is_connected = false;
            s_is_subscribed = false;
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            break;
            
        case BLE_GAP_EVENT_ENC_CHANGE:
            ESP_LOGI(TAG, "Encryption change: status=%d", event->enc_change.status);
            break;
            
        case BLE_GAP_EVENT_SUBSCRIBE:
            ESP_LOGI(TAG, "Subscribe event: attr_handle=%d, notify=%d", 
                     event->subscribe.attr_handle, event->subscribe.cur_notify);
            if (event->subscribe.attr_handle == s_hid_report_handle) {
                s_is_subscribed = event->subscribe.cur_notify;
                if (s_is_subscribed) {
                    ESP_LOGI(TAG, ">>> WINDOWS SUBSCRIBED: HID DATA STREAM STARTING! <<<");
                } else {
                    ESP_LOGI(TAG, ">>> WINDOWS UNSUBSCRIBED: DATA STREAM PAUSED. <<<");
                }
            }
            break;
    }
    return 0;
}

static void ble_hid_on_reset(int reason) {
    ESP_LOGE(TAG, "Resetting state; reason=%d", reason);
}

static void ble_hid_on_sync(void) {
    int rc;

    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    // Just Works Pairing (Mặc định ổn định nhất cho Windows)
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0; // Tắt MITM để không bị đòi xác nhận trên Windows
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO; 
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    // Set Default Tên Thiết Bị
    ble_svc_gap_device_name_set("ELRS TX01 Gamepad");
    ble_svc_gap_device_appearance_set(0x03C4); // Gamepad

    ble_hid_advertise();
}

void nimble_host_task(void *param) {
    ESP_LOGI(TAG, "NimBLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ble_hid_init(void) {
    if (s_ble_initialized) {
        ble_hid_advertise();
        ESP_LOGI(TAG, "BLE HID Resumed Advertising");
        return;
    }
    
    // Tắt các Log rác (Spam) của NimBLE (Nguyên nhân gây freeze Core do nghẽn UART)
    esp_log_level_set("NimBLE", ESP_LOG_WARN);

    // NimBLE INIT
    nimble_port_init();
    
    ble_hs_cfg.reset_cb = ble_hid_on_reset;
    ble_hs_cfg.sync_cb = ble_hid_on_sync;
    ble_hs_cfg.gatts_register_cb = NULL;
    
    // Đăng ký dịch vụ GATTS
    int rc = ble_gatts_count_cfg(gatt_svr_svcs);
    assert(rc == 0);
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    assert(rc == 0);

    // Thiết lập GAP & GATT Service tiêu chuẩn
    ble_svc_gap_init(); 
    ble_svc_gatt_init();

    // Khởi tạo Keystore NVS để lưu Bond (Bắt buộc cho Windows 11)
    ble_store_config_init();

    // Đẩy GAP Event Handle
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    
    // PIN NIMBLE TO CORE 0: Tránh làm treo UI Task trên Core 1
    xTaskCreatePinnedToCore(nimble_host_task, "ble_host", 4096, NULL, 15, NULL, 0);
    
    s_ble_initialized = true;
    ESP_LOGI(TAG, "BLE HID Initialized & NimBLE Core Booted");
}

void ble_hid_deinit(void) {
    if (!s_ble_initialized) return;

    // Thay vì hủy NimBLE Core (Rủi ro rò rỉ bộ nhớ), chúng ta chỉ Cúp sóng (Stop Adv)
    if (s_is_connected) {
        ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
    ble_gap_adv_stop();
    s_is_connected = false;
    
    ESP_LOGI(TAG, "BLE HID Advertising Suspended (Stack Kept Alive)");
}

bool ble_hid_is_connected(void) {
    return s_is_connected;
}

void ble_hid_send_report(uint16_t throttle, uint16_t yaw, uint16_t pitch, uint16_t roll, uint8_t buttons) {
    if (!s_is_connected || !s_is_subscribed) return;

    uint32_t now = xTaskGetTickCount();
    if ((now - s_conn_time) < pdMS_TO_TICKS(1500)) return;

    // Scale 1000-2000 to 0-65535 (16-bit unsigned)
    uint16_t r = (roll > 1000)     ? (uint16_t)(((uint32_t)(roll - 1000) * 65535) / 1000) : 0;
    uint16_t p = (pitch > 1000)    ? (uint16_t)(((uint32_t)(pitch - 1000) * 65535) / 1000) : 0;
    uint16_t y = (yaw > 1000)      ? (uint16_t)(((uint32_t)(yaw - 1000) * 65535) / 1000) : 0;
    uint16_t t = (throttle > 1000) ? (uint16_t)(((uint32_t)(throttle - 1000) * 65535) / 1000) : 0;

    static uint32_t last_send_time = 0;
    if ((now - last_send_time) < pdMS_TO_TICKS(10)) return; // Optimized 100Hz Signal!
    last_send_time = now;

    gamepad_report_t report;
    report.report_id = 1;
    report.roll = r;     // X
    report.pitch = p;    // Y
    report.yaw = y;      // Z
    report.throttle = t; // Rz
    report.buttons = buttons;

    struct os_mbuf *om = ble_hs_mbuf_from_flat(&report, sizeof(report));
    if (om) {
        int rc = ble_gatts_notify_custom(s_conn_handle, s_hid_report_handle, om);
        if (rc == 0) {
            // Success
        }
    }
}
