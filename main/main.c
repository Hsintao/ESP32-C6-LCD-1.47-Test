/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "ST7789.h"
#include "RGB.h"
#include "esp_err.h"
#include "LVGL_Driver.h"
#include "app_http.h"
#include "app_led_state.h"
#include "app_storage.h"
#include "app_ui.h"
#include "app_wifi.h"

void app_main(void)
{
    ESP_ERROR_CHECK(app_storage_init());

    RGB_Init();
    app_led_state_init();

    LCD_Init();
    BK_Light(50);
    LVGL_Init();
    app_ui_init();

    ESP_ERROR_CHECK(app_wifi_start());
    ESP_ERROR_CHECK(app_http_start());

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_timer_handler();
    }
}
