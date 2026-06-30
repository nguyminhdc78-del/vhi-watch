#include "ui.h"
#include "config.h"
#include "app_state.h"
#include "storage.h"
#include "web_upload.h"
#include "ble_nav.h"
#include "hid_remote.h"
#include "display.h"
#include <time.h>

// ============================================================
//  Quan ly man hinh bang 1 state machine don gian.
//  Moi man hinh co 1 container goc (focusable) nhan phim:
//    NEXT/PREV = di chuyen, ENTER = chon, ESC = quay lai
// ============================================================

enum Screen { SCR_WATCH, SCR_MENU, SCR_NAV, SCR_UPLOAD, SCR_NOTIFY, SCR_MUSIC, SCR_REMOTE, SCR_CAMERA, SCR_QR };

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
                                    LV_SYMBOL_SETTINGS " Cai dat" };
static const int   MENU_N = 7;
static int         menuSel = 0;
static lv_obj_t   *menuBtns[MENU_N];

static void show_screen(Screen s);     // forward
static void request_screen(Screen s);  // forward: doi man hinh AN TOAN (hoan ngoai event)
static int  g_pendingScreen = -1;

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

static uint16_t ui_color565() {
    return ((g_uiR >> 3) << 11) | ((g_uiG >> 2) << 5) | (g_uiB >> 3);
}

// Xoa 1 dai ngang: ve lai anh nen neu co, khong thi to den
static void wf_clear_band(int y0, int h) {
    if (!wfHasWp || !display_draw_image_band(WALLPAPER_PATH, y0, h))
        display_fill_rect(0, y0, SCREEN_W, h, 0x0000);
}

static void draw_wf_status() {
    wf_clear_band(0, 22);
    char s[40];
    snprintf(s, sizeof(s), "%s  %d%%",
             g_sys.bleConnected ? "BLE" : "Cho ket noi", g_sys.battPercent);
    display_text_center(SCREEN_W / 2, 4, s, 0x5E8C, 1);
}

static void draw_wf_time() {
    char tb[8] = "--:--", db[24] = "";
    uint32_t ep = clock_now_epoch();
    if (ep) {
        time_t t = (time_t)(ep + 7 * 3600);   // GMT+7
        struct tm tm; gmtime_r(&t, &tm);
        snprintf(tb, sizeof(tb), "%02d:%02d", tm.tm_hour, tm.tm_min);
        static const char *wd[] = {"CN", "T2", "T3", "T4", "T5", "T6", "T7"};
        snprintf(db, sizeof(db), "%s %02d/%02d", wd[tm.tm_wday], tm.tm_mday, tm.tm_mon + 1);
    }
    wf_clear_band(86, 82);   // xoa vung gio (giu anh nen)
    display_text_center(SCREEN_W / 2, 94, tb, ui_color565(), 6);
    display_text_center(SCREEN_W / 2, 144, db, 0xAD55, 2);
}

static void draw_wf_full() {
    wfHasWp = display_draw_image_file(WALLPAPER_PATH);
    if (!wfHasWp) display_fill(0x0000);
    draw_wf_status();
    draw_wf_time();
    if (!wfHasWp)
        display_text_center(SCREEN_W / 2, 224, "Nhan B = Menu", 0x8410, 1);
}

static void build_watchface(lv_obj_t *scr) {
    (void)scr;
    display_set_raw(true);            // ve thang ra man (nhe RAM, ho tro anh nen)
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
        case 6: /* TODO: cai dat */         break;
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
    lv_obj_set_style_text_font(lblNApp, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblNApp, lv_color_hex(0x33CCFF), 0);
    lv_obj_align(lblNApp, LV_ALIGN_TOP_MID, 0, 40);

    lblNTitle = lv_label_create(scr);
    lv_obj_set_style_text_font(lblNTitle, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lblNTitle, lv_color_white(), 0);
    lv_obj_set_width(lblNTitle, 214);
    lv_label_set_long_mode(lblNTitle, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(lblNTitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lblNTitle, LV_ALIGN_TOP_MID, 0, 64);

    lblNText = lv_label_create(scr);
    lv_obj_set_style_text_font(lblNText, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lblNText, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_width(lblNText, 214);
    lv_label_set_long_mode(lblNText, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(lblNText, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lblNText, LV_ALIGN_CENTER, 0, 24);

    lv_obj_t *hint = lv_label_create(scr);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), 0);
    lv_label_set_text(hint, "Nhan C = Quay lai");
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
    lv_obj_set_style_text_font(lblMTitle, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lblMTitle, lv_color_white(), 0);
    lv_obj_set_width(lblMTitle, 214);
    lv_label_set_long_mode(lblMTitle, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(lblMTitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lblMTitle, LV_ALIGN_TOP_MID, 0, 48);

    lblMArtist = lv_label_create(scr);
    lv_obj_set_style_text_font(lblMArtist, &lv_font_montserrat_16, 0);
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
            if (key == LV_KEY_ESC) request_screen(SCR_MENU);
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
    }
}

// ============================================================
//  Chuyen man hinh
// ============================================================
static void show_screen(Screen s) {
    // doc don dep man hinh cu
    lv_obj_t *old = g_scr;
    if (g_cur == SCR_WATCH || g_cur == SCR_QR) display_set_raw(false);  // het man ve-raw
    if (g_cur == SCR_REMOTE || g_cur == SCR_CAMERA) { hid_stop(); ble_init(); } // tat HID, bat lai BLE thuong

    // reset con tro label
    lblTime = lblDate = lblStat = nullptr;
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
    }

    lv_scr_load(g_scr);

    // dua focus container vao group keypad
    lv_group_remove_all_objs(g_group);
    lv_group_add_obj(g_group, focus);
    lv_group_focus_obj(focus);

    if (old) lv_obj_del(old);

    if (s == SCR_NAV)    update_nav();
    if (s == SCR_NOTIFY) update_notify();
    if (s == SCR_MUSIC)  update_music();
}

// ============================================================
//  API
// ============================================================
void ui_init(lv_group_t *group) {
    g_group = group;
    show_screen(SCR_WATCH);
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

    // Co thong bao moi -> tu mo man Thong bao (khi dang o man dong ho)
    if (g_notify.hasNew && g_cur == SCR_WATCH) request_screen(SCR_NOTIFY);

    // Doi mau chu (tu app) -> ve lai gio ngay neu dang o mat dong ho
    if (g_colorChanged) {
        g_colorChanged = false;
        if (g_cur == SCR_WATCH) draw_wf_time();
    }

    // Cap nhat mat dong ho (ve lai khi doi phut hoac doi trang thai)
    if (g_cur == SCR_WATCH) {
        uint32_t ep = clock_now_epoch();
        int curMin = ep ? (int)((ep + 7 * 3600) / 60) : -1;
        if (curMin != wfLastMin) { wfLastMin = curMin; draw_wf_time(); }
        int st = (g_sys.bleConnected ? 1000 : 0) + g_sys.battPercent;
        if (st != wfLastStat) { wfLastStat = st; draw_wf_status(); }
    }

    update_nav();
    route_refresh();
    update_notify();
    update_music();
    update_remote();
    // Vua nhan xong 1 anh QR -> neu dang xem man QR thi ve lai
    if (g_qrImgUpdated) {
        g_qrImgUpdated = false;
        if (g_cur == SCR_QR) qr_draw();
    }
}
