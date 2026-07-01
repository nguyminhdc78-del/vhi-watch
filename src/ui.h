#pragma once
#include <lvgl.h>

// Khoi tao toan bo UI. Truyen vao group keypad tu buttons_init().
void ui_init(lv_group_t *group);

// Goi dinh ky (vd 200ms) de cap nhat dong ho / du lieu nav
void ui_tick();

// Goi khi wallpaper duoc cap nhat tu web (UI nap lai anh nen)
void ui_reload_wallpaper();

// Co cho phep ngu sau khong (chi ngu khi dang o mat dong ho)
bool ui_can_sleep();
