#include "ui.h"
#include "config.h"
#include "app_state.h"
#include "storage.h"
#include "web_upload.h"
#include "ble_nav.h"
#include "hid_remote.h"
#include "display.h"
#include <Arduino.h>
#include <time.h>
#include <math.h>
#include <LittleFS.h>
#include "bao_img.h"
#include "eat_img.h"
#include "heart_img.h"

// Font tieng Viet (co dau) - dung cho thong bao / nhac / ten nguoi goi
LV_FONT_DECLARE(vn_font_16);
LV_FONT_DECLARE(vn_font_20);

// ============================================================
//  Quan ly man hinh bang 1 state machine don gian.
//  Moi man hinh co 1 container goc (focusable) nhan phim:
//    NEXT/PREV = di chuyen, ENTER = chon, ESC = quay lai
// ============================================================

enum Screen { SCR_WATCH, SCR_MENU, SCR_NAV, SCR_UPLOAD, SCR_NOTIFY, SCR_MUSIC, SCR_REMOTE, SCR_CAMERA, SCR_QR, SCR_FIND, SCR_TIMER, SCR_CALL, SCR_DIAL, SCR_REPLY, SCR_PET };

static lv_group_t *g_group   = nullptr;
static lv_obj_t   *g_scr     = nullptr;   // man hinh hien tai
static Screen      g_cur     = SCR_WATCH;

// Con tro toi cac label can cap nhat
static lv_obj_t *lblTime = nullptr, *lblDate = nullptr, *lblStat = nullptr;
static lv_obj_t *navArrow = nullptr, *navDist = nullptr, *navStreet = nullptr, *navEta = nullptr;
static lv_obj_t *navLine = nullptr, *navDot = nullptr;
static lv_obj_t *lblNApp = nullptr, *lblNTitle = nullptr, *lblNText = nullptr;
static lv_obj_t *lblMTitle = nullptr, *lblMArtist = nullptr, *lblMState = nullptr;
static lv_obj_t *lblRemote = nullptr;
static int       qrIdx = 0;
static lv_point_t g_linePts[MAX_ROUTE_PTS];
// Tam vung ve lo trinh (cham vi tri ban) - hoi thap de duong phia truoc huong len
#define ROUTE_CX 120
#define ROUTE_CY 184

// Menu
static const char *MENU_ITEMS[] = { LV_SYMBOL_BELL    " Thong bao",
                                    LV_SYMBOL_AUDIO   " Nhac",
                                    LV_SYMBOL_KEYBOARD " Remote PC",
                                    LV_SYMBOL_IMAGE   " Chup hinh",
                                    LV_SYMBOL_LIST    " The ten (QR)",
                                    LV_SYMBOL_IMAGE   " Doi anh nen",
                                    LV_SYMBOL_CALL    " Tim dien thoai",
                                    LV_SYMBOL_LOOP    " Bam gio",
                                    LV_SYMBOL_CALL    " Goi dien",
                                    LV_SYMBOL_EYE_OPEN " Vector",
                                    LV_SYMBOL_SETTINGS " Cai dat" };
static const int   MENU_N = 11;
static int         menuSel = 0;
static lv_obj_t   *menuBtns[MENU_N];

static void show_screen(Screen s);     // forward
static void request_screen(Screen s);  // forward: doi man hinh AN TOAN (hoan ngoai event)
static int  g_pendingScreen = -1;
// forward cho Bam gio + Game (goi tu key_handler)
static void tmr_toggle();
static void tmr_reset();
static void tmr_cycle();
static void dial_refresh();
static int  dialSel = 0;
static lv_obj_t *dialBtns[MAX_CONTACTS];
static lv_obj_t *lblDialInfo = nullptr;
static void reply_refresh();
static int  replySel = 0;
static lv_obj_t *replyBtns[MAX_REPLIES];
static lv_obj_t *lblReplyInfo = nullptr;
// Vector pet state (port RoboEyes) - dung o key_handler + ui_fast_tick
static float    pex = 0, pexN = 0, pey = 0, peyN = 0;   // wander offset (current, target)
static float    peh = 68, pehN = 68;                    // eye height (blink)
static uint32_t petTWander = 0, petTBlink = 0, petTBlinkOpen = 0;
static uint32_t petHappyUntil = 0, petLastFrame = 0;
// Bo "dao dien" tu doi tro: 0 idle 1 happy 2 laugh 3 confused 4 wink 5 angry 6 look
//                           7 love(trai tim) 8 dizzy(chong mat) 9 read(doc bao) 10 pho(an pho)
static int      petAct = 0, petWinkEye = 0;
static uint32_t petActUntil = 0, petTAct = 0;
// Menu chon animation (nut A) + khoa 1 tro chon tay
static bool     petMenuOpen = false, petLock = false;
static int      petMenuSel = 0;
// Choc gheo: bam nut A (LV_KEY_DOWN) don dap -> buc doc len -> gian
static int      petPokeCnt = 0;
static uint32_t petLastPoke = 0;
// ============================================================
//  BANG DANG KY TRO (registry)
//  Them 1 tro = them 1 DONG o day (+ viet ham ve neu la tro ve rieng).
//  Menu / random / dispatch deu TU DOC tu bang -> khoi sua nhieu cho.
// ============================================================
typedef void (*PetRenderFn)(uint32_t now);
struct PetTrick {
    const char *name;    // ten trong menu (nullptr = chi chay random, khong vao menu)
    int         act;     // ma tro (petAct)
    PetRenderFn render;  // ham ve rieng (nullptr = dung bo ve mat mac dinh theo 'mood')
    uint16_t    dur;     // thoi luong (ms)
    uint8_t     weight;  // trong so random (0 = khong tu chay, chi chon tay)
    uint8_t     night;   // 1 = uu tien luc dem (22h-6h)
};
// Ham ve rieng dinh nghia phia duoi -> khai bao truoc de bang tham chieu duoc
static void pet_read(uint32_t), pet_pho(uint32_t), pet_love(uint32_t), pet_dizzy(uint32_t);
static void pet_yawn(uint32_t), pet_sparkle(uint32_t), pet_confused(uint32_t);

static const PetTrick PET_TRICKS[] = {
  //  ten           act  ham ve         dur    wt  dem
  { "Đọc báo",      9,   pet_read,     10300,  3,  0 },
  { "Ăn phở",       10,  pet_pho,       4350,  5,  0 },
  { "Cười",         2,   nullptr,       2400, 12,  0 },
  { "Trái tim",     7,   pet_love,      2600,  9,  0 },
  { "Chóng mặt",    8,   pet_dizzy,     2000,  8,  0 },
  { "Giận dữ",      5,   nullptr,       1800,  5,  0 },
  { "Nháy mắt",     4,   nullptr,        260,  8,  0 },
  { "Ngáp",         11,  pet_yawn,      3200,  9,  1 },
  { "Lấp lánh",     12,  pet_sparkle,   2600, 11,  0 },
  { "Ngơ ngác",     13,  pet_confused,  2600, 11,  0 },
  { nullptr,        1,   nullptr,       2200, 16,  0 },   // cuoi mim (chi random)
  { nullptr,        3,   nullptr,       1500,  6,  0 },   // boi roi (chi random)
  { nullptr,        6,   nullptr,       1800,  3,  0 },   // to mo 1 ben (chi random)
};
#define PET_TRICK_CNT ((int)(sizeof(PET_TRICKS) / sizeof(PET_TRICKS[0])))

static const PetTrick* pet_trick_of(int act) {
    for (int i = 0; i < PET_TRICK_CNT; i++) if (PET_TRICKS[i].act == act) return &PET_TRICKS[i];
    return nullptr;
}
// Menu = cac tro co ten (theo thu tu bang) + "Tự động" o cuoi
static int pet_menu_n() {
    int n = 1;                                          // +1 cho "Tự động"
    for (int i = 0; i < PET_TRICK_CNT; i++) if (PET_TRICKS[i].name) n++;
    return n;
}
static const char* pet_menu_name(int idx) {
    int k = 0;
    for (int i = 0; i < PET_TRICK_CNT; i++) {
        if (!PET_TRICKS[i].name) continue;
        if (k++ == idx) return PET_TRICKS[i].name;
    }
    return "Tự động";
}
static int pet_menu_act(int idx) {                      // -1 = tu dong
    int k = 0;
    for (int i = 0; i < PET_TRICK_CNT; i++) {
        if (!PET_TRICKS[i].name) continue;
        if (k++ == idx) return PET_TRICKS[i].act;
    }
    return -1;
}

// ---------- tien ich ----------
static lv_obj_t* make_root() {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    return scr;
}

// Ham doi maneuver -> ky hieu mui ten
static const char* maneuver_symbol(NavManeuver m) {
    switch (m) {
        case NAV_TURN_LEFT:  case NAV_SLIGHT_LEFT:  case NAV_SHARP_LEFT:  return LV_SYMBOL_LEFT;
        case NAV_TURN_RIGHT: case NAV_SLIGHT_RIGHT: case NAV_SHARP_RIGHT: return LV_SYMBOL_RIGHT;
        case NAV_UTURN:      return LV_SYMBOL_LOOP;
        case NAV_ARRIVE:     return LV_SYMBOL_OK;
        case NAV_STRAIGHT:   default: return LV_SYMBOL_UP;
    }
}

// ============================================================
//  WATCHFACE
// ============================================================
// Watchface ve TRUC TIEP (anh nen + gio), nhe RAM
static int wfLastMin = -2, wfLastStat = -1;
static bool wfHasWp = false;   // co anh nen hay khong
static int wfStyle = 0;        // kieu mat dong ho (0=so lon, 1=lich, 2=kim)
#define WF_STYLE_N 3
static const char *WF_WD[] = {"CN", "T2", "T3", "T4", "T5", "T6", "T7"};

static uint16_t ui_color565() {
    return ((g_uiR >> 3) << 11) | ((g_uiG >> 2) << 5) | (g_uiB >> 3);
}

// Nap / luu kieu mat dong ho (LittleFS 1 byte, giu sau khi tat nguon)
static void wf_load_style() {
    File f = LittleFS.open("/wfstyle.dat", "r");
    if (f) { int v = f.read(); if (v >= 0 && v < WF_STYLE_N) wfStyle = v; f.close(); }
}
static void wf_save_style() {
    File f = LittleFS.open("/wfstyle.dat", "w");
    if (f) { f.write((uint8_t)wfStyle); f.close(); }
}

// Lay gio hien tai (GMT+7). Tra ve false neu chua dong bo.
static bool wf_get_time(struct tm &tm) {
    uint32_t ep = clock_now_epoch();
    if (!ep) return false;
    time_t t = (time_t)(ep + 7 * 3600);
    gmtime_r(&t, &tm);
    return true;
}

// Xoa 1 dai ngang: ve lai anh nen neu co, khong thi to den
static void wf_clear_band(int y0, int h) {
    if (!wfHasWp || !display_draw_image_band(WALLPAPER_PATH, y0, h))
        display_fill_rect(0, y0, SCREEN_W, h, 0x0000);
}

static void draw_wf_status() {
    wf_clear_band(0, 40);
    char s[40];
    snprintf(s, sizeof(s), "%s  %d%%",
             g_sys.bleConnected ? "BLE" : "Cho ket noi", g_sys.battPercent);
    display_text_center(SCREEN_W / 2, 2, s, 0x5E8C, 1);
    if (g_weather.has) {
        char w[24];
        snprintf(w, sizeof(w), "%dC  %s", g_weather.temp, g_weather.text);
        display_text_center(SCREEN_W / 2, 20, w, 0x0000, 2);   // to hon + mau den
    }
}

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

// --- Mat 0: so gio LON, vi tri + co + mau + an/hien ngay chinh duoc tu app ---
static void wf_face_big() {
    int sz = g_wfSize;  if (sz < 3) sz = 3; if (sz > 6) sz = 6;
    int dsz = g_dateSize; if (dsz < 1) dsz = 1; if (dsz > 3) dsz = 3;
    int timeH = sz * 8;                 // chieu cao chu gio
    int dateH = g_dateShow ? dsz * 8 : 0;
    int blockH = timeH + (g_dateShow ? 6 + dateH : 0);
    int top;
    if (g_wfPos == 0)      top = 46;                     // TREN (duoi thanh trang thai)
    else if (g_wfPos == 2) top = 234 - blockH;           // DUOI
    else                   top = (240 - blockH) / 2 + 8; // GIUA
    if (top < 44) top = 44;

    wf_clear_band(top - 2, blockH + 4);
    char tb[8] = "--:--", db[24] = "";
    struct tm tm;
    if (wf_get_time(tm)) {
        snprintf(tb, sizeof(tb), "%02d:%02d", tm.tm_hour, tm.tm_min);
        snprintf(db, sizeof(db), "%s %02d/%02d", WF_WD[tm.tm_wday], tm.tm_mday, tm.tm_mon + 1);
    }
    display_text_center(120, top, tb, ui_color565(), sz);
    if (g_dateShow)
        display_text_center(120, top + timeH + 6, db, rgb565(g_dateR, g_dateG, g_dateB), dsz);
}

// --- Mat 1: LICH - ngay to o giua, gio nho o tren ---
static void wf_face_date() {
    wf_clear_band(40, 176);
    char tb[8] = "--:--", db[16] = "";
    char wk[12] = "";
    static const char *WD2[] = {"Chu Nhat", "Thu Hai", "Thu Ba", "Thu Tu", "Thu Nam", "Thu Sau", "Thu Bay"};
    struct tm tm;
    if (wf_get_time(tm)) {
        snprintf(tb, sizeof(tb), "%02d:%02d", tm.tm_hour, tm.tm_min);
        snprintf(db, sizeof(db), "%02d/%02d/%04d", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);
        snprintf(wk, sizeof(wk), "%s", WD2[tm.tm_wday]);
    }
    display_text_center(120, 66, tb, ui_color565(), 5);   // GIO to nhat (doi mau duoc)
    display_text_center(120, 126, wk, 0xAD55, 3);          // thu
    display_text_center(120, 168, db, 0xFFFF, 2);          // ngay/thang/nam
}

// --- Mat 2: KIM (analog) ---
static void wf_face_analog() {
    wf_clear_band(40, 200);
    const int cx = 120, cy = 126, R = 86;
    uint16_t col = ui_color565();
    display_draw_circle(cx, cy, R, col);
    display_draw_circle(cx, cy, R - 1, col);
    for (int h = 0; h < 12; h++) {            // vach gio
        float rad = (h * 30 - 90) * DEG_TO_RAD;
        display_draw_line(cx + cosf(rad) * (R - 2),  cy + sinf(rad) * (R - 2),
                          cx + cosf(rad) * (R - 11), cy + sinf(rad) * (R - 11), col);
    }
    struct tm tm;
    if (wf_get_time(tm)) {
        float hdeg = (tm.tm_hour % 12) * 30 + tm.tm_min * 0.5f;
        float mdeg = tm.tm_min * 6;
        float hr = (hdeg - 90) * DEG_TO_RAD, mr = (mdeg - 90) * DEG_TO_RAD;
        display_draw_wide_line(cx, cy, cx + cosf(hr) * (R * 0.5f), cy + sinf(hr) * (R * 0.5f), 5, 0xFFFF);
        display_draw_wide_line(cx, cy, cx + cosf(mr) * (R * 0.78f), cy + sinf(mr) * (R * 0.78f), 3, col);
        char dd[16];
        snprintf(dd, sizeof(dd), "%s %02d/%02d", WF_WD[tm.tm_wday], tm.tm_mday, tm.tm_mon + 1);
        display_text_center(120, 222, dd, 0xAD55, 2);
    }
    display_fill_circle(cx, cy, 4, 0xFFFF);
}

static void draw_wf_time() {
    if (wfStyle == 2)      wf_face_analog();
    else if (wfStyle == 1) wf_face_date();
    else                   wf_face_big();
}

static void draw_wf_full() {
    wfHasWp = display_draw_image_file(WALLPAPER_PATH);
    if (!wfHasWp) display_fill(0x0000);
    draw_wf_status();
    draw_wf_time();
    if (!wfHasWp)
        display_text_center(SCREEN_W / 2, 228, "A: doi mat   B: menu", 0x8410, 1);
}

static void build_watchface(lv_obj_t *scr) {
    (void)scr;
    display_set_raw(true);            // ve thang ra man (nhe RAM, ho tro anh nen)
    wf_load_style();
    wfLastMin = -2; wfLastStat = -1;
    draw_wf_full();
}

// ============================================================
//  MENU
// ============================================================
static void refresh_menu_highlight() {
    for (int i = 0; i < MENU_N; i++) {
        bool sel = (i == menuSel);
        lv_obj_set_style_bg_color(menuBtns[i],
            sel ? lv_color_hex(0x2266FF) : lv_color_hex(0x222222), 0);
    }
    lv_obj_scroll_to_view(menuBtns[menuSel], LV_ANIM_ON);
}

static void build_menu(lv_obj_t *scr) {
    lv_obj_t *list = lv_obj_create(scr);
    lv_obj_set_size(list, 220, 220);
    lv_obj_center(list);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(list, lv_color_black(), 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_row(list, 8, 0);

    for (int i = 0; i < MENU_N; i++) {
        lv_obj_t *btn = lv_obj_create(list);
        lv_obj_set_size(btn, lv_pct(100), 44);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *l = lv_label_create(btn);
        lv_obj_set_style_text_color(l, lv_color_white(), 0);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
        lv_label_set_text(l, MENU_ITEMS[i]);
        lv_obj_center(l);
        menuBtns[i] = btn;
    }
    refresh_menu_highlight();
}

static void menu_activate() {
    switch (menuSel) {
        case 0: request_screen(SCR_NOTIFY); break;
        case 1: request_screen(SCR_MUSIC);  break;
        case 2: request_screen(SCR_REMOTE); break;
        case 3: request_screen(SCR_CAMERA); break;
        case 4: request_screen(SCR_QR);     break;
        case 5: request_screen(SCR_UPLOAD); break;
        case 6: request_screen(SCR_FIND);   break;
        case 7: request_screen(SCR_TIMER);  break;
        case 8: request_screen(SCR_DIAL);   break;
        case 9: request_screen(SCR_PET);    break;
        case 10: /* TODO: cai dat */        break;
    }
}

// ============================================================
//  NAVIGATION
// ============================================================
static void build_nav(lv_obj_t *scr) {
    g_routeCount = 0;   // bo lo trinh cu

    // --- Duong line lo trinh (nen) ---
    navLine = lv_line_create(scr);
    lv_obj_set_pos(navLine, 0, 0);
    lv_obj_set_size(navLine, SCREEN_W, SCREEN_H);
    lv_obj_set_style_line_width(navLine, 6, 0);
    lv_obj_set_style_line_color(navLine, lv_color_hex(0x2E7DFF), 0);
    lv_obj_set_style_line_rounded(navLine, true, 0);

    // --- Cham vi tri hien tai ---
    navDot = lv_obj_create(scr);
    lv_obj_set_size(navDot, 16, 16);
    lv_obj_set_style_radius(navDot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(navDot, lv_color_hex(0xFFCC00), 0);
    lv_obj_set_style_border_width(navDot, 2, 0);
    lv_obj_set_style_border_color(navDot, lv_color_white(), 0);
    lv_obj_clear_flag(navDot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(navDot, ROUTE_CX - 8, ROUTE_CY - 8);

    // --- Mui ten huong re (tren cung) ---
    navArrow = lv_label_create(scr);
    lv_obj_set_style_text_font(navArrow, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(navArrow, lv_color_hex(0x33CCFF), 0);
    lv_label_set_text(navArrow, LV_SYMBOL_GPS);
    lv_obj_align(navArrow, LV_ALIGN_TOP_MID, 0, 2);

    navDist = lv_label_create(scr);
    lv_obj_set_style_text_font(navDist, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(navDist, lv_color_white(), 0);
    lv_label_set_text(navDist, "--");
    lv_obj_align(navDist, LV_ALIGN_TOP_MID, 0, 52);

    // --- Ten duong (duoi cung, co nen mo de de doc) ---
    navStreet = lv_label_create(scr);
    lv_obj_set_style_text_font(navStreet, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(navStreet, lv_color_white(), 0);
    lv_obj_set_style_bg_color(navStreet, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(navStreet, LV_OPA_60, 0);
    lv_obj_set_style_pad_all(navStreet, 4, 0);
    lv_label_set_long_mode(navStreet, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(navStreet, 224);
    lv_obj_set_style_text_align(navStreet, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(navStreet, "Cho dinh tuyen...");
    lv_obj_align(navStreet, LV_ALIGN_BOTTOM_MID, 0, -2);

    navEta = nullptr;
}

// Ve lai duong line tu du lieu nhan qua BLE
static void route_refresh() {
    if (g_cur != SCR_NAV || !navLine) return;
    if (!g_routeDirty) return;
    g_routeDirty = false;
    int n = g_routeCount;
    if (n > MAX_ROUTE_PTS) n = MAX_ROUTE_PTS;
    if (n < 2) { lv_line_set_points(navLine, g_linePts, 0); return; }
    for (int i = 0; i < n; i++) {
        g_linePts[i].x = ROUTE_CX + g_routeXY[i * 2];
        g_linePts[i].y = ROUTE_CY + g_routeXY[i * 2 + 1];
    }
    lv_line_set_points(navLine, g_linePts, n);
}

static void update_nav() {
    if (g_cur != SCR_NAV) return;
    if (!g_nav.active) {
        lv_label_set_text(navArrow, LV_SYMBOL_GPS);
        lv_label_set_text(navDist, "--");
        lv_label_set_text(navStreet, "Chua co lo trinh");
        return;
    }
    lv_label_set_text(navArrow, maneuver_symbol(g_nav.maneuver));

    char b[24];
    if (g_nav.distance_m >= 1000) snprintf(b, sizeof(b), "%.1f km", g_nav.distance_m / 1000.0);
    else                          snprintf(b, sizeof(b), "%lu m", (unsigned long)g_nav.distance_m);
    lv_label_set_text(navDist, b);

    lv_label_set_text(navStreet, g_nav.street[0] ? g_nav.street : "");
}

// ============================================================
//  THONG BAO
// ============================================================
static void build_notify(lv_obj_t *scr) {
    g_notify.hasNew = false;   // da xem

    lv_obj_t *t = lv_label_create(scr);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(0x66CC66), 0);
    lv_label_set_text(t, LV_SYMBOL_BELL " Thong bao");
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 8);

    lblNApp = lv_label_create(scr);
    lv_obj_set_style_text_font(lblNApp, &vn_font_16, 0);
    lv_obj_set_style_text_color(lblNApp, lv_color_hex(0x33CCFF), 0);
    lv_obj_align(lblNApp, LV_ALIGN_TOP_MID, 0, 40);

    lblNTitle = lv_label_create(scr);
    lv_obj_set_style_text_font(lblNTitle, &vn_font_20, 0);
    lv_obj_set_style_text_color(lblNTitle, lv_color_white(), 0);
    lv_obj_set_width(lblNTitle, 214);
    lv_label_set_long_mode(lblNTitle, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(lblNTitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lblNTitle, LV_ALIGN_TOP_MID, 0, 64);

    lblNText = lv_label_create(scr);
    lv_obj_set_style_text_font(lblNText, &vn_font_16, 0);
    lv_obj_set_style_text_color(lblNText, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_width(lblNText, 214);
    lv_label_set_long_mode(lblNText, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(lblNText, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lblNText, LV_ALIGN_CENTER, 0, 24);

    lv_obj_t *hint = lv_label_create(scr);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), 0);
    lv_label_set_text(hint, g_replies.count > 0
                            ? "B = Tra loi   C = Quay lai" : "Nhan C = Quay lai");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);
}

static void update_notify() {
    if (g_cur != SCR_NOTIFY || !lblNTitle) return;
    if (g_notify.title[0] == 0 && g_notify.text[0] == 0) {
        lv_label_set_text(lblNApp, "");
        lv_label_set_text(lblNTitle, "Khong co thong bao");
        lv_label_set_text(lblNText, "");
        return;
    }
    lv_label_set_text(lblNApp, g_notify.app);
    lv_label_set_text(lblNTitle, g_notify.title);
    lv_label_set_text(lblNText, g_notify.text);
}

// ============================================================
//  NHAC
// ============================================================
static void build_music(lv_obj_t *scr) {
    lv_obj_t *t = lv_label_create(scr);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(0xE5A23B), 0);
    lv_label_set_text(t, LV_SYMBOL_AUDIO " Nhac");
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 8);

    lblMTitle = lv_label_create(scr);
    lv_obj_set_style_text_font(lblMTitle, &vn_font_20, 0);
    lv_obj_set_style_text_color(lblMTitle, lv_color_white(), 0);
    lv_obj_set_width(lblMTitle, 214);
    lv_label_set_long_mode(lblMTitle, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(lblMTitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lblMTitle, LV_ALIGN_TOP_MID, 0, 48);

    lblMArtist = lv_label_create(scr);
    lv_obj_set_style_text_font(lblMArtist, &vn_font_16, 0);
    lv_obj_set_style_text_color(lblMArtist, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_width(lblMArtist, 214);
    lv_label_set_long_mode(lblMArtist, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(lblMArtist, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lblMArtist, LV_ALIGN_TOP_MID, 0, 82);

    lblMState = lv_label_create(scr);
    lv_obj_set_style_text_font(lblMState, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lblMState, lv_color_hex(0x33CCFF), 0);
    lv_label_set_text(lblMState, LV_SYMBOL_PLAY);
    lv_obj_align(lblMState, LV_ALIGN_CENTER, 0, 24);

    lv_obj_t *hint = lv_label_create(scr);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x777777), 0);
    lv_label_set_text(hint, "A:sau  B:phat/dung  C:thoat");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);
}

static void update_music() {
    if (g_cur != SCR_MUSIC || !lblMTitle) return;
    lv_label_set_text(lblMTitle, g_music.title[0] ? g_music.title : "Chua co nhac");
    lv_label_set_text(lblMArtist, g_music.artist);
    lv_label_set_text(lblMState, g_music.playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
}

// ============================================================
//  REMOTE PC (HID ban phim Bluetooth)
// ============================================================
static void build_remote(lv_obj_t *scr) {
    ble_stop();    // ngat BLE thuong de bat HID (C3 chi 1 cau hinh BLE 1 luc)
    hid_start();

    lv_obj_t *t = lv_label_create(scr);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(t, lv_color_white(), 0);
    lv_label_set_text(t, LV_SYMBOL_KEYBOARD " Remote PC");
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 14);

    lblRemote = lv_label_create(scr);
    lv_obj_set_style_text_font(lblRemote, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lblRemote, lv_color_hex(0xE5A23B), 0);
    lv_obj_set_width(lblRemote, 214);
    lv_label_set_long_mode(lblRemote, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(lblRemote, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lblRemote,
        "May tinh: vao Bluetooth\nghep voi \"VHI-Remote\"");
    lv_obj_align(lblRemote, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t *hint = lv_label_create(scr);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), 0);
    lv_label_set_text(hint, "A:slide sau  B:slide truoc  C:thoat");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);
}

static void update_remote() {
    if (!lblRemote) return;
    if (g_cur == SCR_REMOTE) {
        if (hid_connected()) {
            lv_obj_set_style_text_color(lblRemote, lv_color_hex(0x66CC66), 0);
            lv_label_set_text(lblRemote, "Da ket noi may tinh!\nBam A/B de lat slide");
        } else {
            lv_obj_set_style_text_color(lblRemote, lv_color_hex(0xE5A23B), 0);
            lv_label_set_text(lblRemote, "May tinh: vao Bluetooth\nghep voi \"VHI-Remote\"");
        }
    } else if (g_cur == SCR_CAMERA) {
        if (hid_connected()) {
            lv_obj_set_style_text_color(lblRemote, lv_color_hex(0x66CC66), 0);
            lv_label_set_text(lblRemote, "San sang! Mo Camera dien thoai\nBam A/B de chup");
        } else {
            lv_obj_set_style_text_color(lblRemote, lv_color_hex(0xE5A23B), 0);
            lv_label_set_text(lblRemote, "Dang ket noi... neu lau,\nghep BT \"VHI-Remote\"");
        }
    }
}

// ============================================================
//  CHUP HINH (HID gui phim Volume Up - kich hoat Camera)
// ============================================================
static void build_camera(lv_obj_t *scr) {
    // 1) Bao app dien thoai mo APP MAY ANH GOC, 2) chuyen dong ho sang che do
    //    HID (ban phim BT) de nut bam = phim Volume = chup hinh tren camera goc.
    bool wasConnected = g_sys.bleConnected;
    if (wasConnected) { ble_send_media("camera_open"); delay(700); }  // doi app mo camera
    ble_stop();      // tat BLE thuong (C3 chi chay 1 cau hinh BLE 1 luc)
    hid_start();     // bat HID -> dien thoai nhan dong ho la remote chup hinh

    lv_obj_t *t = lv_label_create(scr);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(t, lv_color_white(), 0);
    lv_label_set_text(t, LV_SYMBOL_IMAGE " Chup hinh");
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 14);

    lv_obj_t *icon = lv_label_create(scr);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(0x33CCFF), 0);
    lv_label_set_text(icon, LV_SYMBOL_IMAGE);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -20);

    lblRemote = lv_label_create(scr);
    lv_obj_set_style_text_font(lblRemote, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lblRemote, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_width(lblRemote, 214);
    lv_label_set_long_mode(lblRemote, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(lblRemote, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lblRemote,
        wasConnected ? "Camera dien thoai dang mo.\nDua may len chup!"
                     : "Hay ket noi dien thoai truoc!");
    lv_obj_align(lblRemote, LV_ALIGN_CENTER, 0, 36);

    lv_obj_t *hint = lv_label_create(scr);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0xE5A23B), 0);
    lv_label_set_text(hint, "A/B: CHUP   C: thoat");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);
}

// ============================================================
//  TIM DIEN THOAI (dong ho ra lenh -> dien thoai reo to + rung)
// ============================================================
static void build_find(lv_obj_t *scr) {
    if (g_sys.bleConnected) ble_send_media("findphone");   // reo dien thoai ngay

    lv_obj_t *t = lv_label_create(scr);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(t, lv_color_white(), 0);
    lv_label_set_text(t, LV_SYMBOL_CALL " Tim dien thoai");
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 14);

    lv_obj_t *icon = lv_label_create(scr);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(0x33CC66), 0);
    lv_label_set_text(icon, LV_SYMBOL_BELL);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t *info = lv_label_create(scr);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(info, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_width(info, 214);
    lv_label_set_long_mode(info, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(info,
        g_sys.bleConnected ? "Dang reo dien thoai..." : "Hay ket noi dien thoai truoc!");
    lv_obj_align(info, LV_ALIGN_CENTER, 0, 36);

    lv_obj_t *hint = lv_label_create(scr);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0xE5A23B), 0);
    lv_label_set_text(hint, "A/B: reo lai   C: tat & thoat");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);
}

// ============================================================
//  THE TEN THONG MINH (QR code)
// ============================================================
// Ve man QR THANG ra man hinh (it RAM, on dinh hon lv_img full-screen)
static void qr_draw() {
    char path[20];
    snprintf(path, sizeof(path), QR_IMG_PATH_FMT, qrIdx);
    if (!display_draw_image_file(path)) {       // chua co anh
        display_fill(0x0000);
        display_text(54, 100, "Chua co QR", 0xFFFF, 0x0000);
        display_text(28, 128, "Upload tu app", 0x8410, 0x0000);
    }
    char nm[16];
    snprintf(nm, sizeof(nm), "QR %d/%d", qrIdx + 1, QR_COUNT);
    display_text(3, 2, nm, 0xFFFF, 0x0000);                 // nhan o (goc tren-trai)
    display_text(3, 222, "A doi C thoat", 0xFFFF, 0x0000);  // huong dan (goc duoi)
}

static void build_qr(lv_obj_t *scr) {
    display_set_raw(true);   // ve thang ra man, LVGL khong de len
    qr_draw();
}

// ============================================================
//  UPLOAD (WiFi AP)
// ============================================================
static void build_upload(lv_obj_t *scr) {
    // Anh nen gui qua Bluetooth (khong dung WiFi nua)
    lv_obj_t *t = lv_label_create(scr);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(t, lv_color_white(), 0);
    lv_label_set_text(t, LV_SYMBOL_IMAGE " Doi anh nen");
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 16);

    lv_obj_t *info = lv_label_create(scr);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(info, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_width(info, 210);
    lv_label_set_long_mode(info, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(info,
        "Mo app dien thoai\n(da ket noi Bluetooth)\n\n"
        "Tab \"Anh nen\"\n-> Chon anh -> Gui\n\n"
        "Anh tu hien sau ~30 giay");
    lv_obj_center(info);

    lv_obj_t *hint = lv_label_create(scr);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), 0);
    lv_label_set_text(hint, "Nhan C = Quay lai");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);
}

// ============================================================
//  XU LY PHIM (gan vao container goc moi man hinh)
// ============================================================
// Hoan viec doi man hinh ra ngoai event handler (LVGL goi lai o vong timer ke tiep)
static void switch_screen_cb(void *unused) {
    if (g_pendingScreen >= 0) {
        Screen s = (Screen)g_pendingScreen;
        g_pendingScreen = -1;
        show_screen(s);
    }
}
static void request_screen(Screen s) {
    g_pendingScreen = (int)s;
    lv_async_call(switch_screen_cb, NULL);
}

static void key_handler(lv_event_t *e) {
    uint32_t key = lv_event_get_key(e);

    switch (g_cur) {
        case SCR_WATCH:
            if (key == LV_KEY_ENTER) request_screen(SCR_MENU);
            else if (key == LV_KEY_DOWN) {          // A: doi mat dong ho
                wfStyle = (wfStyle + 1) % WF_STYLE_N;
                wf_save_style();
                draw_wf_full();
            }
            break;

        case SCR_MENU:
            if (key == LV_KEY_DOWN)       { menuSel = (menuSel + 1) % MENU_N; refresh_menu_highlight(); }
            else if (key == LV_KEY_UP)    { menuSel = (menuSel - 1 + MENU_N) % MENU_N; refresh_menu_highlight(); }
            else if (key == LV_KEY_ENTER) menu_activate();
            else if (key == LV_KEY_ESC)   request_screen(SCR_WATCH);
            break;

        case SCR_NAV:
            if (key == LV_KEY_ESC) request_screen(SCR_MENU);
            break;

        case SCR_UPLOAD:
            if (key == LV_KEY_ESC) request_screen(SCR_MENU);  // show_screen tu tat WiFi + bat lai BLE
            break;

        case SCR_NOTIFY:
            if (key == LV_KEY_ENTER) request_screen(SCR_REPLY);   // B: mo danh sach cau tra loi
            else if (key == LV_KEY_ESC) request_screen(SCR_MENU);
            break;

        case SCR_REPLY:
            if (g_replies.count > 0) {
                if (key == LV_KEY_DOWN)      { replySel = (replySel + 1) % g_replies.count; reply_refresh(); }
                else if (key == LV_KEY_UP)   { replySel = (replySel - 1 + g_replies.count) % g_replies.count; reply_refresh(); }
                else if (key == LV_KEY_ENTER) {                     // B: gui cau tra loi
                    if (g_notify.canReply) {
                        char cmd[16]; snprintf(cmd, sizeof(cmd), "reply:%d", replySel);
                        ble_send_media(cmd);
                        g_notify.canReply = false;                  // da tra loi
                        if (lblReplyInfo) {
                            lv_label_set_text(lblReplyInfo, "Da gui!");
                            lv_obj_set_style_text_color(lblReplyInfo, lv_color_hex(0x33CC66), 0);
                        }
                    } else if (lblReplyInfo) {                       // app khong ho tro tra loi nhanh (vd Zalo)
                        lv_label_set_text(lblReplyInfo, "App nay ko ho tro tra loi");
                        lv_obj_set_style_text_color(lblReplyInfo, lv_color_hex(0xFF5555), 0);
                    }
                }
            }
            if (key == LV_KEY_ESC) request_screen(SCR_WATCH);       // C: thoat
            break;

        case SCR_MUSIC:
            if (key == LV_KEY_DOWN)       ble_send_media("next");
            else if (key == LV_KEY_UP)    ble_send_media("prev");
            else if (key == LV_KEY_ENTER) ble_send_media("playpause");
            else if (key == LV_KEY_ESC)   request_screen(SCR_MENU);
            break;

        case SCR_REMOTE:
            if (key == LV_KEY_DOWN)       hid_send_key(KEY_RIGHT_ARROW);  // A: slide sau
            else if (key == LV_KEY_ENTER) hid_send_key(KEY_LEFT_ARROW);   // B: slide truoc
            else if (key == LV_KEY_ESC)   request_screen(SCR_MENU);       // C: thoat
            break;

        case SCR_CAMERA:
            // A/B: gui phim Volume (HID) -> camera goc tren dien thoai chup hinh
            if (key == LV_KEY_DOWN || key == LV_KEY_ENTER) hid_send_consumer(CC_VOLUME_UP);
            else if (key == LV_KEY_ESC) request_screen(SCR_MENU);   // C: thoat (show_screen tu tat HID, bat lai BLE)
            break;

        case SCR_QR:
            if (key == LV_KEY_DOWN || key == LV_KEY_UP || key == LV_KEY_ENTER) {
                qrIdx = (qrIdx + 1) % QR_COUNT; qr_draw();   // A/B: doi o QR (ve lai truc tiep)
            } else if (key == LV_KEY_ESC) request_screen(SCR_MENU);      // C: thoat
            break;

        case SCR_FIND:
            if (key == LV_KEY_DOWN || key == LV_KEY_ENTER) ble_send_media("findphone");   // A/B: reo lai
            else if (key == LV_KEY_ESC) { ble_send_media("findphone_stop"); request_screen(SCR_MENU); } // C: tat reo + thoat
            break;

        case SCR_TIMER:
            if (key == LV_KEY_ENTER)      tmr_toggle();               // B: chay/dung
            else if (key == LV_KEY_DOWN)  tmr_reset();                // A ngan: reset
            else if (key == LV_KEY_UP)    tmr_cycle();                // A giu: doi che do
            else if (key == LV_KEY_ESC)   request_screen(SCR_MENU);   // C: thoat
            break;

        case SCR_CALL:
            if (key == LV_KEY_DOWN)  { ble_send_media("call_reject"); g_call.ringing = false; request_screen(SCR_WATCH); } // A: tu choi
            else if (key == LV_KEY_ENTER) { ble_send_media("call_answer"); g_call.ringing = false; request_screen(SCR_WATCH); } // B: nghe
            else if (key == LV_KEY_ESC)   request_screen(SCR_WATCH);   // C: bo qua
            break;

        case SCR_PET:
            if (petMenuOpen) {                                  // dang mo menu chon animation
                if (key == LV_KEY_DOWN)       petMenuSel = (petMenuSel + 1) % pet_menu_n();   // B: xuong
                else if (key == LV_KEY_ENTER) {                 // A: chon
                    int act = pet_menu_act(petMenuSel);
                    if (act < 0) { petLock = false; petAct = 0; petTAct = millis() + 600; } // Tu dong
                    else         { petLock = true;  petAct = act; petActUntil = millis() + 3600000;
                                   if (act == 6 || act == 5 || act == 1) { pexN = 0; peyN = 0; } }
                    petMenuOpen = false;
                }
                else if (key == LV_KEY_ESC)   petMenuOpen = false;        // C: dong menu
            } else {
                if (key == LV_KEY_ENTER)      { petMenuOpen = true; petMenuSel = 0; } // A: mo menu
                else if (key == LV_KEY_DOWN)  {                          // A: choc gheo -> buc -> gian
                    uint32_t nowp = millis();
                    petPokeCnt = (nowp - petLastPoke < 1500) ? petPokeCnt + 1 : 1;   // choc don dap trong 1.5s
                    petLastPoke = nowp;
                    petLock = false;
                    if (petPokeCnt >= 4)      { petAct = 5;  petActUntil = nowp + 2400; petPokeCnt = 0; } // gian du
                    else if (petPokeCnt == 3) { petAct = 3;  petActUntil = nowp + 1300; }                 // lac dau kho chiu
                    else                      { petAct = 13; petActUntil = nowp + 1100; }                 // giat minh "?"
                }
                else if (key == LV_KEY_ESC)   request_screen(SCR_MENU);   // C: quay lai menu
            }
            break;

        case SCR_DIAL:
            if (g_contacts.count > 0) {
                if (key == LV_KEY_DOWN)      { dialSel = (dialSel + 1) % g_contacts.count; dial_refresh(); }
                else if (key == LV_KEY_UP)   { dialSel = (dialSel - 1 + g_contacts.count) % g_contacts.count; dial_refresh(); }
                else if (key == LV_KEY_ENTER) {                     // B: goi
                    char cmd[40];
                    snprintf(cmd, sizeof(cmd), "dial:%s", g_contacts.items[dialSel].number);
                    ble_send_media(cmd);
                    if (lblDialInfo) {
                        char m[48]; snprintf(m, sizeof(m), "Dang goi %s...", g_contacts.items[dialSel].name);
                        lv_label_set_text(lblDialInfo, m);
                        lv_obj_set_style_text_color(lblDialInfo, lv_color_hex(0x33CC66), 0);
                    }
                }
            }
            if (key == LV_KEY_ESC) request_screen(SCR_MENU);        // C: thoat
            break;
    }
}

// ============================================================
//  BAM GIO / POMODORO
// ============================================================
static int  tmrMode = 0;             // 0=25p, 1=5p, 2=15p, 3=bam gio (dem len)
static bool tmrRunning = false, tmrDone = false;
static uint32_t tmrStartMs = 0;
static int  tmrAccumSec = 0;
static const int   TMR_PRESET[] = {25 * 60, 5 * 60, 15 * 60, 0};
static const char *TMR_NAME[]   = {"Pomodoro 25", "Nghi 5 phut", "Tap trung 15", "Bam gio len"};
static lv_obj_t *lblTmrTime = nullptr, *lblTmrMode = nullptr;

static int tmr_elapsed() {
    int el = tmrAccumSec;
    if (tmrRunning) el += (int)((millis() - tmrStartMs) / 1000);
    return el;
}
static void tmr_set_mode_label() {
    if (!lblTmrMode) return;
    char b[40];
    snprintf(b, sizeof(b), "%s  %s", TMR_NAME[tmrMode], tmrRunning ? "(dang chay)" : "(tam dung)");
    lv_label_set_text(lblTmrMode, b);
    lv_obj_set_style_text_color(lblTmrMode, lv_color_hex(0xBBBBBB), 0);
}
static void tmr_show_value(int val) {
    if (!lblTmrTime) return;
    if (val < 0) val = 0;
    char b[8]; snprintf(b, sizeof(b), "%02d:%02d", val / 60, val % 60);
    lv_label_set_text(lblTmrTime, b);
}
static void tmr_reset() {
    tmrRunning = false; tmrDone = false; tmrAccumSec = 0;
    tmr_set_mode_label();
    tmr_show_value(tmrMode == 3 ? 0 : TMR_PRESET[tmrMode]);
}
static void tmr_toggle() {
    if (tmrDone) { tmr_reset(); return; }          // het gio -> nhan B = lam lai
    if (tmrRunning) { tmrAccumSec = tmr_elapsed(); tmrRunning = false; }
    else { tmrStartMs = millis(); tmrRunning = true; }
    tmr_set_mode_label();
}
static void tmr_cycle() { tmrMode = (tmrMode + 1) % 4; tmr_reset(); }

static void update_timer() {
    if (g_cur != SCR_TIMER || !lblTmrTime) return;
    int val;
    if (tmrMode == 3) {
        val = tmr_elapsed();                        // dem len
    } else {
        int rem = TMR_PRESET[tmrMode] - tmr_elapsed();
        if (rem <= 0 && tmrRunning) {               // het gio -> bao
            tmrRunning = false; tmrDone = true; tmrAccumSec = TMR_PRESET[tmrMode];
            lv_label_set_text(lblTmrMode, "HET GIO ! (B de lam lai)");
            lv_obj_set_style_text_color(lblTmrMode, lv_color_hex(0xFF5555), 0);
        }
        val = rem < 0 ? 0 : rem;
    }
    tmr_show_value(val);
}

static void build_timer(lv_obj_t *scr) {
    lblTmrMode = lv_label_create(scr);
    lv_obj_set_style_text_font(lblTmrMode, &lv_font_montserrat_16, 0);
    lv_obj_align(lblTmrMode, LV_ALIGN_TOP_MID, 0, 28);

    lblTmrTime = lv_label_create(scr);
    lv_obj_set_style_text_font(lblTmrTime, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lblTmrTime, lv_color_white(), 0);
    lv_obj_center(lblTmrTime);

    lv_obj_t *hint = lv_label_create(scr);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0xE5A23B), 0);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(hint, "B: chay/dung   A: reset\ngiu A: doi che do   C: thoat");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -6);

    tmr_reset();
}

// ============================================================
//  CUOC GOI DEN
// ============================================================
static void build_call(lv_obj_t *scr) {
    lv_obj_t *t = lv_label_create(scr);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(0x33CC66), 0);
    lv_label_set_text(t, LV_SYMBOL_CALL " Cuoc goi den");
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 16);

    lv_obj_t *app = lv_label_create(scr);
    lv_obj_set_style_text_font(app, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(app, lv_color_hex(0x999999), 0);
    lv_label_set_text(app, g_call.app[0] ? g_call.app : "Dien thoai");
    lv_obj_align(app, LV_ALIGN_TOP_MID, 0, 44);

    lv_obj_t *name = lv_label_create(scr);
    lv_obj_set_style_text_font(name, &vn_font_20, 0);
    lv_obj_set_style_text_color(name, lv_color_white(), 0);
    lv_obj_set_width(name, 220);
    lv_label_set_long_mode(name, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(name, g_call.name[0] ? g_call.name : "Khong ro so");
    lv_obj_align(name, LV_ALIGN_CENTER, 0, -6);

    lv_obj_t *rej = lv_label_create(scr);
    lv_obj_set_style_text_font(rej, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(rej, lv_color_hex(0xFF4444), 0);
    lv_label_set_text(rej, "A: TU CHOI");
    lv_obj_align(rej, LV_ALIGN_BOTTOM_LEFT, 10, -8);

    lv_obj_t *ans = lv_label_create(scr);
    lv_obj_set_style_text_font(ans, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ans, lv_color_hex(0x33CC66), 0);
    lv_label_set_text(ans, "B: NGHE");
    lv_obj_align(ans, LV_ALIGN_BOTTOM_RIGHT, -10, -8);
}

// ============================================================
//  GOI DIEN (danh ba nhanh dong bo tu app)
// ============================================================
static void dial_refresh() {
    for (int i = 0; i < g_contacts.count; i++)
        lv_obj_set_style_bg_color(dialBtns[i],
            i == dialSel ? lv_color_hex(0x2266FF) : lv_color_hex(0x222222), 0);
    if (g_contacts.count > 0) lv_obj_scroll_to_view(dialBtns[dialSel], LV_ANIM_ON);
}

static void build_dial(lv_obj_t *scr) {
    if (dialSel >= g_contacts.count) dialSel = 0;

    if (g_contacts.count == 0) {
        lv_obj_t *l = lv_label_create(scr);
        lv_obj_set_style_text_font(l, &vn_font_16, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(0xCCCCCC), 0);
        lv_obj_set_width(l, 210);
        lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(l, "Chua co danh ba.\nMo app -> tab Goi nhanh de them.");
        lv_obj_center(l);
        return;
    }

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x33CC66), 0);
    lv_label_set_text(title, LV_SYMBOL_CALL " Goi dien");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    lv_obj_t *list = lv_obj_create(scr);
    lv_obj_set_size(list, 224, 166);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(list, lv_color_black(), 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_row(list, 6, 0);
    for (int i = 0; i < g_contacts.count; i++) {
        lv_obj_t *btn = lv_obj_create(list);
        lv_obj_set_size(btn, lv_pct(100), 40);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *l = lv_label_create(btn);
        lv_obj_set_style_text_font(l, &vn_font_16, 0);
        lv_obj_set_style_text_color(l, lv_color_white(), 0);
        lv_label_set_text(l, g_contacts.items[i].name);
        lv_obj_align(l, LV_ALIGN_LEFT_MID, 6, 0);
        dialBtns[i] = btn;
    }

    lblDialInfo = lv_label_create(scr);
    lv_obj_set_style_text_font(lblDialInfo, &vn_font_16, 0);
    lv_obj_set_style_text_color(lblDialInfo, lv_color_hex(0xE5A23B), 0);
    lv_label_set_text(lblDialInfo, "A: chon   B: GOI   C: thoat");
    lv_obj_align(lblDialInfo, LV_ALIGN_BOTTOM_MID, 0, -6);
    dial_refresh();
}

// ============================================================
//  TRA LOI NHANH (chon cau soan san -> gui ve dien thoai)
// ============================================================
static void reply_refresh() {
    for (int i = 0; i < g_replies.count; i++)
        lv_obj_set_style_bg_color(replyBtns[i],
            i == replySel ? lv_color_hex(0x2266FF) : lv_color_hex(0x222222), 0);
    if (g_replies.count > 0) lv_obj_scroll_to_view(replyBtns[replySel], LV_ANIM_ON);
}

static void build_reply(lv_obj_t *scr) {
    if (replySel >= g_replies.count) replySel = 0;

    if (g_replies.count == 0) {
        lv_obj_t *l = lv_label_create(scr);
        lv_obj_set_style_text_font(l, &vn_font_16, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(0xCCCCCC), 0);
        lv_obj_set_width(l, 210);
        lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(l, "Chua co cau tra loi.\nMo app -> Tra loi nhanh de them.");
        lv_obj_center(l);
        return;
    }

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, &vn_font_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x33CC66), 0);
    lv_label_set_text(title, LV_SYMBOL_EDIT " Tra loi");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    lv_obj_t *list = lv_obj_create(scr);
    lv_obj_set_size(list, 226, 166);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(list, lv_color_black(), 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_row(list, 6, 0);
    for (int i = 0; i < g_replies.count; i++) {
        lv_obj_t *btn = lv_obj_create(list);
        lv_obj_set_size(btn, lv_pct(100), 38);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *l = lv_label_create(btn);
        lv_obj_set_style_text_font(l, &vn_font_16, 0);
        lv_obj_set_style_text_color(l, lv_color_white(), 0);
        lv_label_set_text(l, g_replies.items[i]);
        lv_obj_align(l, LV_ALIGN_LEFT_MID, 6, 0);
        replyBtns[i] = btn;
    }

    lblReplyInfo = lv_label_create(scr);
    lv_obj_set_style_text_font(lblReplyInfo, &vn_font_16, 0);
    lv_obj_set_style_text_color(lblReplyInfo, lv_color_hex(0xE5A23B), 0);
    lv_label_set_text(lblReplyInfo, "A: chon   B: GUI   C: thoat");
    lv_obj_align(lblReplyInfo, LV_ALIGN_BOTTOM_MID, 0, -6);
    reply_refresh();
}

// ============================================================
//  VECTOR - mat robot hoat hinh (port tu FluxGarage RoboEyes)
//  Mat bo tron + easing muot + mat tu di lang thang (wander) + chop.
//  Cam xuc: vui = mi mat DUOI cong len; met/ngu = mi mat TREN xech.
// ============================================================
#define PET_CW 224
#define PET_CH 208   // cao gan het man (240) -> animation to hon, lap day man tot hon
#define EYE_W  72
#define EYE_H  76
#define EYE_R  22
#define EYE_SP 26
// Bo dem canvas ~79KB (224x176x2): cap phat DONG khi vao man Pet, giai phong khi thoat.
// Truoc day de static -> chiem 79KB RAM VINH VIEN ke ca khi khong xem Pet -> de het heap/treo.
// Nay chi ton khi dang o man Pet; luc o mat dong ho / dung tinh nang khac thi tra lai het.
static lv_color_t *petCanvasBuf = nullptr;
static lv_obj_t *petCanvas = nullptr;

static void pet_rrect(int x, int y, int w, int h, int r, lv_color_t c) {
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color = c; d.bg_opa = LV_OPA_COVER; d.radius = r;
    lv_canvas_draw_rect(petCanvas, x, y, w, h, &d);
}
static void pet_tri(int x0, int y0, int x1, int y1, int x2, int y2, lv_color_t c) {
    lv_point_t p[3] = {{(lv_coord_t)x0,(lv_coord_t)y0},{(lv_coord_t)x1,(lv_coord_t)y1},{(lv_coord_t)x2,(lv_coord_t)y2}};
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color = c; d.bg_opa = LV_OPA_COVER;
    lv_canvas_draw_polygon(petCanvas, p, 3, &d);
}
static void pet_circle(int cx, int cy, int r, lv_color_t c) {
    pet_rrect(cx - r, cy - r, 2 * r, 2 * r, r, c);
}
static void pet_quad(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, lv_color_t c) {
    lv_point_t p[4] = {{(lv_coord_t)x0,(lv_coord_t)y0},{(lv_coord_t)x1,(lv_coord_t)y1},
                       {(lv_coord_t)x2,(lv_coord_t)y2},{(lv_coord_t)x3,(lv_coord_t)y3}};
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color = c; d.bg_opa = LV_OPA_COVER;
    lv_canvas_draw_polygon(petCanvas, p, 4, &d);
}
static void pet_arc(int cx, int cy, int r, int a0, int a1, int w, lv_color_t c) {
    lv_draw_arc_dsc_t d; lv_draw_arc_dsc_init(&d);
    d.color = c; d.width = w; d.rounded = 1;
    lv_canvas_draw_arc(petCanvas, cx, cy, r, a0, a1, &d);
}
// mood: 0 none  1 happy(cuoi)  2 tired(ngu)  3 angry(gian)
// hlL/hlR: chieu cao rieng mat trai/phai (de nheo mat, curious phong to)
static void pet_render(int mood, int hlL, int hlR, int dx, int dy, lv_color_t col) {
    lv_canvas_fill_bg(petCanvas, lv_color_black(), LV_OPA_COVER);
    lv_color_t bg = lv_color_black();
    int ccx = PET_CW / 2 + (int)pex + dx, ccy = PET_CH / 2 + (int)pey + dy;
    int xL = ccx - EYE_SP / 2 - EYE_W;      // top-left goc mat trai
    int xR = ccx + EYE_SP / 2;
    int yTL = ccy - hlL / 2, yTR = ccy - hlR / 2;

    pet_rrect(xL, yTL, EYE_W, hlL, EYE_R, col);
    pet_rrect(xR, yTR, EYE_W, hlR, EYE_R, col);

    if (mood == 1) {                         // mi duoi cong len -> mat cuoi
        int oL = (int)(hlL * 0.55f), oR = (int)(hlR * 0.55f);
        pet_rrect(xL - 2, yTL + hlL - oL, EYE_W + 4, EYE_H, EYE_R, bg);
        pet_rrect(xR - 2, yTR + hlR - oR, EYE_W + 4, EYE_H, EYE_R, bg);
    } else if (mood == 2) {                  // mi tren xech NGOAI -> met/ngu
        int tL = hlL / 2, tR = hlR / 2;
        pet_tri(xL, yTL - 1, xL + EYE_W, yTL - 1, xL, yTL + tL, bg);
        pet_tri(xR, yTR - 1, xR + EYE_W, yTR - 1, xR + EYE_W, yTR + tR, bg);
    } else if (mood == 3) {                  // mi tren xech TRONG -> gian du
        int tL = hlL / 2, tR = hlR / 2;
        pet_tri(xL, yTL - 1, xL + EYE_W, yTL - 1, xL + EYE_W, yTL + tL, bg);
        pet_tri(xR, yTR - 1, xR + EYE_W, yTR - 1, xR, yTR + tR, bg);
    }
}

// Mat trai tim: dung anh heart-eyes (assets/heart.png). Sinh dong bang nhip dap (zoom
// phong to/thu nho kieu lub-dub) + nhun nhe -> bieu cam "yeu" song dong tu 1 khung hinh.
static void pet_love(uint32_t now) {
    lv_canvas_fill_bg(petCanvas, lv_color_black(), LV_OPA_COVER);

    // Nhip dap: 2 tan so gan nhau tao cam giac "lub-dub", dao quanh 100%
    float b = sinf(now * 0.010f) * 0.7f + sinf(now * 0.020f) * 0.3f;   // -1..1
    int zoom = 256 + (int)(b * 14);                 // ~95%..105% (256 = 100%), khong tran canvas
    int bob  = (int)(sinf(now * 0.006f) * 3);       // nhun nhe +-3px

    lv_draw_img_dsc_t idsc; lv_draw_img_dsc_init(&idsc);
    idsc.zoom     = zoom;
    idsc.pivot.x  = heart_img.header.w / 2;         // phong to quanh tam anh -> giu chinh giua
    idsc.pivot.y  = heart_img.header.h / 2;
    idsc.antialias = 0;

    int x = (PET_CW - heart_img.header.w) / 2;       // canh giua ngang
    int y = (PET_CH - heart_img.header.h) / 2 + bob; // canh giua doc + nhun
    lv_canvas_draw_img(petCanvas, x, y, &heart_img, &idsc);
}

// Ve 1 mat xoay tron oc (@_@) - nhieu vong ban kinh tang dan, xoay theo time
static void pet_swirl(int cx, int cy, int a, lv_color_t c) {
    for (int k = 0; k < 4; k++) {
        int r = 8 + k * 7;
        int s = a + k * 250;
        pet_arc(cx, cy, r, s, s + 230, 5, c);
    }
}

// Chong mat - 2 mat xoay tron oc + sao nhieu mau bay vong quanh dau (dizzy)
static void pet_dizzy(uint32_t now) {
    lv_canvas_fill_bg(petCanvas, lv_color_black(), LV_OPA_COVER);
    lv_color_t swc = lv_color_hex(0x9B6BFF);   // xoay mau tim
    int a  = (int)(now * 0.45f) % 360;
    int cy = PET_CH / 2 + 8;
    pet_swirl(PET_CW / 2 - EYE_SP / 2 - EYE_W / 2, cy, a, swc);
    pet_swirl(PET_CW / 2 + EYE_SP / 2 + EYE_W / 2, cy, a, swc);

    // 3 ngoi sao nhieu mau bay vong tren dau
    static const uint32_t starCol[3] = { 0xFFD23F, 0xFF4D6A, 0x4DE1FF };
    float base = now * 0.006f;
    int ocx = PET_CW / 2, ocy = 30;
    for (int i = 0; i < 3; i++) {
        float ang = base + i * 2.094f;          // 120 do cach nhau
        int sx = ocx + (int)(cosf(ang) * 46);
        int sy = ocy + (int)(sinf(ang) * 14);   // quy dao det (nhin nghieng)
        lv_color_t sc = lv_color_hex(starCol[i]);
        pet_circle(sx, sy, 6, sc);              // than sao
        pet_arc(sx, sy, 9, 0, 360, 2, sc);      // hao quang
    }
}

// ---- Mat cho man doc bao: rounded rect; khi chop thi thanh 2 vom cong ----
static void pet_read_eyes(int cx, int cyE, int w, int h, int r, bool blink, lv_color_t col) {
    int gap = 20;
    int lx = cx - gap / 2 - w, rx = cx + gap / 2;
    if (blink) {
        pet_arc(lx + w / 2, cyE + h / 2, w / 2, 200, 340, 6, col);
        pet_arc(rx + w / 2, cyE + h / 2, w / 2, 200, 340, 6, col);
    } else {
        pet_rrect(lx, cyE, w, h, r, col);
        pet_rrect(rx, cyE, w, h, r, col);
    }
}

// Doc bao (storyboard 10 pha): idle -> mo mat -> bao truot len -> lac vao vi tri
// -> DOC LAU (~8s, co nhun nhe + cham "..." + chop mat nhieu lan) -> ha bao -> ve idle.
// To bao = ANH THAT (bao_img), ho trong suot cho mat lo qua khe giua 2 trang.
static void pet_read(uint32_t now) {
    lv_canvas_fill_bg(petCanvas, lv_color_black(), LV_OPA_COVER);
    lv_color_t cyan = lv_color_hex(0x33E1FF);
    lv_color_t dim  = lv_color_hex(0x0E3A44);
    int cx = PET_CW / 2;

    const uint32_t T = 10300;
    uint32_t t = now % T;

    // --- To bao theo pha ---
    int yoff = 0, wob = 0; bool showPaper = true;
    if (t < 800)        showPaper = false;                                 // idle + mo mat
    else if (t < 1200)  yoff = (int)((1.0f - (t - 800) / 400.0f) * 130);   // truot len
    else if (t < 1450)  wob  = (int)(sinf((t - 1200) * 0.05f) * 4);        // lac nhe vao vi tri
    else if (t < 9500)  yoff = 0;                                          // DANG DOC (~8s)
    else if (t < 9850)  yoff = (int)(((t - 9500) / 350.0f) * 150);         // ha bao truot xuong
    else                showPaper = false;                                 // ve idle

    // --- Nhun nhe khi doc: nhap nho theo chieu doc + dung dua ngang ---
    bool reading = (t >= 1450 && t < 9500);
    int bob = 0, sway = 0;
    if (reading) {
        bob  = (int)(sinf(now * 0.005f) * 3);      // len xuong ~3px
        sway = (int)(sinf(now * 0.0031f) * 2);     // qua lai ~2px
    }

    // --- Mat: idle nho; doc to hon; mo mat phong to dan; chop nhieu lan khi doc ---
    int ew = 56, eh = 32;
    if (t >= 500 && t < 800)      { float k = (t - 500) / 300.0f; ew = 56 + (int)(k * 6); eh = 32 + (int)(k * 6); }
    else if (t >= 800 && t < 9850){ ew = 62; eh = 38; }
    bool blink = (t > 2800 && t < 2980) || (t > 4400 && t < 4580) ||
                 (t > 6000 && t < 6180) || (t > 7600 && t < 7780) || (t > 9000 && t < 9180);
    pet_read_eyes(cx, 60 + bob / 2, ew, eh, 14, blink, cyan);   // +16: canh giua canvas cao 208

    // --- To bao (anh that) truot theo yoff, nhun (bob) + dung dua (sway) ---
    if (showPaper) {
        lv_draw_img_dsc_t idsc; lv_draw_img_dsc_init(&idsc);
        int bx = (PET_CW - bao_img.header.w) / 2 + wob + sway;
        lv_canvas_draw_img(petCanvas, bx, 90 + yoff + bob, &bao_img, &idsc);
    }

    // --- Cham "..." khi dang doc (dom sang lan luot) ---
    if (reading) {
        int nd = ((t - 1450) / 350) % 4;
        for (int i = 0; i < 3; i++)
            pet_circle(cx - 16 + i * 16, 36 + bob / 2, 3, (i < nd) ? cyan : dim);
    }
}

// Ve tia set vang (2 tam giac tao hinh zic-zac)
static void pet_bolt(int cx, int cy, lv_color_t c) {
    pet_tri(cx + 5, cy - 16, cx - 10, cy + 3, cx + 2, cy + 1, c);
    pet_tri(cx - 5, cy + 16, cx + 10, cy - 3, cx - 2, cy - 1, c);
}

// An pho: flipbook 7 frame ANH THAT (eat_imgs tu assets/eat1..7). Chieu 1->7, giu lau
// frame cuoi (tim tim, no ne), co nhun nhe cho sinh dong, roi lap.
static void pet_pho(uint32_t now) {
    lv_canvas_fill_bg(petCanvas, lv_color_black(), LV_OPA_COVER);
    static const uint16_t DUR[EAT_FRAMES] = {700, 350, 350, 500, 350, 400, 1700};  // ms/frame
    uint32_t total = 0;
    for (int i = 0; i < EAT_FRAMES; i++) total += DUR[i];
    uint32_t t = now % total;
    int f = EAT_FRAMES - 1; uint32_t acc = 0;
    for (int i = 0; i < EAT_FRAMES; i++) { acc += DUR[i]; if (t < acc) { f = i; break; } }

    const lv_img_dsc_t *img = eat_imgs[f];
    int bob = (int)(sinf(now * 0.005f) * 2);            // nhun nhe
    lv_draw_img_dsc_t idsc; lv_draw_img_dsc_init(&idsc);
    int x = (PET_CW - img->header.w) / 2;
    int y = (PET_CH - img->header.h) / 2 + bob;
    lv_canvas_draw_img(petCanvas, x, y, img, &idsc);
}

// Tia sang 4 canh (ngoi sao lap lanh) - 2 hinh thoi long nhau
static void pet_spark(int cx, int cy, int r, lv_color_t c) {
    if (r < 3) r = 3;
    int t = r / 3 + 1;
    pet_quad(cx, cy - r, cx + t, cy, cx, cy + r, cx - t, cy, c);   // thoi doc
    pet_quad(cx - r, cy, cx, cy - t, cx + r, cy, cx, cy + t, c);   // thoi ngang
}

// ============================================================
//  Bieu cam moi (ve canvas): Ngap - Mat lap lanh - Ngo ngac "?"
// ============================================================

// NGAP: mat lim dim -> nham tit (2 vom cong) khi ha mieng to; mieng "O" phong ra +
// luoi hong, nuoc mat ri khoe ngoai, roi "z z z" bay len. Nhip ngap muot theo nua sin.
static void pet_yawn(uint32_t now) {
    lv_canvas_fill_bg(petCanvas, lv_color_black(), LV_OPA_COVER);
    lv_color_t cyan = lv_color_hex(0x33E1FF);
    lv_color_t pink = lv_color_hex(0xFF7AA0);
    int cx = PET_CW / 2, cy = PET_CH / 2;
    uint32_t T = 2900, t = now % T;

    float openv = (t < 1700) ? sinf((float)t / 1700.0f * 3.14159f) : 0.0f;   // do ha mieng 0..1
    int tiltY = -(int)(openv * 6);                          // hoi ngua dau

    int ew = EYE_W;
    int xL = cx - EYE_SP / 2 - ew;
    int xR = cx + EYE_SP / 2;
    int eyeCY = cy - 10 + tiltY;

    if (openv < 0.5f) {                                     // mat lim dim (mi tren xech)
        int eh = (int)(EYE_H * (0.66f - 0.22f * openv));
        int yT = eyeCY - eh / 2;
        pet_rrect(xL, yT, ew, eh, EYE_R, cyan);
        pet_rrect(xR, yT, ew, eh, EYE_R, cyan);
        pet_tri(xL, yT - 1, xL + ew, yT - 1, xL, yT + eh / 2, lv_color_black());
        pet_tri(xR, yT - 1, xR + ew, yT - 1, xR + ew, yT + eh / 2, lv_color_black());
    } else {                                               // nham tit: 2 vom cong ^^
        int r = ew / 2;
        pet_arc(xL + ew / 2, eyeCY + 6, r, 200, 340, 7, cyan);
        pet_arc(xR + ew / 2, eyeCY + 6, r, 200, 340, 7, cyan);
    }

    if (openv > 0.55f) {                                   // nuoc mat khoe ngoai
        pet_circle(xL - 5, eyeCY + 6, 3, cyan);
        pet_circle(xR + ew + 5, eyeCY + 6, 3, cyan);
    }

    if (openv > 0.05f) {                                   // mieng "O" phong ra
        int mw = 26 + (int)(openv * 14);
        int mh = (int)(openv * 60);
        int my = cy + 42 + tiltY;
        int rr = (mw < mh ? mw : mh) / 2;
        pet_rrect(cx - mw / 2, my - mh / 2, mw, mh, rr, cyan);
        int iw = mw - 8, ih = mh - 8;
        if (iw > 4 && ih > 4) {
            pet_rrect(cx - iw / 2, my - ih / 2, iw, ih, (iw < ih ? iw : ih) / 2, lv_color_black());
            if (mh > 30) {                                 // luoi hong
                int tw = iw - 8;
                pet_rrect(cx - tw / 2, my + ih / 2 - 11, tw, 9, 4, pink);
            }
        }
    }

    if (t > 1500) {                                        // "z z z" bay len (buon ngu)
        lv_draw_label_dsc_t d; lv_draw_label_dsc_init(&d);
        d.color = cyan; d.font = &lv_font_montserrat_16;
        int drift = ((t - 1500) / 90) % 22;
        for (int i = 0; i < 3; i++)
            lv_canvas_draw_text(petCanvas, cx + 34 + i * 12, cy - 30 - i * 16 - drift, 24, &d, "z");
    }
}

// MAT LAP LANH: mat to sang, ngoi sao trang lap lanh trong mat (co gian = twinkle),
// vai tia sang nhieu mau bay lo lung quanh dau. Ca mat nhun theo nhip phan khich.
static void pet_sparkle(uint32_t now) {
    lv_canvas_fill_bg(petCanvas, lv_color_black(), LV_OPA_COVER);
    lv_color_t cyan  = lv_color_hex(0x33E1FF);
    lv_color_t white = lv_color_hex(0xFFFFFF);
    int cx = PET_CW / 2, cy = PET_CH / 2;

    int bob = (int)(sinf(now * 0.013f) * 4);
    int ew = EYE_W, eh = EYE_H;
    int xL = cx - EYE_SP / 2 - ew;
    int xR = cx + EYE_SP / 2;
    int eyeCY = cy - 2 + bob;

    pet_rrect(xL, eyeCY - eh / 2, ew, eh, EYE_R, cyan);     // mat to mo
    pet_rrect(xR, eyeCY - eh / 2, ew, eh, EYE_R, cyan);

    float tw = 0.6f + 0.4f * sinf(now * 0.02f);            // twinkle
    int sr = 10 + (int)(tw * 12);
    pet_spark(xL + ew / 2, eyeCY, sr, white);
    pet_spark(xR + ew / 2, eyeCY, sr, white);
    pet_circle(xL + ew / 2 + 15, eyeCY - 16, 4, white);     // dom sang phu
    pet_circle(xR + ew / 2 + 15, eyeCY - 16, 4, white);

    static const uint32_t sc[5] = {0xFFFFFF, 0x33E1FF, 0xFFD23F, 0xFF7AD1, 0x8AF0A0};
    static const int bx[5] = { 22, PET_CW - 22, 60, PET_CW - 60, PET_CW / 2 };
    static const int by[5] = { 40, 40, 14, 14, 6 };
    float base = now * 0.005f;
    for (int i = 0; i < 5; i++) {
        float k = 0.5f + 0.5f * sinf(base * 2.0f + i * 1.7f);
        int dy = (int)(sinf(base + i) * 4);
        pet_spark(bx[i], by[i] + dy, 3 + (int)(k * 7), lv_color_hex(sc[i]));
    }
}

// NGO NGAC "?": dau hoi vang to nhun tren dau, mat le (1 to 1 nheo) + liec qua lai,
// dau hoi nhun len xuong. Kieu "ha? gi vay?".
static void pet_confused(uint32_t now) {
    lv_canvas_fill_bg(petCanvas, lv_color_black(), LV_OPA_COVER);
    lv_color_t cyan   = lv_color_hex(0x33E1FF);
    lv_color_t yellow = lv_color_hex(0xFFD23F);
    int cx = PET_CW / 2, cy = PET_CH / 2;

    float g  = sinf(now * 0.004f);                 // liec cham qua lai
    int gx   = (int)(g * 10);
    int tilt = (int)(sinf(now * 0.0022f) * 6);     // nghieng dau

    int ew = EYE_W;
    int xL = cx - EYE_SP / 2 - ew + gx;
    int xR = cx + EYE_SP / 2 + gx;
    int eyeCY = cy + 8;
    int ehL = EYE_H;                               // mat trai to (to mo)
    int ehR = (int)(EYE_H * 0.6f);                // mat phai nheo (kho hieu)
    pet_rrect(xL, eyeCY - ehL / 2 - tilt, ew, ehL, EYE_R, cyan);
    pet_rrect(xR, eyeCY - ehR / 2 + tilt, ew, ehR, EYE_R, cyan);

    int qx = cx + 46 + (int)(g * 6);              // "?" vang, goc tren-phai, nhun
    int qy = 30 + (int)(sinf(now * 0.006f) * 6);
    lv_draw_label_dsc_t d; lv_draw_label_dsc_init(&d);
    d.color = yellow; d.font = &lv_font_montserrat_28; d.align = LV_TEXT_ALIGN_CENTER;
    lv_canvas_draw_text(petCanvas, qx - 24, qy, 48, &d, "?");
}

// Menu chon animation - ve trang len canvas, dieu khien bang nut A/B/C
static void pet_menu_draw() {
    lv_canvas_fill_bg(petCanvas, lv_color_hex(0x0A0F1E), LV_OPA_COVER);
    lv_draw_label_dsc_t t; lv_draw_label_dsc_init(&t);
    t.color = lv_color_hex(0x33E1FF); t.font = &vn_font_20;
    t.align = LV_TEXT_ALIGN_CENTER;
    lv_canvas_draw_text(petCanvas, 0, 6, PET_CW, &t, "Chọn animation");

    int y0 = 30, rh = 16;
    for (int i = 0; i < pet_menu_n(); i++) {
        int y = y0 + i * rh;
        lv_draw_label_dsc_t d; lv_draw_label_dsc_init(&d);
        d.font = &vn_font_16;
        if (i == petMenuSel) {
            pet_rrect(14, y - 1, PET_CW - 28, rh, 4, lv_color_hex(0x1E5066));
            d.color = lv_color_white();
        } else {
            d.color = lv_color_hex(0x8FB7C7);
        }
        lv_canvas_draw_text(petCanvas, 26, y, PET_CW - 44, &d, pet_menu_name(i));
    }
}

static void build_pet(lv_obj_t *scr) {
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // Cap phat bo dem canvas ngay khi vao man Pet (giai phong khi thoat o show_screen).
    if (!petCanvasBuf)
        petCanvasBuf = (lv_color_t *)malloc((size_t)PET_CW * PET_CH * sizeof(lv_color_t));
    if (!petCanvasBuf) {                      // het RAM -> bao nhe, khong tao canvas (petCanvas = null)
        lv_obj_t *l = lv_label_create(scr);
        lv_obj_set_style_text_font(l, &vn_font_16, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(0xE5A23B), 0);
        lv_obj_set_width(l, 200);
        lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(l, "Khong du RAM cho Pet.\nNhan C de quay lai.");
        lv_obj_center(l);
        return;
    }

    petCanvas = lv_canvas_create(scr);
    lv_canvas_set_buffer(petCanvas, petCanvasBuf, PET_CW, PET_CH, LV_IMG_CF_TRUE_COLOR);
    lv_obj_center(petCanvas);

    pex = pexN = pey = peyN = 0; peh = pehN = EYE_H;
    petTWander = petTBlink = petTBlinkOpen = petHappyUntil = petLastFrame = 0;
    petAct = 0; petActUntil = 0; petTAct = 0;
    petMenuOpen = false; petLock = false; petMenuSel = 0;
    petPokeCnt = 0; petLastPoke = 0;
    pet_render(0, EYE_H, EYE_H, 0, 0, lv_color_hex(0x33E1FF));
}

// Goi moi vong loop() -> hoat hinh Vector (chi khi o man Pet)
void ui_fast_tick() {
    if (g_cur != SCR_PET || !petCanvas) return;
    uint32_t now = millis();
    if (now - petLastFrame < 40) return;    // ~25fps cho easing muot
    petLastFrame = now;

    // ---- Dang mo menu chon animation -> chi ve menu ----
    if (petMenuOpen) { pet_menu_draw(); return; }

    if (g_notify.hasNew && !petLock) { g_notify.hasNew = false; petAct = 7; petActUntil = now + 2800; }  // tin nhan -> mat trai tim

    // ---- Dao dien tu dong (chi khi KHONG khoa tro chon tay) ----
    if (!petLock) {
        if (petAct == 0 && now >= petTAct) {
            struct tm tmv;
            bool night = wf_get_time(tmv) && (tmv.tm_hour >= 22 || tmv.tm_hour < 6);
            // Chon tro theo trong so (tro co co 'dem' duoc nhan 4 luc ban dem)
            int total = 0;
            for (int i = 0; i < PET_TRICK_CNT; i++) {
                int w = PET_TRICKS[i].weight;
                if (w) total += (night && PET_TRICKS[i].night) ? w * 4 : w;
            }
            int r = (int)random(0, total);
            const PetTrick *pick = nullptr;
            for (int i = 0; i < PET_TRICK_CNT; i++) {
                int w = PET_TRICKS[i].weight;
                if (!w) continue;
                if (night && PET_TRICKS[i].night) w *= 4;
                if (r < w) { pick = &PET_TRICKS[i]; break; }
                r -= w;
            }
            if (pick) {
                petAct = pick->act;
                petActUntil = now + pick->dur;
                if (petAct == 4) petWinkEye = (int)random(0, 2);              // nhay mat: chon mat
                if (petAct == 6) { pexN = (random(0,2) ? 40 : -40); peyN = 0; } // to mo: nhin 1 ben
            }
            petTAct = now + 2600 + (uint32_t)random(0, 3200);   // lan quyet dinh ke tiep
        }
        if (petAct != 0 && now >= petActUntil) { petAct = 0; }   // het tro -> ve idle
    }

    // ---- Cac tro ve rieng, thoat som ----
    // Tro co ham ve rieng (love/dizzy/read/pho/yawn/sparkle/confused) -> ve va thoat som
    const PetTrick *cur = pet_trick_of(petAct);
    if (cur && cur->render) { cur->render(now); return; }

    // ---- Wander + chop mat chi khi idle (tro khac tu lo phan cua no) ----
    if (petAct == 0 || petAct == 1 || petAct == 5) {
        if (now >= petTWander) {
            pexN = (float)random(-36, 37);
            peyN = (float)random(-22, 23);
            petTWander = now + 800 + (uint32_t)random(0, 2200);
        }
    }
    // Chop mat (khong chop khi dang nhay/nhun de khoi doi nhau)
    if (petAct != 2 && petAct != 4) {
        if (now >= petTBlink && pehN >= EYE_H) { pehN = 6; petTBlinkOpen = now + 110; }
        if (pehN < EYE_H && now >= petTBlinkOpen) { pehN = EYE_H; petTBlink = now + 1600 + (uint32_t)random(0, 3600); }
    }

    // Easing muot toi dich
    pex += (pexN - pex) * 0.30f;
    pey += (peyN - pey) * 0.30f;
    peh += (pehN - peh) * 0.45f;

    // ---- Tinh tham so + MAU ve theo tro hien tai ----
    int mood = 0, hlL = (int)(peh + 0.5f), hlR = hlL, dx = 0, dy = 0;
    lv_color_t col = lv_color_hex(0x33E1FF);                    // cyan mac dinh
    switch (petAct) {
        case 1:  mood = 1; col = lv_color_hex(0x4DF08A); break; // cuoi mim -> xanh la
        case 2:  mood = 1; dy = (int)(sinf(now * 0.028f) * 10); // cuoi ha ha: nhun len xuong -> vang
                 hlL = hlR = EYE_H; col = lv_color_hex(0xFFD23F); break;
        case 3:  dx = (int)(sinf(now * 0.045f) * 14);           // boi roi: lac ngang -> tim
                 col = lv_color_hex(0xB37DFF); break;
        case 4:  if (petWinkEye == 0) hlL = 6; else hlR = 6;    // nhay 1 mat -> hong
                 col = lv_color_hex(0xFF8AD1); break;
        case 5:  mood = 3; col = lv_color_hex(0xFF4D4D); break; // gian du -> do
        case 6: {                                               // to mo: mat ben nhin to hon -> cam
                 if (pex > 8)      hlR += 14;
                 else if (pex < -8) hlL += 14;
                 col = lv_color_hex(0xFF9F40); break; }
        default: {
                 struct tm tm;                                  // dem khuya -> lim dim, xanh mo
                 if (wf_get_time(tm) && (tm.tm_hour >= 22 || tm.tm_hour < 6)) {
                     mood = 2; col = lv_color_hex(0x2A7AA8);
                 }
                 break; }
    }
    if (hlL < 4) hlL = 4;  if (hlR < 4) hlR = 4;
    pet_render(mood, hlL, hlR, dx, dy, col);
}

// ============================================================
//  Chuyen man hinh
// ============================================================
static void show_screen(Screen s) {
    // doc don dep man hinh cu
    lv_obj_t *old = g_scr;
    bool leavingPet = (g_cur == SCR_PET);   // roi man Pet -> tra lai ~79KB bo dem canvas
    if (g_cur == SCR_WATCH || g_cur == SCR_QR) display_set_raw(false);  // het man ve-raw
    if (g_cur == SCR_REMOTE || g_cur == SCR_CAMERA) { hid_stop(); ble_init(); } // tat HID, bat lai BLE thuong

    // reset con tro label
    lblTime = lblDate = lblStat = nullptr;
    lblTmrTime = lblTmrMode = nullptr;
    lblDialInfo = nullptr;
    lblReplyInfo = nullptr;
    petCanvas = nullptr;
    navArrow = navDist = navStreet = navEta = nullptr;
    navLine = navDot = nullptr;
    lblNApp = lblNTitle = lblNText = nullptr;
    lblMTitle = lblMArtist = lblMState = nullptr;
    lblRemote = nullptr;

    g_scr = make_root();
    g_cur = s;

    // container goc focusable nhan phim (phu toan man hinh, trong suot)
    lv_obj_t *focus = lv_obj_create(g_scr);
    lv_obj_set_size(focus, 1, 1);
    lv_obj_set_style_opa(focus, LV_OPA_0, 0);
    lv_obj_set_style_border_width(focus, 0, 0);
    lv_obj_add_event_cb(focus, key_handler, LV_EVENT_KEY, NULL);

    switch (s) {
        case SCR_WATCH:  build_watchface(g_scr); break;
        case SCR_MENU:   build_menu(g_scr);      break;
        case SCR_NAV:    build_nav(g_scr);       break;
        case SCR_UPLOAD: build_upload(g_scr);    break;
        case SCR_NOTIFY: build_notify(g_scr);    break;
        case SCR_MUSIC:  build_music(g_scr);     break;
        case SCR_REMOTE: build_remote(g_scr);    break;
        case SCR_CAMERA: build_camera(g_scr);    break;
        case SCR_QR:     build_qr(g_scr);        break;
        case SCR_FIND:   build_find(g_scr);      break;
        case SCR_TIMER:  build_timer(g_scr);     break;
        case SCR_CALL:   build_call(g_scr);      break;
        case SCR_DIAL:   build_dial(g_scr);      break;
        case SCR_REPLY:  build_reply(g_scr);     break;
        case SCR_PET:    build_pet(g_scr);       break;
    }

    lv_scr_load(g_scr);

    // dua focus container vao group keypad
    lv_group_remove_all_objs(g_group);
    lv_group_add_obj(g_group, focus);
    lv_group_focus_obj(focus);

    if (old) lv_obj_del(old);

    // Da xoa xong canvas cu -> gio moi giai phong bo dem Pet (79KB) cho tinh nang khac dung
    if (leavingPet && petCanvasBuf) { free(petCanvasBuf); petCanvasBuf = nullptr; }

    if (s == SCR_NAV)    update_nav();
    if (s == SCR_NOTIFY) update_notify();
    if (s == SCR_MUSIC)  update_music();
}

// ============================================================
//  API
// ============================================================
void ui_init(lv_group_t *group) {
    g_group = group;
    // Nap mau chu da luu (giu sau khi tat nguon)
    File cf = LittleFS.open("/uicolor.dat", "r");
    if (cf) {
        if (cf.size() >= 3) { g_uiR = cf.read(); g_uiG = cf.read(); g_uiB = cf.read(); }
        cf.close();
    }
    // Nap giao dien gio da luu (vi tri + co)
    File wf = LittleFS.open("/wfcfg.dat", "r");
    if (wf) {
        int n = wf.size();
        if (n >= 2) {
            int p = wf.read(), s = wf.read();
            if (p >= 0 && p <= 2) g_wfPos = p;
            if (s >= 3 && s <= 6) g_wfSize = s;
        }
        if (n >= 4) {
            int ds = wf.read(), sh = wf.read();
            if (ds >= 1 && ds <= 3) g_dateSize = ds;
            g_dateShow = sh ? 1 : 0;
        }
        if (n >= 7) { g_dateR = wf.read(); g_dateG = wf.read(); g_dateB = wf.read(); }
        wf.close();
    }
    show_screen(SCR_WATCH);   // man chinh = mat dong ho
}

void ui_reload_wallpaper() {
    if (g_cur == SCR_WATCH) {
        storage_free_wallpaper();
        show_screen(SCR_WATCH);   // ve lai voi anh moi
    }
}

bool ui_can_sleep() {
    return g_cur == SCR_WATCH;   // chi ngu khi dang o mat dong ho
}

void ui_tick() {
    // Vua nhan xong anh nen moi qua BLE -> nap lai
    if (g_wpUpdated) { g_wpUpdated = false; ui_reload_wallpaper(); }

    // Cuoc goi den: tu mo man cuoc goi; het goi -> dong lai
    if (g_call.changed) {
        g_call.changed = false;
        if (g_call.ringing && g_cur != SCR_CALL) request_screen(SCR_CALL);
        else if (!g_call.ringing && g_cur == SCR_CALL) request_screen(SCR_WATCH);
    }

    // Danh ba vua dong bo -> ve lai man goi dien neu dang mo
    if (g_contacts.updated) {
        g_contacts.updated = false;
        if (g_cur == SCR_DIAL) request_screen(SCR_DIAL);
    }

    // Cau tra loi nhanh vua dong bo -> ve lai neu dang mo
    if (g_replies.updated) {
        g_replies.updated = false;
        if (g_cur == SCR_REPLY) request_screen(SCR_REPLY);
    }

    // Co thong bao moi -> tu mo man Thong bao (khi dang o mat dong ho, khong phai luc co cuoc goi)
    if (g_notify.hasNew && g_cur == SCR_WATCH && !g_call.ringing) request_screen(SCR_NOTIFY);

    // Doi mau chu (tu app) -> ve lai gio ngay neu dang o mat dong ho
    if (g_colorChanged) {
        g_colorChanged = false;
        if (g_cur == SCR_WATCH) draw_wf_time();
    }

    // Doi giao dien gio (vi tri/co) tu app -> luu flash + nhay ve mat "So lon" + ve lai
    if (g_wfCfgChanged) {
        g_wfCfgChanged = false;
        File f = LittleFS.open("/wfcfg.dat", "w");
        if (f) {
            uint8_t d[7] = {g_wfPos, g_wfSize, g_dateSize, g_dateShow, g_dateR, g_dateG, g_dateB};
            f.write(d, 7); f.close();
        }
        wfStyle = 0;                       // chinh giao dien -> chuyen ve mat So lon de thay ngay
        wf_save_style();
        if (g_cur == SCR_WATCH) { wfLastMin = -2; draw_wf_full(); }
    }

    // Cap nhat mat dong ho (ve lai khi doi phut hoac doi trang thai)
    if (g_cur == SCR_WATCH) {
        uint32_t ep = clock_now_epoch();
        int curMin = ep ? (int)((ep + 7 * 3600) / 60) : -1;
        if (curMin != wfLastMin) { wfLastMin = curMin; draw_wf_time(); }
        int st = (g_sys.bleConnected ? 1000 : 0) + g_sys.battPercent
                 + (g_weather.has ? 5000 + g_weather.temp * 10 + g_weather.text[0] : 0);
        if (st != wfLastStat) { wfLastStat = st; draw_wf_status(); }
    }

    update_nav();
    route_refresh();
    update_notify();
    update_music();
    update_remote();
    update_timer();
    // Vua nhan xong 1 anh QR -> neu dang xem man QR thi ve lai
    if (g_qrImgUpdated) {
        g_qrImgUpdated = false;
        if (g_cur == SCR_QR) qr_draw();
    }
}
