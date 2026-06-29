#include "storage.h"
#include "config.h"
#include <LittleFS.h>

static lv_img_dsc_t  s_wp_dsc;
static uint8_t      *s_wp_buf = nullptr;

bool storage_init() {
    if (!LittleFS.begin(true)) {        // true = format neu chua co
        Serial.println("[FS] LittleFS mount that bai");
        return false;
    }
    Serial.println("[FS] LittleFS OK");
    return true;
}

bool storage_has_wallpaper() {
    return LittleFS.exists(WALLPAPER_PATH);
}

lv_img_dsc_t* storage_load_wallpaper() {
    if (s_wp_buf) return &s_wp_dsc;          // da nap roi
    if (!storage_has_wallpaper()) return nullptr;

    File f = LittleFS.open(WALLPAPER_PATH, "r");
    if (!f) return nullptr;
    if (f.size() < WALLPAPER_BYTES) { f.close(); return nullptr; }

    // Cap phat 115KB tren heap. Neu het RAM se tra NULL -> UI dung mau nen mac dinh.
    s_wp_buf = (uint8_t *)malloc(WALLPAPER_BYTES);
    if (!s_wp_buf) {
        Serial.println("[FS] Het RAM cho wallpaper");
        f.close();
        return nullptr;
    }
    f.read(s_wp_buf, WALLPAPER_BYTES);
    f.close();

    s_wp_dsc.header.always_zero = 0;
    s_wp_dsc.header.w  = SCREEN_W;
    s_wp_dsc.header.h  = SCREEN_H;
    s_wp_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;   // RGB565 (LV_COLOR_DEPTH=16)
    s_wp_dsc.data_size = WALLPAPER_BYTES;
    s_wp_dsc.data      = s_wp_buf;
    return &s_wp_dsc;
}

void storage_free_wallpaper() {
    if (s_wp_buf) { free(s_wp_buf); s_wp_buf = nullptr; }
}
