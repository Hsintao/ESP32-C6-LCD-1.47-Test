#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define APP_WIFI_SSID_MAX_LEN 32
#define APP_WIFI_PASS_MAX_LEN 64
#define APP_LED_STATE_MAX_LEN 16

esp_err_t app_storage_init(void);
bool app_storage_get_wifi(char *ssid, size_t ssid_len, char *pass, size_t pass_len);
esp_err_t app_storage_set_wifi(const char *ssid, const char *pass);
esp_err_t app_storage_clear_wifi(void);
bool app_storage_get_led_state(char *state, size_t state_len);
esp_err_t app_storage_set_led_state(const char *state);
bool app_storage_get_led_rgb(uint8_t *r, uint8_t *g, uint8_t *b);
esp_err_t app_storage_set_led_rgb(uint8_t r, uint8_t g, uint8_t b);
