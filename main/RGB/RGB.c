#include "RGB.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>

static led_strip_handle_t led_strip;
static TaskHandle_t breathing_task_handle = NULL;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
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
    led_strip_clear(led_strip);
}

void Set_RGB(uint8_t red_val, uint8_t green_val, uint8_t blue_val)
{
    led_strip_set_pixel(led_strip, 0, green_val, red_val, blue_val);
    led_strip_refresh(led_strip);
}

static void breathing_task(void *arg)
{
    rgb_color_t *color = (rgb_color_t *)arg;
    int brightness = 0;
    int step = 2;
    while (1) {
        brightness += step;
        if (brightness >= 255) {
            brightness = 255;
            step = -2;
        } else if (brightness <= 0) {
            brightness = 0;
            step = 2;
        }
        uint8_t r = (uint8_t)((color->r * brightness) / 255);
        uint8_t g = (uint8_t)((color->g * brightness) / 255);
        uint8_t b = (uint8_t)((color->b * brightness) / 255);
        Set_RGB(r, g, b);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void RGB_StartBreathing(uint8_t r, uint8_t g, uint8_t b)
{
    RGB_StopBreathing();
    rgb_color_t *color = malloc(sizeof(rgb_color_t));
    if (color == NULL) return;
    color->r = r;
    color->g = g;
    color->b = b;
    xTaskCreatePinnedToCore(breathing_task, "RGB Breathing", 4096, color, 4, &breathing_task_handle, 0);
}

void RGB_StopBreathing(void)
{
    if (breathing_task_handle != NULL) {
        vTaskDelete(breathing_task_handle);
        breathing_task_handle = NULL;
    }
    led_strip_clear(led_strip);
}

void RGB_SetSolid(uint8_t r, uint8_t g, uint8_t b)
{
    if (breathing_task_handle != NULL) {
        vTaskDelete(breathing_task_handle);
        breathing_task_handle = NULL;
    }
    Set_RGB(r, g, b);
}
