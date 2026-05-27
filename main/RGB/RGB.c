#include "RGB.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdlib.h>

static led_strip_handle_t led_strip;
static TaskHandle_t breathing_task_handle = NULL;
static SemaphoreHandle_t led_strip_mutex;
static SemaphoreHandle_t rgb_control_mutex;
static volatile bool effect_stop_requested;

typedef enum {
    RGB_EFFECT_BREATHING,
    RGB_EFFECT_PULSE,
} rgb_effect_t;

typedef struct {
    rgb_effect_t effect;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t min_brightness;
    uint8_t max_brightness;
    uint8_t step;
    uint8_t peak_brightness;
    uint16_t delay_ms;
    uint16_t on_ms;
    uint16_t off_ms;
} rgb_color_t;

void RGB_Init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = BLINK_GPIO,
        .max_leds = 1,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_mutex = xSemaphoreCreateMutex();
    rgb_control_mutex = xSemaphoreCreateMutex();
    RGB_SetSolid(0, 0, 0);
}

void Set_RGB(uint8_t red_val, uint8_t green_val, uint8_t blue_val)
{
    if (led_strip == NULL || led_strip_mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(led_strip_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }

    led_strip_set_pixel(led_strip, 0, green_val, red_val, blue_val);
    led_strip_refresh(led_strip);
    xSemaphoreGive(led_strip_mutex);
}

static bool effect_delay(uint16_t delay_ms)
{
    TickType_t remaining = pdMS_TO_TICKS(delay_ms);
    const TickType_t step = pdMS_TO_TICKS(20);

    while (!effect_stop_requested && remaining > 0) {
        TickType_t chunk = remaining > step ? step : remaining;
        vTaskDelay(chunk);
        remaining -= chunk;
    }

    return effect_stop_requested;
}

static void stop_effect_locked(void)
{
    if (breathing_task_handle != NULL) {
        effect_stop_requested = true;
        for (int i = 0; i < 50 && breathing_task_handle != NULL; i++) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

static void breathing_task(void *arg)
{
    rgb_color_t color = *(rgb_color_t *)arg;
    free(arg);

    int brightness = color.min_brightness;
    int step = color.step ? color.step : 1;

    while (!effect_stop_requested) {
        if (color.effect == RGB_EFFECT_PULSE) {
            uint8_t r = (uint8_t)((color.r * color.peak_brightness) / 255);
            uint8_t g = (uint8_t)((color.g * color.peak_brightness) / 255);
            uint8_t b = (uint8_t)((color.b * color.peak_brightness) / 255);
            Set_RGB(r, g, b);
            if (effect_delay(color.on_ms)) {
                break;
            }
            Set_RGB(0, 0, 0);
            if (effect_delay(color.off_ms)) {
                break;
            }
            continue;
        }

        brightness += step;
        if (brightness >= color.max_brightness) {
            brightness = color.max_brightness;
            step = -step;
        } else if (brightness <= color.min_brightness) {
            brightness = color.min_brightness;
            step = -step;
        }

        uint8_t r = (uint8_t)((color.r * brightness) / 255);
        uint8_t g = (uint8_t)((color.g * brightness) / 255);
        uint8_t b = (uint8_t)((color.b * brightness) / 255);
        Set_RGB(r, g, b);
        effect_delay(color.delay_ms);
    }

    breathing_task_handle = NULL;
    vTaskDelete(NULL);
}

void RGB_StartBreathing(uint8_t r, uint8_t g, uint8_t b)
{
    RGB_StartBreathingEx(r, g, b, 0, 255, 2, 10);
}

void RGB_StartBreathingEx(uint8_t r, uint8_t g, uint8_t b, uint8_t min_brightness, uint8_t max_brightness, uint8_t step, uint16_t delay_ms)
{
    if (rgb_control_mutex != NULL) {
        xSemaphoreTake(rgb_control_mutex, portMAX_DELAY);
    }

    stop_effect_locked();
    rgb_color_t *color = malloc(sizeof(rgb_color_t));
    if (color == NULL) {
        if (rgb_control_mutex != NULL) {
            xSemaphoreGive(rgb_control_mutex);
        }
        return;
    }
    color->effect = RGB_EFFECT_BREATHING;
    color->r = r;
    color->g = g;
    color->b = b;
    color->min_brightness = min_brightness;
    color->max_brightness = max_brightness < min_brightness ? min_brightness : max_brightness;
    color->step = step ? step : 1;
    color->delay_ms = delay_ms ? delay_ms : 10;
    color->peak_brightness = 255;
    color->on_ms = 0;
    color->off_ms = 0;
    effect_stop_requested = false;
    if (xTaskCreatePinnedToCore(breathing_task, "RGB Breathing", 4096, color, 4, &breathing_task_handle, 0) != pdPASS) {
        effect_stop_requested = true;
        free(color);
        breathing_task_handle = NULL;
    }

    if (rgb_control_mutex != NULL) {
        xSemaphoreGive(rgb_control_mutex);
    }
}

void RGB_StartPulse(uint8_t r, uint8_t g, uint8_t b, uint8_t peak_brightness, uint16_t on_ms, uint16_t off_ms)
{
    if (rgb_control_mutex != NULL) {
        xSemaphoreTake(rgb_control_mutex, portMAX_DELAY);
    }

    stop_effect_locked();
    rgb_color_t *color = malloc(sizeof(rgb_color_t));
    if (color == NULL) {
        if (rgb_control_mutex != NULL) {
            xSemaphoreGive(rgb_control_mutex);
        }
        return;
    }
    color->effect = RGB_EFFECT_PULSE;
    color->r = r;
    color->g = g;
    color->b = b;
    color->min_brightness = 0;
    color->max_brightness = 0;
    color->step = 0;
    color->delay_ms = 0;
    color->peak_brightness = peak_brightness;
    color->on_ms = on_ms ? on_ms : 120;
    color->off_ms = off_ms ? off_ms : 1880;
    effect_stop_requested = false;
    if (xTaskCreatePinnedToCore(breathing_task, "RGB Breathing", 4096, color, 4, &breathing_task_handle, 0) != pdPASS) {
        effect_stop_requested = true;
        free(color);
        breathing_task_handle = NULL;
    }

    if (rgb_control_mutex != NULL) {
        xSemaphoreGive(rgb_control_mutex);
    }
}

void RGB_StopBreathing(void)
{
    if (rgb_control_mutex != NULL) {
        xSemaphoreTake(rgb_control_mutex, portMAX_DELAY);
    }

    stop_effect_locked();
    Set_RGB(0, 0, 0);

    if (rgb_control_mutex != NULL) {
        xSemaphoreGive(rgb_control_mutex);
    }
}

void RGB_SetSolid(uint8_t r, uint8_t g, uint8_t b)
{
    if (rgb_control_mutex != NULL) {
        xSemaphoreTake(rgb_control_mutex, portMAX_DELAY);
    }

    stop_effect_locked();
    Set_RGB(r, g, b);

    if (rgb_control_mutex != NULL) {
        xSemaphoreGive(rgb_control_mutex);
    }
}
