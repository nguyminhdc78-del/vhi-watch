#pragma once
#include <Arduino.h>

// Khoi tao BLE server (nhan goi navigation + dong bo thoi gian tu dien thoai)
void ble_init();

// Gui trang thai len dien thoai (notify): pin, dang dan duong...
void ble_notify_status();
