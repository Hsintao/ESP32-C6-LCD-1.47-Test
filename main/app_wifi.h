#pragma once

#include <stdbool.h>
#include "esp_err.h"

#define APP_CONFIG_AP_SSID "ESP32-C6-SETUP"
#define APP_CONFIG_AP_PASS "12345678"

esp_err_t app_wifi_start(void);
esp_err_t app_wifi_set_credentials_and_connect(const char *ssid, const char *pass);
bool app_wifi_is_connected(void);
const char *app_wifi_get_ip(void);
const char *app_wifi_get_ssid(void);
