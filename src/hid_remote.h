#pragma once
#include <Arduino.h>

// Che do Remote: dong ho gia lam ban phim Bluetooth (HID) cho may tinh.
// Luu y: goi ble_stop() TRUOC khi hid_start() (C3 chi chay 1 cau hinh BLE 1 luc).
void hid_start();
void hid_stop();
bool hid_connected();

// Gui 1 phim ban phim (USB HID keycode), tu nhan + nha
void hid_send_key(uint8_t keycode);

// Gui 1 phim Consumer Control (16-bit usage): vol up/down, chup hinh...
void hid_send_consumer(uint16_t usage);

// Keycode ban phim hay dung cho trinh chieu
#define KEY_RIGHT_ARROW 0x4F   // slide sau
#define KEY_LEFT_ARROW  0x50   // slide truoc

// Consumer usage
#define CC_VOLUME_UP    0x00E9 // dung lam nut chup hinh (Camera Android/iOS)
#define CC_VOLUME_DOWN  0x00EA
