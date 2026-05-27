#include "app_led_state.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "RGB.h"
#include "app_storage.h"

static char current_state[APP_LED_STATE_MAX_LEN] = "off";
static uint8_t current_r;
static uint8_t current_g;
static uint8_t current_b;

static bool is_valid_state(const char *state)
{
    return strcmp(state, "red") == 0 ||
           strcmp(state, "green") == 0 ||
           strcmp(state, "off") == 0;
}

static void update_state_name(void)
{
    if (current_r == 255 && current_g == 0 && current_b == 0) {
        strlcpy(current_state, "red", sizeof(current_state));
    } else if (current_r == 0 && current_g == 255 && current_b == 0) {
        strlcpy(current_state, "green", sizeof(current_state));
    } else if (current_r == 0 && current_g == 0 && current_b == 0) {
        strlcpy(current_state, "off", sizeof(current_state));
    } else {
        snprintf(current_state, sizeof(current_state), "#%02X%02X%02X", current_r, current_g, current_b);
    }
}

void app_led_state_init(void)
{
    if (app_storage_get_led_rgb(&current_r, &current_g, &current_b)) {
        update_state_name();
        RGB_SetSolid(current_r, current_g, current_b);
        return;
    }

    char saved[APP_LED_STATE_MAX_LEN] = {0};
    if (app_storage_get_led_state(saved, sizeof(saved)) && is_valid_state(saved)) {
        app_led_state_set(saved, false);
    } else {
        app_led_state_set("off", false);
    }
}

const char *app_led_state_get(void)
{
    return current_state;
}

esp_err_t app_led_state_set(const char *state, bool persist)
{
    if (!state || !is_valid_state(state)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (strcmp(state, "red") == 0) {
        return app_led_state_set_rgb(255, 0, 0, persist);
    }
    if (strcmp(state, "green") == 0) {
        return app_led_state_set_rgb(0, 255, 0, persist);
    }
    return app_led_state_set_rgb(0, 0, 0, persist);
}

void app_led_state_get_rgb(uint8_t *r, uint8_t *g, uint8_t *b)
{
    *r = current_r;
    *g = current_g;
    *b = current_b;
}

esp_err_t app_led_state_set_rgb(uint8_t r, uint8_t g, uint8_t b, bool persist)
{
    current_r = r;
    current_g = g;
    current_b = b;
    update_state_name();
    RGB_SetSolid(current_r, current_g, current_b);

    if (!persist) {
        return ESP_OK;
    }

    esp_err_t ret = app_storage_set_led_rgb(current_r, current_g, current_b);
    if (ret == ESP_OK) {
        ret = app_storage_set_led_state(current_state);
    }
    return ret;
}
