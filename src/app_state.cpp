#include "app_state.h"
#include <sys/time.h>
#include <time.h>

NavState   g_nav;
ClockState g_clock;
SysState   g_sys;
volatile bool g_wpUpdated = false;

volatile int  g_routeCount = 0;
int8_t        g_routeXY[MAX_ROUTE_PTS * 2];
volatile bool g_routeDirty = false;

NotifyState g_notify;
ReplyBook   g_replies;
MusicState  g_music;
uint8_t      g_uiR = 255, g_uiG = 255, g_uiB = 255;
volatile bool g_colorChanged = false;
volatile bool g_colorSave = false;
uint8_t       g_wfPos = 1;    // giua
uint8_t       g_wfSize = 4;   // vua
uint8_t       g_dateSize = 2;
uint8_t       g_dateShow = 1;
uint8_t       g_dateR = 170, g_dateG = 170, g_dateB = 170;  // xam
volatile bool g_wfCfgChanged = false;
WeatherState g_weather;
CallState g_call;
ContactBook g_contacts;
volatile uint32_t g_lastInputMs = 0;

volatile uint8_t g_uploadTarget = 0xFF;   // mac dinh: anh nen
volatile bool    g_qrImgUpdated = false;

// Dung dong ho he thong (RTC) -> giu duoc gio qua deep sleep
void clock_sync(uint32_t epoch) {
    struct timeval tv;
    tv.tv_sec  = (time_t)epoch;
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);
    g_clock.synced = true;
}

uint32_t clock_now_epoch() {
    time_t t = time(nullptr);
    return (t > 1700000000) ? (uint32_t)t : 0;   // >2023 = da dong bo
}
