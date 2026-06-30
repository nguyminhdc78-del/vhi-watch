#pragma once
#include <Arduino.h>
#include <lvgl.h>

// Khoi tao LittleFS
bool storage_init();

// Nap/giai phong 1 anh RGB565 240x240 tu LittleFS (dung chung cho anh nen + QR)
lv_img_dsc_t* storage_load_image(const char *path);
void          storage_free_image();
bool          storage_has_image(const char *path);

// Tien ich rieng cho anh nen
bool          storage_has_wallpaper();
lv_img_dsc_t* storage_load_wallpaper();
void          storage_free_wallpaper();
