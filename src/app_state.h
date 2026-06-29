#pragma once
#include <Arduino.h>

// ============================================================
//  Trang thai dung chung giua BLE / Web / UI
// ============================================================

// Huong re (khop voi enum ben app dien thoai)
enum NavManeuver {
    NAV_NONE = 0,
    NAV_STRAIGHT,
    NAV_TURN_LEFT,
    NAV_TURN_RIGHT,
    NAV_SLIGHT_LEFT,
    NAV_SLIGHT_RIGHT,
    NAV_SHARP_LEFT,
    NAV_SHARP_RIGHT,
    NAV_UTURN,
    NAV_ARRIVE,
    NAV_ROUNDABOUT
};

struct NavState {
    bool        active        = false;     // dang dan duong?
    NavManeuver maneuver      = NAV_NONE;
    uint32_t    distance_m    = 0;         // khoang cach toi diem re (met)
    char        street[48]    = "";        // ten duong / huong dan
    char        eta[16]       = "";        // gio den du kien "12:34"
    uint32_t    remain_m      = 0;         // tong quang duong con lai (met)
    uint32_t    lastUpdateMs  = 0;
};

struct ClockState {
    bool     synced  = false;
    uint32_t epoch   = 0;     // unix epoch (giay) luc dong bo
    uint32_t baseMs  = 0;     // millis() luc dong bo
};

struct SysState {
    bool bleConnected = false;
    int  battPercent  = 100;  // TODO: doc ADC neu co mach pin
    bool wifiUp       = false;
};

extern NavState   g_nav;
extern ClockState g_clock;
extern SysState   g_sys;

// Lay gio hien tai (epoch giay) tu m2 dong bo
uint32_t clock_now_epoch();
void     clock_sync(uint32_t epoch);
