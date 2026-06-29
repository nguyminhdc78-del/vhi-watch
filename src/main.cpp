// ============================================================
//  ESP32-C3 Smartwatch + Navigation (BLE relay)
//  - Man hinh: ST7789 240x240 SPI
//  - UI: LVGL 8.3 + TFT_eSPI
//  - BLE: nhan goi chi duong + dong bo gio tu dien thoai
//  - WiFi AP: upload anh nen (RGB565) tu dien thoai
// ============================================================
#include <Arduino.h>
#include <lvgl.h>
#include "config.h"
#include "display.h"
#include "buttons.h"
#include "storage.h"
#include "ui.h"
#include "ble_nav.h"
#include "web_upload.h"
#include "app_state.h"

static uint32_t lastLvgl = 0, lastUi = 0, lastStatus = 0;

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=== VHI Smartwatch khoi dong ===");

    storage_init();                 // LittleFS
    display_init();                 // TFT + LVGL
    lv_group_t *grp = buttons_init();
    ui_init(grp);                   // dung UI

    ble_init();                     // BLE server
    web_on_wallpaper_updated(ui_reload_wallpaper);

    Serial.println("=== San sang ===");
}

void loop() {
    uint32_t now = millis();

    // 1) Quet nut nhan
    buttons_task();

    // 2) LVGL handler (~5ms)
    if (now - lastLvgl >= LVGL_TICK_MS) {
        lastLvgl = now;
        lv_timer_handler();
    }

    // 3) Cap nhat UI (dong ho/nav) ~200ms
    if (now - lastUi >= 200) {
        lastUi = now;
        ui_tick();
    }

    // 4) Web server (khi dang upload)
    web_task();

    // 5) Notify trang thai len dien thoai ~3s
    if (now - lastStatus >= 3000) {
        lastStatus = now;
        ble_notify_status();
    }
}
