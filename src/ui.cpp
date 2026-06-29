#include "ui.h"
#include "config.h"
#include "app_state.h"
#include "storage.h"
#include "web_upload.h"
#include "ble_nav.h"
#include <time.h>

// ============================================================
//  Quan ly man hinh bang 1 state machine don gian.
//  Moi man hinh co 1 container goc (focusable) nhan phim:
//    NEXT/PREV = di chuyen, ENTER = chon, ESC = quay lai
// ============================================================

enum Screen { SCR_WATCH, SCR_MENU, SCR_NAV, SCR_UPLOAD };

static lv_group_t *g_group   = nullptr;
static lv_obj_t   *g_scr     = nullptr;   // man hinh hien tai
static Screen      g_cur     = SCR_WATCH;

// Con tro toi cac label can cap nhat
static lv_obj_t *lblTime = nullptr, *lblDate = nullptr, *lblStat = nullptr;
static lv_obj_t *navArrow = nullptr, *navDist = nullptr, *navStreet = nullptr, *navEta = nullptr;

// Menu
static const char *MENU_ITEMS[] = { LV_SYMBOL_GPS " Chi duong",
                                    LV_SYMBOL_IMAGE " Doi anh nen",
                                    LV_SYMBOL_SETTINGS " Cai dat",
                                    LV_SYMBOL_BELL " Thong tin" };
static const int   MENU_N = 4;
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
static void build_watchface(lv_obj_t *scr) {
    // Anh nen (neu co)
    lv_img_dsc_t *wp = storage_load_wallpaper();
    if (wp) {
        lv_obj_t *bg = lv_img_create(scr);
        lv_img_set_src(bg, wp);
        lv_obj_center(bg);
    }

    // Gio (font to)
    lblTime = lv_label_create(scr);
    lv_obj_set_style_text_font(lblTime, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lblTime, lv_color_white(), 0);
    lv_label_set_text(lblTime, "--:--");
    lv_obj_align(lblTime, LV_ALIGN_CENTER, 0, -10);

    // Ngay
    lblDate = lv_label_create(scr);
    lv_obj_set_style_text_font(lblDate, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lblDate, lv_color_hex(0xBBBBBB), 0);
    lv_label_set_text(lblDate, "");
    lv_obj_align(lblDate, LV_ALIGN_CENTER, 0, 30);

    // Trang thai (BLE + pin) o tren
    lblStat = lv_label_create(scr);
    lv_obj_set_style_text_font(lblStat, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblStat, lv_color_hex(0x66CC66), 0);
    lv_label_set_text(lblStat, "");
    lv_obj_align(lblStat, LV_ALIGN_TOP_MID, 0, 8);

    // Goi y thao tac
    lv_obj_t *hint = lv_label_create(scr);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), 0);
    lv_label_set_text(hint, "Nhan B = Menu");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);
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
        case 0: request_screen(SCR_NAV);    break;
        case 1: request_screen(SCR_UPLOAD); break;
        case 2: /* TODO: cai dat */         break;
        case 3: /* TODO: thong tin */       break;
    }
}

// ============================================================
//  NAVIGATION
// ============================================================
static void build_nav(lv_obj_t *scr) {
    navArrow = lv_label_create(scr);
    lv_obj_set_style_text_font(navArrow, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(navArrow, lv_color_hex(0x33CCFF), 0);
    lv_label_set_text(navArrow, LV_SYMBOL_GPS);
    lv_obj_align(navArrow, LV_ALIGN_TOP_MID, 0, 30);

    navDist = lv_label_create(scr);
    lv_obj_set_style_text_font(navDist, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(navDist, lv_color_white(), 0);
    lv_label_set_text(navDist, "--");
    lv_obj_align(navDist, LV_ALIGN_CENTER, 0, -5);

    navStreet = lv_label_create(scr);
    lv_obj_set_style_text_font(navStreet, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(navStreet, lv_color_hex(0xCCCCCC), 0);
    lv_label_set_long_mode(navStreet, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(navStreet, 200);
    lv_obj_set_style_text_align(navStreet, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(navStreet, "Cho dien thoai ket noi...");
    lv_obj_align(navStreet, LV_ALIGN_CENTER, 0, 40);

    navEta = lv_label_create(scr);
    lv_obj_set_style_text_font(navEta, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(navEta, lv_color_hex(0x888888), 0);
    lv_label_set_text(navEta, "");
    lv_obj_align(navEta, LV_ALIGN_BOTTOM_MID, 0, -10);
}

static void update_nav() {
    if (g_cur != SCR_NAV) return;
    if (!g_nav.active) {
        lv_label_set_text(navArrow, LV_SYMBOL_GPS);
        lv_label_set_text(navDist, "--");
        lv_label_set_text(navStreet, "Chua co lo trinh");
        lv_label_set_text(navEta, "");
        return;
    }
    lv_label_set_text(navArrow, maneuver_symbol(g_nav.maneuver));

    char b[24];
    if (g_nav.distance_m >= 1000) snprintf(b, sizeof(b), "%.1f km", g_nav.distance_m / 1000.0);
    else                          snprintf(b, sizeof(b), "%lu m", (unsigned long)g_nav.distance_m);
    lv_label_set_text(navDist, b);

    lv_label_set_text(navStreet, g_nav.street[0] ? g_nav.street : "");

    char e[40];
    snprintf(e, sizeof(e), "ETA %s  -  con %.1f km",
             g_nav.eta[0] ? g_nav.eta : "--", g_nav.remain_m / 1000.0);
    lv_label_set_text(navEta, e);
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
    }
}

// ============================================================
//  Chuyen man hinh
// ============================================================
static void show_screen(Screen s) {
    // doc don dep man hinh cu
    lv_obj_t *old = g_scr;
    if (g_cur == SCR_WATCH) storage_free_wallpaper();   // giai phong 115KB

    // reset con tro label
    lblTime = lblDate = lblStat = nullptr;
    navArrow = navDist = navStreet = navEta = nullptr;

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
    }

    lv_scr_load(g_scr);

    // dua focus container vao group keypad
    lv_group_remove_all_objs(g_group);
    lv_group_add_obj(g_group, focus);
    lv_group_focus_obj(focus);

    if (old) lv_obj_del(old);

    if (s == SCR_NAV) update_nav();
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

void ui_tick() {
    // Vua nhan xong anh nen moi qua BLE -> nap lai
    if (g_wpUpdated) { g_wpUpdated = false; ui_reload_wallpaper(); }

    // Cap nhat dong ho
    if (g_cur == SCR_WATCH && lblTime) {
        uint32_t ep = clock_now_epoch();
        if (ep) {
            time_t t = (time_t)ep;
            struct tm tm; gmtime_r(&t, &tm);
            // GMT+7 (Viet Nam) - cong 7 gio
            ep += 7 * 3600; t = (time_t)ep; gmtime_r(&t, &tm);
            char b[8];  snprintf(b, sizeof(b), "%02d:%02d", tm.tm_hour, tm.tm_min);
            lv_label_set_text(lblTime, b);
            static const char *wd[] = {"CN","T2","T3","T4","T5","T6","T7"};
            char d[24]; snprintf(d, sizeof(d), "%s %02d/%02d", wd[tm.tm_wday], tm.tm_mday, tm.tm_mon + 1);
            lv_label_set_text(lblDate, d);
        }
        if (lblStat) {
            char s[40];
            snprintf(s, sizeof(s), "%s  %d%%",
                     g_sys.bleConnected ? LV_SYMBOL_BLUETOOTH " BLE" : "Cho ket noi",
                     g_sys.battPercent);
            lv_label_set_text(lblStat, s);
        }
    }

    update_nav();
}
