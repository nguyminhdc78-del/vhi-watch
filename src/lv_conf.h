/**
 * lv_conf.h - cau hinh toi gian cho LVGL 8.3 tren ESP32-C3
 * Cac option khong khai bao o day se lay gia tri mac dinh trong lv_conf_internal.h
 */
#if 1 /* bat file cau hinh nay */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*==================== MAU SAC ====================*/
#define LV_COLOR_DEPTH 16
/* LovyanGFX pushImage dung mau RGB565 native -> de 0 */
#define LV_COLOR_16_SWAP 0

/*==================== BO NHO ====================*/
/* C3 RAM nho -> cap 48KB cho LVGL */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (48U * 1024U)

/*==================== HAL / TICK ====================*/
/* Tu cap tick bang millis() trong main loop (xem main.cpp) */
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_DPI_DEF 130

/*==================== TINH NANG ====================*/
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR  0
#define LV_USE_LOG 0

/*==================== FONT ====================*/
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_48 1   /* dong ho so to */
#define LV_FONT_DEFAULT &lv_font_montserrat_16

/*==================== WIDGET ====================*/
/* Mac dinh LVGL bat het cac widget co ban; giu nguyen. */

/*==================== ANH / DECODER ====================*/
/* Khong dung decoder PNG/JPG tren MCU (tiet kiem RAM).
   Wallpaper da convert RGB565 san tu dien thoai. */
#define LV_USE_GIF 0
#define LV_USE_PNG 0
#define LV_USE_SJPG 0

/*==================== QR CODE: dung ANH upload (khong tao tu link) ====================*/
#define LV_USE_QRCODE 0

#endif /* LV_CONF_H */
#endif /* bat file */
