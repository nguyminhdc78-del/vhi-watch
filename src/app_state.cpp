#include "app_state.h"

NavState   g_nav;
ClockState g_clock;
SysState   g_sys;

void clock_sync(uint32_t epoch) {
    g_clock.epoch  = epoch;
    g_clock.baseMs = millis();
    g_clock.synced = true;
}

uint32_t clock_now_epoch() {
    if (!g_clock.synced) return 0;
    return g_clock.epoch + (millis() - g_clock.baseMs) / 1000;
}
