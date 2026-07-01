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
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "esp_task_wdt.h"
#include <LittleFS.h>

// Watchdog: neu loop() treo qua bao lau (giay) -> tu reset chip (khong can nut RESET)
#define WDT_TIMEOUT_S   15

static uint32_t lastLvgl = 0, lastUi = 0, lastStatus = 0;
static bool s_screenOff = false;   // man hinh dang tat de tiet kiem pin

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

    g_lastInputMs = millis();       // bat dau dem idle

    // Watchdog: canh chung loopTask, treo qua WDT_TIMEOUT_S -> panic + tu reset
    esp_task_wdt_init(WDT_TIMEOUT_S, true);
    esp_task_wdt_add(NULL);
    Serial.println("=== San sang ===");
}

void loop() {
    uint32_t now = millis();

    esp_task_wdt_reset();   // "cho watchdog an" -> bao loop van chay binh thuong

    // 1) Quet nut nhan
    buttons_task();

    // 1b) Tu tat man hinh khi idle (tiet kiem pin) - KHONG reset chip, GIU Bluetooth.
    //     Bam nut bat cu nut nao -> sang lai ngay (phim danh thuc bi nuot, khong kich hoat menu).
    if (s_screenOff) {
        if (now - g_lastInputMs < 150) {   // vua co nut/thong bao -> bat man hinh
            display_wake();
            buttons_flush();               // nuot phim vua danh thuc
            ui_reload_wallpaper();         // ve lai mat dong ho cho chac
            s_screenOff = false;
        }
    } else if (ui_can_sleep() && (now - g_lastInputMs > SLEEP_TIMEOUT_MS)) {
        display_sleep();                   // tat den nen + ngu panel
        buttons_flush();
        s_screenOff = true;
    }

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

    // 3b) Game chay nhanh (~30fps) khi dang o man Game
    ui_fast_tick();

    // 4) Web server (khi dang upload)
    web_task();

    // 5) Notify trang thai len dien thoai ~3s
    if (now - lastStatus >= 3000) {
        lastStatus = now;
        ble_notify_status();
    }
}
