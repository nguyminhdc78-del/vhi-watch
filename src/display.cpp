#include "display.h"
#include "config.h"
#include <TFT_eSPI.h>

static TFT_eSPI tft = TFT_eSPI();

// Buffer ve tung phan: 240 x LVGL_BUF_LINES pixel
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[SCREEN_W * LVGL_BUF_LINES];

// PWM backlight (ESP32 LEDC)
static const int BL_CH   = 0;
static const int BL_FREQ = 5000;
static const int BL_RES  = 8;

// LVGL goi ham nay moi khi can day pixel ra man hinh
static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    // tham so cuoi = swap byte; khop voi LV_COLOR_16_SWAP trong lv_conf.h
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(drv);
}

void display_set_backlight(uint8_t level) {
    ledcWrite(BL_CH, level);
}

void display_init() {
    // --- TFT ---
    tft.init();
    tft.setRotation(TFT_ROTATION);
    tft.fillScreen(TFT_BLACK);
    // Mot so panel ST7789 1.54" bi am ban -> bo comment dong duoi neu mau bi nguoc
    // tft.invertDisplay(true);

    // --- Backlight PWM ---
    ledcSetup(BL_CH, BL_FREQ, BL_RES);
    ledcAttachPin(TFT_BL, BL_CH);
    display_set_backlight(BACKLIGHT_DEFAULT);

    // --- LVGL ---
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, SCREEN_W * LVGL_BUF_LINES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = SCREEN_W;
    disp_drv.ver_res  = SCREEN_H;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
}
