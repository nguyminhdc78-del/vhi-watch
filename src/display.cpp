#include "display.h"
#include "config.h"
#include <LittleFS.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// ============================================================
//  Cau hinh man hinh ST7789 240x240 SPI cho ESP32-C3 (LovyanGFX)
//  LovyanGFX xu ly dung SPI2/FSPI cua C3 (TFT_eSPI bi loi tren C3)
// ============================================================
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 _panel;
    lgfx::Bus_SPI      _bus;
    lgfx::Light_PWM    _light;
public:
    LGFX() {
        {   // --- bus SPI ---
            auto cfg = _bus.config();
            cfg.spi_host   = SPI2_HOST;   // FSPI tren C3
            cfg.spi_mode   = 0;
            cfg.freq_write = LCD_SPI_FREQ;
            cfg.freq_read  = 16000000;
            cfg.spi_3wire  = false;
            cfg.use_lock   = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk   = PIN_LCD_SCLK;
            cfg.pin_mosi   = PIN_LCD_MOSI;
            cfg.pin_miso   = -1;
            cfg.pin_dc     = PIN_LCD_DC;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {   // --- panel ST7789 ---
            auto cfg = _panel.config();
            cfg.pin_cs        = PIN_LCD_CS;
            cfg.pin_rst       = PIN_LCD_RST;
            cfg.pin_busy      = -1;
            cfg.panel_width   = SCREEN_W;
            cfg.panel_height  = SCREEN_H;
            cfg.offset_x      = 0;
            cfg.offset_y      = 0;
            cfg.offset_rotation = 0;
            cfg.readable      = false;
            cfg.invert        = LCD_INVERT;
            cfg.rgb_order     = false;   // doi true neu do<->xanh bi nguoc
            cfg.dlen_16bit    = false;
            cfg.bus_shared    = false;
            _panel.config(cfg);
        }
        {   // --- den nen PWM ---
            auto cfg = _light.config();
            cfg.pin_bl      = PIN_LCD_BL;
            cfg.invert      = false;
            cfg.freq        = 5000;
            cfg.pwm_channel = 0;
            _light.config(cfg);
            _panel.setLight(&_light);
        }
        setPanel(&_panel);
    }
};

static LGFX lcd;

// Buffer ve tung phan (partial) de tiet kiem RAM
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[SCREEN_W * LVGL_BUF_LINES];

// Khi ve anh truc tiep (QR), bo qua flush cua LVGL de khong de len
static volatile bool s_rawMode = false;
void display_set_raw(bool on) { s_rawMode = on; }

// LVGL goi de day pixel ra man hinh
static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    if (s_rawMode) { lv_disp_flush_ready(drv); return; }  // dang ve raw -> bo qua
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    lcd.startWrite();
    lcd.pushImage(area->x1, area->y1, w, h, (lgfx::rgb565_t *)&color_p->full);
    lcd.endWrite();

    lv_disp_flush_ready(drv);
}

void display_set_backlight(uint8_t level) {
    lcd.setBrightness(level);
}

void display_sleep() {
    lcd.setBrightness(0);   // tat den nen
    lcd.sleep();            // dua ST7789 vao che do ngu (tiet kiem dong)
}

void display_wake() {
    lcd.wakeup();                          // danh thuc panel ST7789
    display_set_backlight(BACKLIGHT_DEFAULT);
}

// Ve 1 anh RGB565 240x240 tu LittleFS THANG ra man (it RAM: doc tung dong)
bool display_draw_image_file(const char *path) {
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    if (f.size() < (size_t)(SCREEN_W * SCREEN_H * 2)) { f.close(); return false; }
    static uint16_t line[SCREEN_W];
    lcd.startWrite();
    for (int y = 0; y < SCREEN_H; y++) {
        f.read((uint8_t *)line, SCREEN_W * 2);
        lcd.pushImage(0, y, SCREEN_W, 1, (lgfx::rgb565_t *)line);
    }
    lcd.endWrite();
    f.close();
    return true;
}

// Ve lai 1 dai ngang cua anh (tu dong y0, cao h) - dung de xoa chu cu ma giu anh nen
bool display_draw_image_band(const char *path, int y0, int h) {
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    if (f.size() < (size_t)(SCREEN_W * SCREEN_H * 2)) { f.close(); return false; }
    static uint16_t line[SCREEN_W];
    f.seek((size_t)y0 * SCREEN_W * 2);
    lcd.startWrite();
    for (int y = y0; y < y0 + h && y < SCREEN_H; y++) {
        f.read((uint8_t *)line, SCREEN_W * 2);
        lcd.pushImage(0, y, SCREEN_W, 1, (lgfx::rgb565_t *)line);
    }
    lcd.endWrite();
    f.close();
    return true;
}

void display_fill(uint16_t color) { lcd.fillScreen(color); }

void display_text(int x, int y, const char *s, uint16_t color, uint16_t bg) {
    lcd.setTextColor(color, bg);
    lcd.setTextSize(2);
    lcd.drawString(s, x, y);
}

void display_fill_rect(int x, int y, int w, int h, uint16_t color) {
    lcd.fillRect(x, y, w, h, color);
}

// Ve chu can giua theo chieu ngang (nen trong suot)
void display_text_center(int cx, int yTop, const char *s, uint16_t color, int size) {
    lcd.setTextSize(size);
    lcd.setTextColor(color);
    int w = lcd.textWidth(s);
    lcd.drawString(s, cx - w / 2, yTop);
}

void display_init() {
    lcd.init();
    lcd.setRotation(TFT_ROTATION);
    lcd.fillScreen(0x0000);   // den
    display_set_backlight(BACKLIGHT_DEFAULT);

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
