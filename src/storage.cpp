#include "storage.h"
#include "config.h"
#include "app_state.h"
#include <LittleFS.h>

static lv_img_dsc_t  s_dsc;
static uint8_t      *s_buf = nullptr;   // buffer dung chung (anh nen / QR), 1 anh 1 luc

bool storage_init() {
    if (!LittleFS.begin(true)) {        // true = format neu chua co
        Serial.println("[FS] LittleFS mount that bai");
        return false;
    }
    Serial.println("[FS] LittleFS OK");
    return true;
}

bool storage_has_image(const char *path) {
    return LittleFS.exists(path);
}

// Nap 1 anh RGB565 240x240 (115200 byte) tu LittleFS vao buffer heap.
void storage_free_image() {
    if (s_buf) { free(s_buf); s_buf = nullptr; }
}

lv_img_dsc_t* storage_load_image(const char *path) {
    storage_free_image();                       // bo anh cu (chi giu 1 anh)
    if (!LittleFS.exists(path)) return nullptr;
    File f = LittleFS.open(path, "r");
    if (!f) return nullptr;
    if (f.size() < WALLPAPER_BYTES) { f.close(); return nullptr; }

    s_buf = (uint8_t *)malloc(WALLPAPER_BYTES);
    if (!s_buf) { f.close(); return nullptr; }
    f.read(s_buf, WALLPAPER_BYTES);
    f.close();

    s_dsc.header.always_zero = 0;
    s_dsc.header.w  = SCREEN_W;
    s_dsc.header.h  = SCREEN_H;
    s_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;     // RGB565
    s_dsc.data_size = WALLPAPER_BYTES;
    s_dsc.data      = s_buf;
    return &s_dsc;
}

// --- Tien ich cho anh nen (dung lai loader chung) ---
bool storage_has_wallpaper()            { return storage_has_image(WALLPAPER_PATH); }
lv_img_dsc_t* storage_load_wallpaper()  { return storage_load_image(WALLPAPER_PATH); }
void storage_free_wallpaper()           { storage_free_image(); }
