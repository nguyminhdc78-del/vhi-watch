#pragma once
#include <lvgl.h>

// Khoi tao 2 nut nhan + dang ky lam input keypad cho LVGL.
// Tra ve group de gan cac doi tuong UI vao (focus/navigation).
lv_group_t* buttons_init();

// Goi dinh ky trong loop() de quet nut + sinh su kien
void buttons_task();

// Xoa hang doi phim (nuot phim danh thuc man hinh khi vua sleep)
void buttons_flush();
