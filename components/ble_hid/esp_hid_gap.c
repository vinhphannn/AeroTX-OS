/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <string.h>
#include <stdbool.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_hid_gap.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"

static const char *TAG = "ESP_HID_GAP";
static bool s_ble_init = false;

static SemaphoreHandle_t ble_hidh_cb_semaphore = NULL;
#define WAIT_BLE_CB() xSemaphoreTake(ble_hidh_cb_semaphore, pdMS_TO_TICKS(5000))
#define SEND_BLE_CB() xSemaphoreGive(ble_hidh_cb_semaphore)

// Dummy function if not implemented in project
__attribute__((weak)) void ble_hid_task_start_up(void) {
    // Optional: can be implemented in ble_hid.c
}

static void ble_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        SEND_BLE_CB();
        break;
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
        ESP_LOGI(TAG, "BLE GAP ADV/SCAN_RSP DATA SET COMPLETE");
        SEND_BLE_CB();
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        ESP_LOGI(TAG, "BLE GAP ADV_START_COMPLETE");
        break;
    case ESP_GAP_BLE_SEC_REQ_EVT:
        for(int i = 0; i < ESP_BD_ADDR_LEN; i++) {
            ESP_LOGD(TAG, "%x:",param->ble_security.ble_req.bd_addr[i]);
        }
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(TAG, "BLE GAP UPDATE_CONN_PARAMS status:%d, min_int:%d, max_int:%d,conn_int:%d,latency:%d, timeout:%d",
                 param->update_conn_params.status,
                 param->update_conn_params.min_int,
                 param->update_conn_params.max_int,
                 param->update_conn_params.conn_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
        if (!param->ble_security.auth_cmpl.success) {
            ESP_LOGE(TAG, "BLE GAP AUTH ERROR: 0x%x", param->ble_security.auth_cmpl.fail_reason);
        } else {
            ESP_LOGI(TAG, "BLE GAP AUTH SUCCESS");
            // Request connection parameter update for better responsiveness/stability
            esp_ble_conn_update_params_t conn_params = {0};
            memcpy(conn_params.bda, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
            conn_params.latency = 0;
            conn_params.max_int = 0x000C;    // 15ms
            conn_params.min_int = 0x0009;    // 11.25ms
            conn_params.timeout = 400;       // 4s
            esp_ble_gap_update_conn_params(&conn_params);
        }
        ble_hid_task_start_up();
        break;
    default:
        break;
    }
}

static esp_err_t init_ble_gap(void)
{
    return esp_ble_gap_register_callback(ble_gap_event_handler);
}

esp_err_t esp_hid_gap_init(uint8_t mode)
{
    esp_err_t ret;
    if (s_ble_init) return ESP_OK;
    
    if (ble_hidh_cb_semaphore == NULL) {
        ble_hidh_cb_semaphore = xSemaphoreCreateBinary();
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret && ret != ESP_ERR_INVALID_STATE) return ret;

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret && ret != ESP_ERR_INVALID_STATE) return ret;

    ret = esp_bluedroid_init();
    if (ret && ret != ESP_ERR_INVALID_STATE) return ret;

    ret = esp_bluedroid_enable();
    if (ret && ret != ESP_ERR_INVALID_STATE) return ret;

    ret = init_ble_gap();
    if (ret) return ret;

    /* set the security parameters */
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_BOND; // Secure Connection + Bonding
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;       // No Input No Output
    uint8_t key_size = 16;                          
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

    esp_ble_gatt_set_local_mtu(512);

    s_ble_init = true;
    return ESP_OK;
}

esp_err_t esp_hid_gap_deinit(void)
{
    if (!s_ble_init) return ESP_OK;

    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();

    s_ble_init = false;
    return ESP_OK;
}

esp_err_t esp_hid_ble_gap_adv_start(void)
{
    static esp_ble_adv_params_t hidd_adv_params = {
        .adv_int_min        = 0x20,
        .adv_int_max        = 0x30,
        .adv_type           = ADV_TYPE_IND,
        .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
        .channel_map        = ADV_CHNL_ALL,
        .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    };
    return esp_ble_gap_start_advertising(&hidd_adv_params);
}

void esp_hid_ble_gap_wait_set_complete(void)
{
    WAIT_BLE_CB();
}
