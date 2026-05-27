#pragma once

#include "driver/gpio.h"
#include "led_strip.h"

#define BLINK_GPIO 8

void RGB_Init(void);
void Set_RGB(uint8_t red_val, uint8_t green_val, uint8_t blue_val);
void RGB_StartBreathing(uint8_t r, uint8_t g, uint8_t b);
void RGB_StartBreathingEx(uint8_t r, uint8_t g, uint8_t b, uint8_t min_brightness, uint8_t max_brightness, uint8_t step, uint16_t delay_ms);
void RGB_StartPulse(uint8_t r, uint8_t g, uint8_t b, uint8_t peak_brightness, uint16_t on_ms, uint16_t off_ms);
void RGB_StopBreathing(void);
void RGB_SetSolid(uint8_t r, uint8_t g, uint8_t b);
