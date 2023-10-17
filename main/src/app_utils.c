
//
// Created by asus on ۱۰/۰۵/۲۰۲۱.
//
#ifndef APP_UTILS_H
#define APP_UTILS_H

#include <string.h>
#include <stdlib.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include <sodium/crypto_hash_sha256.h>
#include <connect.h>

#include <coap3/coap.h>
#include "app_utils.h"

static const char *NVS_SPACE = "customer data";
static const char *KEY = "UP";
extern struct production_struct device_keys;

#define HW_MODEL    "model"
#define HW_SERIAL   "serial"
#define FIRMWARE    "firmware"
#define APPLICATION "application"
#define AESKEY      "aeskey"
#define IDENTITY    "identity"
#define MFG_PARTITION_NAME "fac_set"
#define MFG_NAMESPACE "setup"

#define READ_NVS(KEY, b) \
    ESP_ERROR_CHECK(nvs_get_str(fac_handle, KEY, NULL, &len)); \
    memset(buff, 0, sizeof(buff)); \
    ESP_ERROR_CHECK(nvs_get_str(fac_handle, KEY, buff, &len)); \
    memcpy(b, buff, len < sizeof(b) - 1 ? len : sizeof(b) - 1 )

#define TAG "app_utils"

static esp_err_t load_cfg(nvs_sec_cfg_t *cfg)
{
    esp_err_t ret = ESP_FAIL;
    const esp_partition_t *key_part = esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS, NULL);
    if (key_part == NULL) {
        ESP_LOGE(TAG, "CONFIG_NVS_ENCRYPTION is enabled, but no partition with subtype nvs_keys found in the partition table.");
        return ret;
    }

    ret = nvs_flash_read_security_cfg(key_part, cfg);
    if (ret == ESP_ERR_NVS_KEYS_NOT_INITIALIZED) {
        ESP_LOGI(TAG, "NVS key partition empty, generating keys");
        ret = nvs_flash_generate_keys(key_part, cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to generate keys: [0x%02X] (%s)", ret, esp_err_to_name(ret));
            return ret;
        }
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read NVS security cfg: [0x%02X] (%s)", ret, esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

void config_load_mfg_keys_from_factory() {
    char buff[513];
    uint32_t len = 0;
    nvs_handle fac_handle;
    memset(&device_keys, 0x0, sizeof(device_keys));
    nvs_sec_cfg_t cfg = {};
    ESP_ERROR_CHECK(load_cfg(&cfg));
    ESP_ERROR_CHECK(nvs_flash_secure_init_partition(MFG_PARTITION_NAME, &cfg));
    ESP_LOGI(TAG, "NVS partition \"%s\" is encrypted.", MFG_PARTITION_NAME);
    ESP_ERROR_CHECK(nvs_open_from_partition(MFG_PARTITION_NAME, MFG_NAMESPACE, NVS_READONLY, &fac_handle));

    READ_NVS(HW_MODEL, device_keys.model);
    READ_NVS(HW_SERIAL, device_keys.serial);
    READ_NVS(FIRMWARE, device_keys.firmware);
    READ_NVS(APPLICATION, device_keys.application);
    READ_NVS(AESKEY, device_keys.aeskey);
    READ_NVS(IDENTITY, device_keys.identity);
    nvs_close(fac_handle);
}
uint8_t *crptoHash256(const char * s, size_t inlen)
{
    static uint8_t calculated[32];
    crypto_hash_sha256(calculated, (const uint8_t *) s, inlen);
    return calculated;
}

void config_load_connection_data_from_flash(user_pass_t *sta) {
    nvs_handle handle;
    uint32_t len = sizeof(user_pass_t);
    memset(sta, 0x0, len);

    esp_err_t err = nvs_open(NVS_SPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return;
        //      printf("nvs namespace not found\n");
/*        ESP_ERROR_CHECK(err);
        return;*/
    }
    if (nvs_get_blob(handle, KEY, sta, &len) != ESP_OK) {
        nvs_close(handle);
        return;
    }

    char p[100];
    memset(p, 0, 100);
    len = 10;
    nvs_get_str(handle, "TEST", NULL, &len);
    nvs_close(handle);
}

void write_connection_data_to_flash(user_pass_t *info) {
    nvs_handle handle;
    ESP_ERROR_CHECK(nvs_open(NVS_SPACE, NVS_READWRITE, &handle));
    ESP_ERROR_CHECK(nvs_set_blob(handle, KEY, info, sizeof(user_pass_t)));
    ESP_ERROR_CHECK(nvs_commit(handle));
    nvs_close(handle);
}

//todo: completely erase and recreate partition
void erase_connection_data_from_flash() {
    nvs_handle handle;
    ESP_ERROR_CHECK(nvs_open(NVS_SPACE, NVS_READWRITE, &handle));
    ESP_ERROR_CHECK(nvs_erase_all(handle));
    ESP_ERROR_CHECK(nvs_commit(handle));
    nvs_close(handle);
}

#endif // APP_UTILS_H