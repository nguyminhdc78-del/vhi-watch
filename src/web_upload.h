#pragma once
#include <Arduino.h>

// Bat che do upload: mo WiFi AP + web server.
// Tra ve SSID/IP de hien len man hinh.
void web_start();
void web_stop();
bool web_is_running();
void web_task();          // goi trong loop khi dang chay

// Callback khi upload xong (UI nap lai wallpaper)
typedef void (*WallpaperUpdatedCb)();
void web_on_wallpaper_updated(WallpaperUpdatedCb cb);
