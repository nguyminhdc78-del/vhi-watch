#include <Arduino.h>
#include <lvgl.h>
#include <time.h>
#include <math.h>
#include "config.h"
#include "app_state.h"
#include "pet-tricks.h"
#include "bao_img.h"
#include "eat_img.h"
#include "heart_img.h"

LV_FONT_DECLARE(vn_font_16);
LV_FONT_DECLARE(vn_font_20);

// gio dia phuong (GMT+7) tu dong ho dong bo; false neu chua co gio
static bool pet_localtime(struct tm &out) {
    uint32_t ep = clock_now_epoch();
    if (!ep) return false;
    time_t t = (time_t)(ep + 7 * 3600);
    gmtime_r(&t, &out);
    return true;
}

// Vector pet state (port RoboEyes) - dung o key_handler + ui_fast_tick
static float    pex = 0, pexN = 0, pey = 0, peyN = 0;   // wander offset (current, target)
static float    peh = 68, pehN = 68;                    // eye height (blink)
static uint32_t petTWander = 0, petTBlink = 0, petTBlinkOpen = 0;
static uint32_t petHappyUntil = 0, petLastFrame = 0;
// Bo "dao dien" tu doi tro: 0 idle 1 happy 2 laugh 3 confused 4 wink 5 angry 6 look
//                           7 love(trai tim) 8 dizzy(chong mat) 9 read(doc bao) 10 pho(an pho)
static int      petAct = 0, petWinkEye = 0;
static uint32_t petActUntil = 0, petRestUntil = 0;
static uint8_t  petBag[16];               // "tui" cac tro se dien (xao tron, luot het moi lap)
static int      petBagN = 0, petBagPos = 0;
// Menu chon animation (nut A) + khoa 1 tro chon tay
static bool     petMenuOpen = false, petLock = false;
static int      petMenuSel = 0;
// Choc gheo: bam nut A (LV_KEY_DOWN) don dap -> buc doc len -> gian
static int      petPokeCnt = 0;
static uint32_t petLastPoke = 0;
static uint32_t petPokeLockUntil = 0;   // khoa nut A khi dang gian -> animation gian chay tron
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
  { "Trái tim",     7,   pet_love,      2600,  9,  1 },
  { "Chóng mặt",    8,   pet_dizzy,     2000,  8,  0 },
  { "Giận dữ",      5,   nullptr,       1800,  5,  0 },
  { "Nháy mắt",     4,   nullptr,        260,  8,  0 },
  { "Ngáp",         11,  pet_yawn,      3200,  9,  1 },
  { "Lấp lánh",     12,  pet_sparkle,   2600, 11,  0 },
  { "Ngơ ngác",     13,  pet_confused,  2600, 11,  0 },
  { nullptr,        1,   nullptr,       2200, 16,  1 },   // cuoi mim (chi random; diu -> dem OK)
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
static void pet_render(int mood, int hlL, int hlR, int dx, int dy, lv_color_t col, bool glint = false) {
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

    // Catchlight: dom bong goc tren mat -> mat long lanh, co hon (khong phai con nguoi)
    if (glint && hlL > 28 && hlR > 28) {
        lv_color_t wht = lv_color_hex(0xFFFFFF);
        int gr = EYE_W / 8, gx = EYE_W / 3;
        pet_circle(xL + gx, yTL + hlL / 4, gr, wht);
        pet_circle(xR + gx, yTR + hlR / 4, gr, wht);
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

void build_pet(lv_obj_t *scr) {
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
    petAct = 0; petActUntil = 0; petRestUntil = 0; petBagN = petBagPos = 0;
    petMenuOpen = false; petLock = false; petMenuSel = 0;
    petPokeCnt = 0; petLastPoke = 0; petPokeLockUntil = 0;
    pet_render(0, EYE_H, EYE_H, 0, 0, lv_color_hex(0x33E1FF));
}

// Do lai "tui" cac tro se dien: xao tron -> luot HET bo moi lap lai (co trat tu, khong nhay coc trung).
// Ban dem chi lay tro co co 'dem' (diu: ngap / trai tim / cuoi mim).
static void pet_refill_bag(bool night) {
    petBagN = 0;
    for (int i = 0; i < PET_TRICK_CNT; i++) {
        if (PET_TRICKS[i].weight == 0) continue;         // tro chi-chon-tay (khong tu dien)
        if (night && !PET_TRICKS[i].night) continue;     // ban dem: bo tro "loud"
        if (petBagN < (int)(sizeof(petBag) / sizeof(petBag[0]))) petBag[petBagN++] = (uint8_t)i;
    }
    for (int i = petBagN - 1; i > 0; i--) {              // Fisher-Yates xao tron
        int j = (int)random(0, i + 1);
        uint8_t t = petBag[i]; petBag[i] = petBag[j]; petBag[j] = t;
    }
    petBagPos = 0;
}

// Goi moi vong loop() (~40ms) khi dang o man Pet -> hoat hinh Vector
void pet_tick(uint32_t now) {
    if (!petCanvas) return;
    if (now - petLastFrame < 40) return;    // ~25fps cho easing muot
    petLastFrame = now;

    // ---- Dang mo menu chon animation -> chi ve menu ----
    if (petMenuOpen) { pet_menu_draw(); return; }

    if (g_notify.hasNew && !petLock) { g_notify.hasNew = false; petAct = 7; petActUntil = now + 2800; }  // tin nhan -> mat trai tim

    // ---- Dao dien: nghi idle lau -> lam 1 tro tu "tui xao tron" (co trat tu, khong lap lien) ----
    if (!petLock) {
        struct tm tmv;
        bool night = pet_localtime(tmv) && (tmv.tm_hour >= 22 || tmv.tm_hour < 6);
        if (petAct != 0 && now >= petActUntil) {              // tro chay XONG -> ve idle + hen nghi
            petAct = 0;
            petRestUntil = now + (night ? 10000 + (uint32_t)random(0, 8000)    // dem: nghi 10-18s
                                        :  4500 + (uint32_t)random(0, 4000));  // ngay: nghi 4.5-8.5s
        }
        else if (petAct == 0 && now >= petRestUntil) {        // het nghi -> lam tro ke tiep trong tui
            if (petBagPos >= petBagN) pet_refill_bag(night);  // het tui -> xao lai (ngay/dem khac bo)
            if (petBagN > 0) {
                const PetTrick *pick = &PET_TRICKS[petBag[petBagPos++]];
                petAct = pick->act;
                petActUntil = now + pick->dur;
                if (petAct == 4) petWinkEye = (int)random(0, 2);              // nhay mat: chon mat
                if (petAct == 6) { pexN = (random(0,2) ? 40 : -40); peyN = 0; } // to mo: nhin 1 ben
            }
        }
    }

    // ---- Tro co ham ve rieng (love/dizzy/read/pho/yawn/sparkle/confused) -> ve va thoat som ----
    const PetTrick *cur = pet_trick_of(petAct);
    if (cur && cur->render) { cur->render(now); return; }

    // ---- Wander + chop mat chi khi idle ----
    if (petAct == 0 || petAct == 1 || petAct == 5) {
        if (now >= petTWander) {
            pexN = (float)random(-52, 53);        // ngo quanh RONG hon
            peyN = (float)random(-30, 16);        // hoi thien nhin LEN (bot ve buon)
            petTWander = now + 600 + (uint32_t)random(0, 1600);   // ngo thuong xuyen hon
        }
    }
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
    bool glint = false;
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
                 if (pet_localtime(tm) && (tm.tm_hour >= 22 || tm.tm_hour < 6)) {
                     mood = 2; col = lv_color_hex(0x2A7AA8);
                 } else {
                     glint = true;                              // ngay: idle mat to, long lanh, ngo quanh
                 }
                 break; }
    }
    if (hlL < 4) hlL = 4;  if (hlR < 4) hlR = 4;
    pet_render(mood, hlL, hlR, dx, dy, col, glint);
}

// Xu ly 1 phim khi o man Pet -> tra ve PET_KEY_TO_MENU neu can thoat ve menu chinh
PetKeyResult pet_key(uint32_t key) {
    if (petMenuOpen) {                                          // dang mo menu chon animation
        if (key == LV_KEY_DOWN)       petMenuSel = (petMenuSel + 1) % pet_menu_n();
        else if (key == LV_KEY_ENTER) {
            int act = pet_menu_act(petMenuSel);
            if (act < 0) { petLock = false; petAct = 0; petRestUntil = millis() + 600; }   // Tu dong
            else         { petLock = true;  petAct = act; petActUntil = millis() + 3600000;
                           if (act == 6 || act == 5 || act == 1) { pexN = 0; peyN = 0; } }
            petMenuOpen = false;
        }
        else if (key == LV_KEY_ESC)   petMenuOpen = false;
    } else {
        if (key == LV_KEY_ENTER)      { petMenuOpen = true; petMenuSel = 0; }              // B: mo menu
        else if (key == LV_KEY_DOWN)  {                                                    // A: choc gheo -> buc -> gian
            uint32_t nowp = millis();
            if (nowp >= petPokeLockUntil) {                    // dang gian -> nuot phim A cho animation chay xong
                petPokeCnt = (nowp - petLastPoke < 1500) ? petPokeCnt + 1 : 1;   // choc don dap trong 1.5s
                petLastPoke = nowp;
                petLock = false;
                if (petPokeCnt >= 4) {                          // GIAN -> khoa nut A het thoi luong animation
                    petAct = 5; petActUntil = nowp + 2400; petPokeCnt = 0; petPokeLockUntil = nowp + 2400;
                }
                else if (petPokeCnt == 3) { petAct = 3;  petActUntil = nowp + 1300; }             // lac dau kho chiu
                else                      { petAct = 13; petActUntil = nowp + 1100; }             // giat minh "?"
            }
        }
        else if (key == LV_KEY_ESC)   return PET_KEY_TO_MENU;                              // C: quay lai menu
    }
    return PET_KEY_NONE;
}

// Giai phong bo dem canvas Pet (~79KB) khi roi man Pet
void pet_teardown() {
    if (petCanvasBuf) { free(petCanvasBuf); petCanvasBuf = nullptr; }
    petCanvas = nullptr;
}

