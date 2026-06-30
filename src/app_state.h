#pragma once
#include <Arduino.h>
#include "config.h"

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

// Co bao: vua nhan xong anh nen moi qua BLE -> UI nap lai
extern volatile bool g_wpUpdated;

// Duong line lo trinh nhan tu app (toa do int8 lech so voi tam, da xoay theo huong di)
#define MAX_ROUTE_PTS 48
extern volatile int  g_routeCount;
extern int8_t        g_routeXY[MAX_ROUTE_PTS * 2];
extern volatile bool g_routeDirty;

// --- Thong bao tu dien thoai ---
struct NotifyState {
    char app[24]   = "";
    char title[48] = "";
    char text[96]  = "";
    bool hasNew     = false;   // co thong bao moi chua xem
};
extern NotifyState g_notify;

// --- Bai hat dang phat ---
struct MusicState {
    char title[48]  = "";
    char artist[48] = "";
    bool playing     = false;
};
extern MusicState g_music;

// --- Mau chu giao dien (mac dinh trang) ---
extern uint8_t      g_uiR, g_uiG, g_uiB;
extern volatile bool g_colorChanged;

// Moc thoi gian co hoat dong gan nhat (nut bam / thong bao) - de tinh idle -> deep sleep
extern volatile uint32_t g_lastInputMs;

// --- Upload anh: chon dich (0xFF = anh nen, 0..QR_COUNT-1 = o QR) ---
extern volatile uint8_t g_uploadTarget;
extern volatile bool    g_qrImgUpdated;   // vua nhan xong 1 anh QR

// Lay gio hien tai (epoch giay) tu m2 dong bo
uint32_t clock_now_epoch();
void     clock_sync(uint32_t epoch);
