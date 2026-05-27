#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

void app_led_state_init(void);
const char *app_led_state_get(void);
esp_err_t app_led_state_set(const char *state, bool persist);
void app_led_state_get_rgb(uint8_t *r, uint8_t *g, uint8_t *b);
esp_err_t app_led_state_set_rgb(uint8_t r, uint8_t g, uint8_t b, bool persist);
void app_led_state_apply_codex_status(const char *status);
