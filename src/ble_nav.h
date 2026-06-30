#pragma once
#include <Arduino.h>

// Khoi tao BLE server (nhan goi navigation + dong bo thoi gian tu dien thoai)
void ble_init();

// Tat han BLE (giai phong song radio cho WiFi tren ESP32-C3)
void ble_stop();

// Gui lenh dieu khien nhac len dien thoai: "next" / "prev" / "playpause"
void ble_send_media(const char *cmd);

// Gui trang thai len dien thoai (notify): pin, dang dan duong...
void ble_notify_status();
