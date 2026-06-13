#include "core/sys_state.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "STORAGE";

esp_err_t storage_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

esp_err_t storage_save_calib(CalibData_t *data) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(handle, "calib", data, sizeof(CalibData_t));
    if (err == ESP_OK) nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "Calibration Saved.");
    return err;
}

esp_err_t storage_load_calib(CalibData_t *data) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &handle);
    if (err != ESP_OK) return err;

    size_t size = sizeof(CalibData_t);
    err = nvs_get_blob(handle, "calib", data, &size);
    nvs_close(handle);
    return err;
}

esp_err_t storage_save_model(ModelConfig_t *model) {
    nvs_handle_t handle;
    char key[16];
    snprintf(key, sizeof(key), "model_%d", model->id);

    esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(handle, key, model, sizeof(ModelConfig_t));
    if (err == ESP_OK) nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "Model %d (%s) Saved.", model->id, model->name);
    return err;
}

esp_err_t storage_load_model(uint8_t id, ModelConfig_t *model) {
    nvs_handle_t handle;
    char key[16];
    snprintf(key, sizeof(key), "model_%d", id);

    esp_err_t err = nvs_open("storage", NVS_READONLY, &handle);
    if (err != ESP_OK) return err;

    size_t size = sizeof(ModelConfig_t);
    err = nvs_get_blob(handle, key, model, &size);
    nvs_close(handle);
    return err;
}
