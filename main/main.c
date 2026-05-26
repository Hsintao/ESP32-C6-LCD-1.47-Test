/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "ST7789.h"
#include "SD_SPI.h"
#include "RGB.h"
#include "Wireless.h"
#include "LVGL_Example.h"

void app_main(void)
{
    RGB_Init();
    RGB_StartBreathing(0, 0, 255);
    BLE_Peripheral_Init();
    SD_Init();
    LCD_Init();
    BK_Light(50);
    LVGL_Init();

    Lvgl_Example1();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_timer_handler();
    }
}
