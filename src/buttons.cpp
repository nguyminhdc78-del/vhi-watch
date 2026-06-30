#include "buttons.h"
#include "config.h"
#include "app_state.h"
#include <Arduino.h>

// ============================================================
//  2 nut nhan -> LVGL keypad
//  A ngan : NEXT  (chuyen focus / cuon)
//  A giu  : PREV  (lui focus)
//  B ngan : ENTER (chon / kich hoat)
//  B giu  : ESC   (quay lai)
// ============================================================

struct Btn {
    uint8_t  pin;
    bool     last;          // trang thai da debounce (true = dang nhan)
    uint32_t tEdge;         // moc thoi gian doi trang thai
    uint32_t tPress;        // moc bat dau nhan
    bool     longSent;      // da ban su kien long-press chua
};

static Btn btnA = { PIN_BTN_A, false, 0, 0, false };
static Btn btnB = { PIN_BTN_B, false, 0, 0, false };
static Btn btnC = { PIN_BTN_C, false, 0, 0, false };

// Hang doi key don gian (ring buffer)
static uint32_t keyQueue[8];
static uint8_t  qHead = 0, qTail = 0;

static lv_indev_t* indev_keypad = nullptr;
static lv_group_t* ui_group     = nullptr;

static void pushKey(uint32_t k) {
    g_lastInputMs = millis();   // co hoat dong -> reset dem idle (chong deep sleep)
    uint8_t next = (qHead + 1) % 8;
    if (next != qTail) { keyQueue[qHead] = k; qHead = next; }
}

static bool rawPressed(uint8_t pin) {
    int v = digitalRead(pin);
    return BTN_ACTIVE_LOW ? (v == LOW) : (v == HIGH);
}

// Cap nhat 1 nut, sinh su kien short/long
static void updateBtn(Btn &b, uint32_t keyShort, uint32_t keyLong) {
    bool now = rawPressed(b.pin);
    uint32_t t = millis();

    if (now != b.last) {
        if (t - b.tEdge >= BTN_DEBOUNCE_MS) {     // qua debounce -> chap nhan
            b.last  = now;
            b.tEdge = t;
            if (now) {                            // vua nhan xuong
                b.tPress   = t;
                b.longSent = false;
            } else {                              // vua nha ra
                if (!b.longSent) pushKey(keyShort); // short press khi nha
            }
        }
    } else {
        b.tEdge = t;
        // dang giu -> kiem tra long press
        if (now && !b.longSent && (t - b.tPress >= BTN_LONGPRESS_MS)) {
            b.longSent = true;
            pushKey(keyLong);
        }
    }
}

void buttons_flush() {
    qTail = qHead;   // bo het phim dang cho (vd phim vua danh thuc man hinh)
}

void buttons_task() {
    // Dung DOWN/UP (khong dung NEXT/PREV vi LVGL nuot NEXT/PREV cho focus noi bo)
    updateBtn(btnA, LV_KEY_DOWN,  LV_KEY_UP);     // A: cuon xuong (giu = len)
    updateBtn(btnB, LV_KEY_ENTER, LV_KEY_ENTER);  // B: chon
    updateBtn(btnC, LV_KEY_ESC,   LV_KEY_ESC);    // C: quay lai
}

// LVGL goi de doc input
static void keypad_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    static uint32_t holdKey = 0;
    static bool     holding = false;

    if (holding) {
        // chu ky truoc da "press" -> bay gio bao "release"
        data->state = LV_INDEV_STATE_RELEASED;
        data->key   = holdKey;
        holding     = false;
        return;
    }

    if (qTail != qHead) {                 // co key trong hang doi
        holdKey = keyQueue[qTail];
        qTail   = (qTail + 1) % 8;
        data->state = LV_INDEV_STATE_PRESSED;
        data->key   = holdKey;
        holding     = true;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        data->key   = 0;
    }
}

lv_group_t* buttons_init() {
    pinMode(PIN_BTN_A, BTN_ACTIVE_LOW ? INPUT_PULLUP : INPUT);
    pinMode(PIN_BTN_B, BTN_ACTIVE_LOW ? INPUT_PULLUP : INPUT);
    pinMode(PIN_BTN_C, BTN_ACTIVE_LOW ? INPUT_PULLUP : INPUT);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = keypad_read;
    indev_keypad = lv_indev_drv_register(&indev_drv);

    ui_group = lv_group_create();
    lv_indev_set_group(indev_keypad, ui_group);
    return ui_group;
}
