#pragma once
#include <lvgl.h>

// ============================================================
//  VECTOR pet - man hoat hinh + cac tro (tach tu ui.cpp)
//  Them 1 tro: xem bang PET_TRICKS trong pet-tricks.cpp
// ============================================================

// Ket qua xu ly phim tren man Pet -> ui.cpp doc de quyet dinh chuyen man
enum PetKeyResult { PET_KEY_NONE, PET_KEY_TO_MENU };

void         build_pet(lv_obj_t *scr);   // dung man Pet (cap phat canvas + ve khung dau)
void         pet_teardown();             // giai phong bo dem canvas (goi khi roi man Pet)
void         pet_tick(uint32_t now);     // hoat hinh moi vong loop (~40ms), goi khi dang o man Pet
PetKeyResult pet_key(uint32_t key);      // xu ly 1 phim (LV_KEY_*) khi o man Pet
