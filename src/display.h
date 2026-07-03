#pragma once
#include <lvgl.h>

// Khoi tao TFT_eSPI + LVGL (driver, buffer, flush callback)
void display_init();

// Dieu khien do sang backlight (0..255)
void display_set_backlight(uint8_t level);

// Tat man hinh + dua panel vao sleep (tiet kiem pin khi idle)
void display_sleep();
// Bat lai man hinh sau khi sleep
void display_wake();

// Che do ve truc tiep (bo qua LVGL flush) - dung cho man QR
void display_set_raw(bool on);
bool display_draw_image_file(const char *path);  // ve anh 240x240 RGB565 tu file
bool display_draw_image_band(const char *path, int y0, int h);  // ve lai 1 dai ngang cua anh
void display_fill(uint16_t color);
void display_text(int x, int y, const char *s, uint16_t color, uint16_t bg);
void display_fill_rect(int x, int y, int w, int h, uint16_t color);
void display_text_center(int cx, int yTop, const char *s, uint16_t color, int size);
void display_draw_line(int x0, int y0, int x1, int y1, uint16_t color);
void display_draw_wide_line(int x0, int y0, int x1, int y1, int w, uint16_t color);
void display_draw_circle(int x, int y, int r, uint16_t color);
void display_fill_circle(int x, int y, int r, uint16_t color);
void display_fill_round_rect(int x, int y, int w, int h, int r, uint16_t color);
