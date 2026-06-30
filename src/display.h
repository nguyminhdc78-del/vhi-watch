#pragma once
#include <lvgl.h>

// Khoi tao TFT_eSPI + LVGL (driver, buffer, flush callback)
void display_init();

// Dieu khien do sang backlight (0..255)
void display_set_backlight(uint8_t level);

// Tat man hinh + dua panel vao sleep (truoc khi deep sleep)
void display_sleep();
