#pragma once
#include <Arduino.h>
#include <lvgl.h>

// Khoi tao LittleFS
bool storage_init();

// Co wallpaper trong flash khong?
bool storage_has_wallpaper();

// Nap wallpaper (RGB565 raw) tu LittleFS vao buffer heap.
// Tra ve con tro lv_img_dsc_t da cap phat (NULL neu loi / het RAM).
// Nho goi storage_free_wallpaper() khi roi man hinh dong ho.
lv_img_dsc_t* storage_load_wallpaper();
void          storage_free_wallpaper();
