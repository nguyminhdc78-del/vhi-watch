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

static uint32_t lastLvgl = 0, lastUi = 0, lastStatus = 0;

// Vao deep sleep: tat man hinh, ngu, thuc khi nhan nut A(GPIO0) hoac B(GPIO1)
// (Chip khoi dong lai khi thuc; gio van giu nho dong ho RTC)
static void enter_deep_sleep() {
    Serial.println("[SLEEP] Idle 30s -> deep sleep");
    Serial.flush();
    display_sleep();
    delay(50);
    gpio_set_pull_mode((gpio_num_t)PIN_BTN_A, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)PIN_BTN_B, GPIO_PULLUP_ONLY);
    esp_deep_sleep_enable_gpio_wakeup(
        (1ULL << PIN_BTN_A) | (1ULL << PIN_BTN_B),
        ESP_GPIO_WAKEUP_GPIO_LOW);   // nhan nut keo xuong LOW -> thuc
    esp_deep_sleep_start();
}

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

    // 6) Idle qua lau (va dang o mat dong ho) -> ngu sau
    if (ui_can_sleep() && (now - g_lastInputMs > SLEEP_TIMEOUT_MS)) {
        enter_deep_sleep();
    }
}
