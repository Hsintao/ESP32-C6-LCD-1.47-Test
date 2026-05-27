#include "app_storage.h"

#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"

#define APP_NVS_NAMESPACE "app_cfg"
#define KEY_WIFI_SSID "wifi_ssid"
#define KEY_WIFI_PASS "wifi_pass"
#define KEY_LED_STATE "led_state"
#define KEY_LED_RGB "led_rgb"

esp_err_t app_storage_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

static bool nvs_read_string(nvs_handle_t nvs, const char *key, char *out, size_t out_len)
{
    size_t required = out_len;
    esp_err_t ret = nvs_get_str(nvs, key, out, &required);
    return ret == ESP_OK && out[0] != '\0';
}

bool app_storage_get_wifi(char *ssid, size_t ssid_len, char *pass, size_t pass_len)
{
    nvs_handle_t nvs;
    if (nvs_open(APP_NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return false;
    }

    bool ok = nvs_read_string(nvs, KEY_WIFI_SSID, ssid, ssid_len);
    if (ok) {
        size_t required = pass_len;
        esp_err_t ret = nvs_get_str(nvs, KEY_WIFI_PASS, pass, &required);
        if (ret == ESP_ERR_NVS_NOT_FOUND) {
            pass[0] = '\0';
        } else if (ret != ESP_OK) {
            ok = false;
        }
    }

    nvs_close(nvs);
    return ok;
}

esp_err_t app_storage_set_wifi(const char *ssid, const char *pass)
{
    nvs_handle_t nvs;
    esp_err_t ret = nvs_open(APP_NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_set_str(nvs, KEY_WIFI_SSID, ssid);
    if (ret == ESP_OK) {
        ret = nvs_set_str(nvs, KEY_WIFI_PASS, pass ? pass : "");
    }
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs);
    }

    nvs_close(nvs);
    return ret;
}

esp_err_t app_storage_clear_wifi(void)
{
    nvs_handle_t nvs;
    esp_err_t ret = nvs_open(APP_NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (ret != ESP_OK) {
        return ret;
    }

    esp_err_t ret_ssid = nvs_erase_key(nvs, KEY_WIFI_SSID);
    esp_err_t ret_pass = nvs_erase_key(nvs, KEY_WIFI_PASS);
    if (ret_ssid == ESP_ERR_NVS_NOT_FOUND) {
        ret_ssid = ESP_OK;
    }
    if (ret_pass == ESP_ERR_NVS_NOT_FOUND) {
        ret_pass = ESP_OK;
    }
    ret = ret_ssid == ESP_OK ? ret_pass : ret_ssid;
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs);
    }

    nvs_close(nvs);
    return ret;
}

bool app_storage_get_led_state(char *state, size_t state_len)
{
    nvs_handle_t nvs;
    if (nvs_open(APP_NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return false;
    }

    bool ok = nvs_read_string(nvs, KEY_LED_STATE, state, state_len);
    nvs_close(nvs);
    return ok;
}

esp_err_t app_storage_set_led_state(const char *state)
{
    nvs_handle_t nvs;
    esp_err_t ret = nvs_open(APP_NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_set_str(nvs, KEY_LED_STATE, state);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs);
    }

    nvs_close(nvs);
    return ret;
}

bool app_storage_get_led_rgb(uint8_t *r, uint8_t *g, uint8_t *b)
{
    nvs_handle_t nvs;
    if (nvs_open(APP_NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return false;
    }

    uint32_t rgb = 0;
    esp_err_t ret = nvs_get_u32(nvs, KEY_LED_RGB, &rgb);
    nvs_close(nvs);
    if (ret != ESP_OK) {
        return false;
    }

    *r = (uint8_t)((rgb >> 16) & 0xff);
    *g = (uint8_t)((rgb >> 8) & 0xff);
    *b = (uint8_t)(rgb & 0xff);
    return true;
}

esp_err_t app_storage_set_led_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    nvs_handle_t nvs;
    esp_err_t ret = nvs_open(APP_NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (ret != ESP_OK) {
        return ret;
    }

    uint32_t rgb = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    ret = nvs_set_u32(nvs, KEY_LED_RGB, rgb);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs);
    }

    nvs_close(nvs);
    return ret;
}
