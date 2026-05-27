#include "app_led_state.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "RGB.h"
#include "app_storage.h"

#define CODEX_STATUS_TIMEOUT_MS 30000

static char current_state[APP_LED_STATE_MAX_LEN] = "off";
static uint8_t current_r;
static uint8_t current_g;
static uint8_t current_b;
static bool codex_led_enabled;
static TickType_t codex_status_tick;
static TaskHandle_t codex_led_task_handle;
static bool codex_last_offline;
static bool codex_offline_known;

static void apply_led_output(void);
static void apply_codex_led_effect(bool offline);

static void codex_led_task(void *arg)
{
    (void)arg;

    while (1) {
        if (codex_led_enabled) {
            TickType_t now = xTaskGetTickCount();
            bool offline = (now - codex_status_tick) > pdMS_TO_TICKS(CODEX_STATUS_TIMEOUT_MS);
            if (!codex_offline_known || offline != codex_last_offline) {
                codex_last_offline = offline;
                codex_offline_known = true;
                apply_codex_led_effect(offline);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void ensure_codex_led_task(void)
{
    if (codex_led_task_handle == NULL) {
        xTaskCreate(codex_led_task, "codex_led", 3072, NULL, 3, &codex_led_task_handle);
    }
}

static void apply_codex_led_effect(bool offline)
{
    if (offline) {
        RGB_StartPulse(255, 48, 48, 160, 140, 1860);
        return;
    }

    if (strcmp(current_state, "Active") == 0) {
        RGB_StartBreathingEx(0, 180, 255, 18, 140, 2, 12);
        return;
    }

    RGB_StartBreathingEx(90, 120, 160, 8, 48, 1, 30);
}

static void apply_led_output(void)
{
    if (codex_led_enabled) {
        TickType_t now = xTaskGetTickCount();
        bool offline = (now - codex_status_tick) > pdMS_TO_TICKS(CODEX_STATUS_TIMEOUT_MS);
        codex_last_offline = offline;
        codex_offline_known = true;
        apply_codex_led_effect(offline);
        return;
    }

    RGB_SetSolid(current_r, current_g, current_b);
}

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
    ensure_codex_led_task();

    if (app_storage_get_led_rgb(&current_r, &current_g, &current_b)) {
        update_state_name();
        apply_led_output();
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
    codex_led_enabled = false;
    codex_offline_known = false;
    current_r = r;
    current_g = g;
    current_b = b;
    update_state_name();
    apply_led_output();

    if (!persist) {
        return ESP_OK;
    }

    esp_err_t ret = app_storage_set_led_rgb(current_r, current_g, current_b);
    if (ret == ESP_OK) {
        ret = app_storage_set_led_state(current_state);
    }
    return ret;
}

void app_led_state_apply_codex_status(const char *status)
{
    if (!status || status[0] == '\0') {
        return;
    }

    codex_led_enabled = true;
    codex_status_tick = xTaskGetTickCount();
    codex_offline_known = false;
    if (strcmp(status, "Active") == 0) {
        current_r = 0;
        current_g = 180;
        current_b = 255;
    } else {
        current_r = 90;
        current_g = 120;
        current_b = 160;
    }
    strlcpy(current_state, status, sizeof(current_state));
    apply_led_output();
}
