#pragma once

#include "driver/gpio.h"
#include "led_strip.h"

#define BLINK_GPIO 8

void RGB_Init(void);
void Set_RGB(uint8_t red_val, uint8_t green_val, uint8_t blue_val);
void RGB_StartBreathing(uint8_t r, uint8_t g, uint8_t b);
void RGB_StopBreathing(void);
void RGB_SetSolid(uint8_t r, uint8_t g, uint8_t b);
