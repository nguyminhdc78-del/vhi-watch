#pragma once
// ============================================================
//  Cau hinh chung cho ESP32-C3 Smartwatch
// ============================================================

// ---------------- Man hinh ----------------
#define SCREEN_W            240
#define SCREEN_H            240
// Xoay man hinh: 0..3. 1.54" ST7789 thuong dung 0 hoac 2.
#define TFT_ROTATION        0
// Do sang mac dinh (0..255) - de max cho de nhin ngoai troi
#define BACKLIGHT_DEFAULT   255

// Chan ket noi ST7789 (SPI) - xem so do trong README
#define PIN_LCD_SCLK        4
#define PIN_LCD_MOSI        6
#define PIN_LCD_DC          3
#define PIN_LCD_CS          7
#define PIN_LCD_RST         10
#define PIN_LCD_BL          5
#define LCD_SPI_FREQ        40000000
// ST7789 thuong can dao mau (invert). Neu mau bi nguoc thi doi false.
#define LCD_INVERT          true

// ---------------- Nut nhan ----------------
// 3 nut, noi GPIO -> GND, dung internal pull-up (nhan = LOW)
#define PIN_BTN_A           0   // NEXT  - chuyen/cuon
#define PIN_BTN_B           1   // ENTER - chon / mo menu
#define PIN_BTN_C           9   // BACK  - quay lai (thuong la nut BOOT san co tren board)
#define BTN_ACTIVE_LOW      1
#define BTN_DEBOUNCE_MS     30
#define BTN_LONGPRESS_MS    600

// ---------------- BLE ----------------
#define BLE_DEVICE_NAME     "VHI-Watch"
// UUID dich vu/dac tinh (tu dinh nghia - phai khop voi app dien thoai)
#define BLE_SVC_UUID        "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_CHR_NAV_UUID    "6e400002-b5a3-f393-e0a9-e50e24dcca9e" // phone -> watch: goi nav (JSON)
#define BLE_CHR_TIME_UUID   "6e400003-b5a3-f393-e0a9-e50e24dcca9e" // phone -> watch: dong bo thoi gian (epoch)
#define BLE_CHR_STATUS_UUID "6e400004-b5a3-f393-e0a9-e50e24dcca9e" // watch -> phone: trang thai (notify)
#define BLE_CHR_WP_UUID     "6e400005-b5a3-f393-e0a9-e50e24dcca9e" // phone -> watch: anh nen RGB565 (stream tung chunk)
#define BLE_CHR_ROUTE_UUID  "6e400006-b5a3-f393-e0a9-e50e24dcca9e" // phone -> watch: duong line lo trinh (int8 x,y pairs)
#define BLE_CHR_NOTIFY_UUID "6e400007-b5a3-f393-e0a9-e50e24dcca9e" // phone -> watch: thong bao (JSON app/title/text)
#define BLE_CHR_MUSIC_UUID  "6e400008-b5a3-f393-e0a9-e50e24dcca9e" // phone -> watch: bai hat dang phat (JSON)
#define BLE_CHR_MEDIA_UUID  "6e400009-b5a3-f393-e0a9-e50e24dcca9e" // watch -> phone: lenh dieu khien nhac (notify)
#define BLE_CHR_COLOR_UUID  "6e40000a-b5a3-f393-e0a9-e50e24dcca9e" // phone -> watch: mau chu (3 byte R,G,B)
#define BLE_CHR_IMGSEL_UUID "6e40000b-b5a3-f393-e0a9-e50e24dcca9e" // phone -> watch: chon dich upload anh (0xFF=anh nen, 0..N=o QR)
#define BLE_CHR_WEATHER_UUID "6e40000c-b5a3-f393-e0a9-e50e24dcca9e" // phone -> watch: thoi tiet (JSON {"t":31,"w":"Nang"})
#define BLE_CHR_CALL_UUID    "6e40000d-b5a3-f393-e0a9-e50e24dcca9e" // phone -> watch: cuoc goi den (JSON {"st":1,"name":"..","app":".."})
#define BLE_CHR_CONTACT_UUID "6e40000e-b5a3-f393-e0a9-e50e24dcca9e" // phone -> watch: danh ba nhanh ("C"=xoa het, "ten\tso"=them 1)
#define BLE_CHR_WFCFG_UUID   "6e40000f-b5a3-f393-e0a9-e50e24dcca9e" // phone -> watch: giao dien gio (2 byte: pos, size)
#define BLE_CHR_REPLY_UUID   "6e400010-b5a3-f393-e0a9-e50e24dcca9e" // phone -> watch: cau tra loi nhanh ("C"=xoa, "text"=them 1)

// The ten / QR badge: so o luu anh QR (240x240 RGB565 trong LittleFS)
#define QR_COUNT            4
#define QR_IMG_PATH_FMT     "/qr%d.bin"

// ---------------- WiFi / Web upload anh ----------------
// Che do Access Point: dien thoai ket noi truc tiep vao wifi cua dong ho de upload anh
#define WIFI_AP_SSID        "VHI-Watch-Setup"
#define WIFI_AP_PASS        "12345678"
#define WEB_PORT            80

// ---------------- Wallpaper ----------------
// Anh wallpaper: raw RGB565, 240x240 = 115200 bytes, luu LittleFS
#define WALLPAPER_PATH      "/wallpaper.bin"
#define WALLPAPER_BYTES     (SCREEN_W * SCREEN_H * 2)

// ---------------- Deep sleep ----------------
// Khong bam nut trong bao lau (ms) thi tat man hinh (chi khi dang o mat dong ho)
#define SLEEP_TIMEOUT_MS    60000

// ---------------- LVGL ----------------
#define LVGL_TICK_MS        5
// Buffer ve tung phan (partial) de tiet kiem RAM: 1/6 man hinh
#define LVGL_BUF_LINES      40
