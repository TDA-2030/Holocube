

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "driver/i2s.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"
#include <math.h>
#include "esp_dsp.h"

static const char *TAG = "spectrum";

static uint64_t g_period = 0;
extern esp_lcd_panel_handle_t panel_handle;

#define AUDIO_RATE 30000
#define N_SAMPLES 1024
#define CONFIG_DSP_MAX_FFT_SIZE 4096

static const uint8_t bmp_240x43[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x07, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x03, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x0F, 0xC0, 0x3F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x1F, 0xF8, 0x0F, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x06, 0x3F, 0xFE, 0x07, 0xF0, 0x00, 0x00, 0xFF, 0xFC, 0x03,
    0xFF, 0xF0, 0x3F, 0xFF, 0xC0, 0x3F, 0xFF, 0x80, 0x0F, 0xFF,
    0xC0, 0x3F, 0xFF, 0x00, 0xFF, 0xFE, 0x07, 0x80, 0xFF, 0xFC,
    0x0C, 0x3F, 0xFF, 0x83, 0xF8, 0x00, 0x03, 0xFF, 0xFC, 0x0F,
    0xFF, 0xF0, 0x3F, 0xFF, 0xE0, 0x3F, 0xFF, 0xE0, 0x3F, 0xFF,
    0xC0, 0xFF, 0xFF, 0x01, 0xFF, 0xFE, 0x07, 0x81, 0xFF, 0xFC,
    0x08, 0x3F, 0xFF, 0xE0, 0xF8, 0x00, 0x07, 0xFF, 0xFC, 0x1F,
    0xFF, 0xF0, 0x3F, 0xFF, 0xF0, 0x3F, 0xFF, 0xE0, 0x3F, 0xFF,
    0xC0, 0xFF, 0xFF, 0x03, 0xFF, 0xFE, 0x07, 0x83, 0xFF, 0xFC,
    0x18, 0x00, 0xFF, 0xF0, 0x7C, 0x00, 0x07, 0xC0, 0x00, 0x1F,
    0x00, 0x00, 0x3C, 0x00, 0xF8, 0x38, 0x01, 0xE0, 0x7C, 0x00,
    0x01, 0xF0, 0x00, 0x03, 0xC0, 0x00, 0x07, 0x83, 0xC0, 0x00,
    0x10, 0x00, 0x1F, 0xF8, 0x3E, 0x00, 0x07, 0x80, 0x00, 0x1E,
    0x00, 0x00, 0x3C, 0x00, 0x78, 0x38, 0x00, 0xF0, 0x78, 0x00,
    0x01, 0xE0, 0x00, 0x03, 0xC0, 0x00, 0x07, 0x87, 0x80, 0x00,
    0x30, 0x00, 0x07, 0xFC, 0x3E, 0x00, 0x07, 0x00, 0x00, 0x1C,
    0x00, 0x00, 0x3C, 0x00, 0x78, 0x38, 0x00, 0xF0, 0x78, 0x00,
    0x01, 0xE0, 0x00, 0x03, 0x80, 0x00, 0x07, 0x87, 0x80, 0x00,
    0x20, 0xFF, 0x01, 0xFE, 0x1F, 0x00, 0x07, 0x00, 0x00, 0x1C,
    0x00, 0x00, 0x3C, 0x00, 0x78, 0x38, 0x00, 0xF0, 0x78, 0x00,
    0x01, 0xE0, 0x00, 0x03, 0x80, 0x00, 0x07, 0x87, 0x80, 0x00,
    0x23, 0xFF, 0xE0, 0xFF, 0x0F, 0x00, 0x07, 0x00, 0x00, 0x1C,
    0x00, 0x00, 0x3C, 0x00, 0x78, 0x38, 0x00, 0xF0, 0x78, 0x00,
    0x01, 0xE0, 0x00, 0x03, 0x80, 0x00, 0x07, 0x87, 0x80, 0x00,
    0x43, 0xFF, 0xF8, 0x3F, 0x87, 0x00, 0x07, 0x00, 0x00, 0x1C,
    0x00, 0x00, 0x3C, 0x00, 0x78, 0x38, 0x00, 0xF0, 0x78, 0x00,
    0x01, 0xE0, 0x00, 0x03, 0x80, 0x00, 0x07, 0x87, 0x80, 0x00,
    0x47, 0xFF, 0xFC, 0x1F, 0xC7, 0x80, 0x07, 0x00, 0x00, 0x1C,
    0x00, 0x00, 0x3C, 0x00, 0x78, 0x38, 0x00, 0xF0, 0x78, 0x00,
    0x01, 0xE0, 0x00, 0x03, 0x80, 0x00, 0x07, 0x87, 0x80, 0x00,
    0x47, 0xFF, 0xFE, 0x0F, 0xE3, 0x80, 0x07, 0x00, 0x00, 0x1C,
    0x00, 0x00, 0x3C, 0x00, 0x78, 0x38, 0x00, 0xF0, 0x78, 0x00,
    0x01, 0xE0, 0x00, 0x03, 0x80, 0x00, 0x07, 0x87, 0x80, 0x00,
    0x4F, 0xC3, 0xFF, 0x0F, 0xE1, 0x80, 0x07, 0x00, 0x00, 0x1E,
    0x00, 0x00, 0x3C, 0x00, 0x78, 0x38, 0x00, 0xF0, 0x78, 0x00,
    0x01, 0xE0, 0x00, 0x03, 0xC0, 0x00, 0x07, 0x87, 0x80, 0x00,
    0x4F, 0x80, 0xFF, 0x87, 0xF1, 0x80, 0x07, 0x00, 0x00, 0x1F,
    0x00, 0x00, 0x3C, 0x00, 0x78, 0x38, 0x00, 0xF0, 0x78, 0x00,
    0x01, 0xF0, 0x00, 0x03, 0xC0, 0x00, 0x07, 0x87, 0x80, 0x00,
    0x4F, 0xC0, 0x3F, 0xC3, 0xF0, 0x80, 0x07, 0xFF, 0xF0, 0x1F,
    0xFF, 0xE0, 0x3C, 0x00, 0x78, 0x38, 0x00, 0xF0, 0x7F, 0xFF,
    0x01, 0xFF, 0xFE, 0x03, 0xFF, 0xF8, 0x07, 0x87, 0xFF, 0xF8,
    0x4F, 0xF8, 0x0F, 0xE1, 0xF8, 0x80, 0x07, 0xFF, 0xF0, 0x0F,
    0xFF, 0xF0, 0x3C, 0x00, 0xF8, 0x3C, 0x07, 0xE0, 0x7F, 0xFF,
    0x00, 0xFF, 0xFF, 0x01, 0xFF, 0xFE, 0x07, 0x87, 0xFF, 0xF8,
    0x47, 0xFF, 0x07, 0xF1, 0xF8, 0x00, 0x07, 0xFF, 0xF0, 0x07,
    0xFF, 0xF8, 0x3F, 0xFF, 0xF0, 0x3F, 0xFF, 0xE0, 0x7F, 0xFF,
    0x00, 0x7F, 0xFF, 0x80, 0xFF, 0xFF, 0x07, 0x87, 0xFF, 0xF8,
    0x47, 0xFF, 0x83, 0xF0, 0xFC, 0x00, 0x07, 0x00, 0x00, 0x01,
    0xFF, 0xF8, 0x3F, 0xFF, 0xF0, 0x3F, 0xFF, 0x80, 0x78, 0x00,
    0x00, 0x1F, 0xFF, 0x80, 0x3F, 0xFF, 0x07, 0x87, 0x80, 0x00,
    0x43, 0xFF, 0xE3, 0xF8, 0xFC, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x00, 0x78, 0x3F, 0xFF, 0xC0, 0x3F, 0xFF, 0x80, 0x78, 0x00,
    0x00, 0x00, 0x07, 0x80, 0x00, 0x0F, 0x07, 0x87, 0x80, 0x00,
    0x40, 0xFF, 0xE1, 0xF8, 0xFC, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x00, 0x78, 0x3C, 0x00, 0x00, 0x38, 0x03, 0xC0, 0x78, 0x00,
    0x00, 0x00, 0x07, 0x80, 0x00, 0x0F, 0x07, 0x87, 0x80, 0x00,
    0x20, 0x1F, 0xF0, 0xFC, 0x7C, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x00, 0x78, 0x3C, 0x00, 0x00, 0x38, 0x01, 0xE0, 0x78, 0x00,
    0x00, 0x00, 0x03, 0x80, 0x00, 0x0F, 0x07, 0x87, 0x80, 0x00,
    0x20, 0x03, 0xF8, 0xFC, 0x7E, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x00, 0x78, 0x3C, 0x00, 0x00, 0x38, 0x01, 0xE0, 0x78, 0x00,
    0x00, 0x00, 0x03, 0x80, 0x00, 0x0F, 0x07, 0x87, 0x80, 0x00,
    0x20, 0x01, 0xF8, 0xFC, 0x7E, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x00, 0x78, 0x3C, 0x00, 0x00, 0x38, 0x01, 0xE0, 0x78, 0x00,
    0x00, 0x00, 0x03, 0x80, 0x00, 0x0F, 0x07, 0x87, 0x80, 0x00,
    0x10, 0x00, 0xFC, 0x7C, 0x3E, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x00, 0x78, 0x3C, 0x00, 0x00, 0x38, 0x00, 0xF0, 0x78, 0x00,
    0x00, 0x00, 0x03, 0x80, 0x00, 0x0F, 0x07, 0x87, 0x80, 0x00,
    0x10, 0x78, 0xFC, 0x7E, 0x3C, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x00, 0x78, 0x3C, 0x00, 0x00, 0x38, 0x00, 0xF0, 0x78, 0x00,
    0x00, 0x00, 0x03, 0x80, 0x00, 0x0F, 0x07, 0x87, 0x80, 0x00,
    0x08, 0x7C, 0x7C, 0x7E, 0x3C, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x00, 0x78, 0x3C, 0x00, 0x00, 0x38, 0x00, 0xF0, 0x78, 0x00,
    0x00, 0x00, 0x03, 0x80, 0x00, 0x0F, 0x07, 0x87, 0x80, 0x00,
    0x0C, 0x7C, 0x7C, 0x7E, 0x38, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x00, 0x78, 0x3C, 0x00, 0x00, 0x38, 0x00, 0xF0, 0x78, 0x00,
    0x00, 0x00, 0x07, 0x80, 0x00, 0x0F, 0x07, 0x87, 0x80, 0x00,
    0x04, 0x78, 0x7C, 0x7E, 0x00, 0x00, 0x07, 0x80, 0x00, 0x00,
    0x00, 0x78, 0x3C, 0x00, 0x00, 0x38, 0x00, 0x78, 0x78, 0x00,
    0x00, 0x00, 0x07, 0x80, 0x00, 0x0F, 0x07, 0x87, 0x80, 0x00,
    0x02, 0x30, 0x7C, 0x7E, 0x00, 0x00, 0x07, 0xFF, 0xFC, 0x1F,
    0xFF, 0xF0, 0x3C, 0x00, 0x00, 0x38, 0x00, 0x78, 0x7F, 0xFF,
    0xC1, 0xFF, 0xFF, 0x83, 0xFF, 0xFE, 0x07, 0x87, 0x80, 0x00,
    0x01, 0x00, 0x7C, 0x7E, 0x02, 0x00, 0x03, 0xFF, 0xFC, 0x1F,
    0xFF, 0xF0, 0x3C, 0x00, 0x00, 0x38, 0x00, 0x78, 0x3F, 0xFF,
    0xC1, 0xFF, 0xFF, 0x03, 0xFF, 0xFE, 0x07, 0x87, 0x80, 0x00,
    0x00, 0x80, 0xFC, 0x7E, 0x0C, 0x00, 0x01, 0xFF, 0xFC, 0x1F,
    0xFF, 0xE0, 0x3C, 0x00, 0x00, 0x38, 0x00, 0x78, 0x1F, 0xFF,
    0xC1, 0xFF, 0xFE, 0x03, 0xFF, 0xFC, 0x07, 0x87, 0x80, 0x00,
    0x00, 0x60, 0x7C, 0x7C, 0x18, 0x00, 0x00, 0x00, 0x04, 0x00,
    0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x30, 0x18, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x0C, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x03, 0x80, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x7F, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /*"espressif-logo.bmp",0*/
};


/* Color definitions, RGB565 format */
#define COLOR_BLACK       0x0000
#define COLOR_NAVY        0x000F
#define COLOR_DARKGREEN   0x03E0
#define COLOR_DARKCYAN    0x03EF
#define COLOR_MAROON      0x7800
#define COLOR_PURPLE      0x780F
#define COLOR_OLIVE       0x7BE0
#define COLOR_LIGHTGREY   0xC618
#define COLOR_DARKGREY    0x7BEF
#define COLOR_BLUE        0x001F
#define COLOR_GREEN       0x07E0
#define COLOR_CYAN        0x07FF
#define COLOR_RED         0xF800
#define COLOR_MAGENTA     0xF81F
#define COLOR_YELLOW      0xFFE0
#define COLOR_WHITE       0xFFFF
#define COLOR_ORANGE      0xFD20
#define COLOR_GREENYELLOW 0xAFE5
#define COLOR_PINK        0xF81F
#define COLOR_SILVER      0xC618
#define COLOR_GRAY        0x8410
#define COLOR_LIME        0x07E0
#define COLOR_TEAL        0x0410
#define COLOR_FUCHSIA     0xF81F
#define COLOR_ESP_BKGD    0xD185

static void color_hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    h = (uint32_t)((uint32_t)h * 255) / 360;
    s = (uint16_t)((uint16_t)s * 255) / 100;
    v = (uint16_t)((uint16_t)v * 255) / 100;

    uint8_t rr, gg, bb;

    uint8_t region, remainder, p, q, t;

    if (s == 0) {
        rr = v;
        gg = v;
        bb = v;
        *r = *g = *b = v;
        return ;
    }

    region = h / 43;
    remainder = (h - (region * 43)) * 6;

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
    case 0:
        rr = v;
        gg = t;
        bb = p;
        break;
    case 1:
        rr = q;
        gg = v;
        bb = p;
        break;
    case 2:
        rr = p;
        gg = v;
        bb = t;
        break;
    case 3:
        rr = p;
        gg = q;
        bb = v;
        break;
    case 4:
        rr = t;
        gg = p;
        bb = v;
        break;
    default:
        rr = v;
        gg = p;
        bb = q;
        break;
    }

    *r = rr;
    *g = gg;
    *b = bb;
}

static uint16_t rgb888_to_565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static void screen_clear(int color)
{
    int w = 320;
    int h = 240;
    uint16_t *buffer = malloc(w * sizeof(uint16_t));
    if (NULL == buffer) {
        return;
    } else {
        for (size_t i = 0; i < w; i++) {
            buffer[i] = color;
        }

        for (int y = 0; y < h; y++) {
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y, w, y + 1, buffer);
        }

        free(buffer);
    }
}

static void _lcd_init()
{
    screen_clear(rgb888_to_565(0, 0, 0));
    float df = AUDIO_RATE / N_SAMPLES;
    uint16_t p[20];
    memset(p, 255, 40);
    for (size_t i = 0; i <= AUDIO_RATE / 2; i += 1000) {
        int xx = (float)i / df;
        xx = xx * 320 / (N_SAMPLES / 2);
        esp_lcd_panel_draw_bitmap(panel_handle, xx, 120, xx + 1, 130, p);
    }
}

static void disp_fft(uint16_t *fb, uint16_t xx, int16_t yy, uint16_t w, uint16_t h, float *power_db, size_t data_len)
{
    static uint8_t floater[320] = {0};
    static uint8_t floater_delay[320] = {0};

    uint8_t r, g, b;
    for (size_t x = 0; x < w; x++) {
        int index = x * data_len / w;
        float v = power_db[index] + 60;
        if (v < 0) {
            v = 0;
        }
        v = v * h / 60;
        uint16_t t = v;
        if (t > floater[x]) {
            floater[x] = t;
            floater_delay[x] = 10;
        }
        for (size_t y = 0; y < h; y++) {
            int i = (h - 1 - y) * w + x;

            if (t >= y) {
                uint8_t value = 100;
                if (y < 43 && x < 240) {
                    uint8_t v = bmp_240x43[(43 - 1 - y) * 240 / 8 + x / 8];
                    uint8_t s = x % 8;
                    value = (v & (0x80 >> s)) ? 70 : value;
                }
                color_hsv_to_rgb((t - y) * 2, 100, value, &r, &g, &b);
                uint16_t c = rgb888_to_565(r, g, b);
                fb[i] = c >> 8 | c << 8;
            } else {
                fb[i] = COLOR_BLACK;
            }

            if (floater[x] == y) {
                uint16_t c = rgb888_to_565(0, 255, 0);
                fb[i] = c >> 8 | c << 8;
            }
        }
        if (0 == floater_delay[x]) {
            if (floater[x] > t) {
                floater[x]--;
            }
        } else {
            floater_delay[x]--;
        }


    }

    esp_lcd_panel_draw_bitmap(panel_handle, xx, yy, xx + w, yy + h, fb);
}

// indata: L, R, L, R, ......
static void _fft_data_input_chunk(void *indata, size_t data_size, float wind[], float y_cf[], size_t *outlen)
{
    data_size /= 4;
    int16_t *ap = (int16_t *)indata;

    for (int i = 0; i < data_size; i++) {
        y_cf[(i) * 2 + 0] = (float)ap[(i) * 2 + 0] / 32768.0;
        // y_cf[(i) * 2 + 1] = (float)ap[(i) * 2 + 1] / 32768.0;
    }

    if (wind) {
        // apply window
        for (int i = 0; i < N_SAMPLES; i++) {
            y_cf[i * 2 + 0] *= wind[i];
            y_cf[i * 2 + 1] = 0; //= wind[i];
        }
    } else {
        for (int i = 0; i < N_SAMPLES; i++) {
            y_cf[i * 2 + 0] *= 1.0;
            y_cf[i * 2 + 1] = 0; //= wind[i];
        }
    }

    // FFT
    dsps_fft2r_fc32(y_cf, N_SAMPLES);
    // Bit reverse
    dsps_bit_rev_fc32(y_cf, N_SAMPLES);
    // Convert one complex vector to two complex vectors
    dsps_cplx2reC_fc32(y_cf, N_SAMPLES);
    // Pointers to result arrays
    float *y1_cf = &y_cf[0];
    float *y2_cf = &y_cf[N_SAMPLES];

    for (int i = 0; i < N_SAMPLES / 2; i++) {
        float a = y1_cf[i * 2 + 0];
        float b = y1_cf[i * 2 + 1];
        y1_cf[i] = 10.0 * log10f((a * a + b * b) / N_SAMPLES);
        // y2_cf[i] = 10 * log10f((y2_cf[i * 2 + 0] * y2_cf[i * 2 + 0] + y2_cf[i * 2 + 1] * y2_cf[i * 2 + 1])/N_SAMPLES);
    }
    if (outlen) {
        *outlen = N_SAMPLES / 2;
    }

    // Show power spectrum in 64x10 window from -100 to 0 dB from 0..N_SAMPLES/4 samples
    // ESP_LOGW(TAG, "Signal x1");
    // dsps_view(y1_cf, N_SAMPLES/2, 64, 10,  -60, 40, '*');
}


static void fft_task(void *args)
{
    esp_err_t ret = ESP_OK;
    uint8_t *abuffer = NULL;
    uint16_t *disp_buf = NULL;
    float *y_cf = NULL;
    float *wind = NULL;
    // Window coefficients
    wind = heap_caps_aligned_alloc(16, sizeof(float) * N_SAMPLES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_GOTO_ON_FALSE(NULL != wind, ESP_ERR_NO_MEM, exit, TAG, "no mem for window");
    // working complex array
    y_cf = heap_caps_aligned_alloc(16, sizeof(float) * N_SAMPLES * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_GOTO_ON_FALSE(NULL != y_cf, ESP_ERR_NO_MEM, exit, TAG, "no mem for result");

    disp_buf = heap_caps_calloc(1, sizeof(uint16_t) * 240 * 240, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_GOTO_ON_FALSE(NULL != disp_buf, ESP_ERR_NO_MEM, exit, TAG, "no mem for disp_buf");

    size_t chunk_size = sizeof(int16_t) * 2 * N_SAMPLES;
    abuffer = heap_caps_malloc(chunk_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_GOTO_ON_FALSE(NULL != abuffer, ESP_ERR_NO_MEM, exit, TAG, "no mem for chunk buffer");
    size_t bytes_read;

    ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    ESP_GOTO_ON_FALSE(ESP_OK == ret, ESP_FAIL, exit, TAG, "Not possible to initialize FFT. Error = %i", ret);

    // Generate hann window
    dsps_wind_hann_f32(wind, N_SAMPLES);

    i2s_set_sample_rates(I2S_NUM_0, AUDIO_RATE);
    while (1) {
        static uint64_t last_time = 0;
        uint64_t t = esp_timer_get_time();
        g_period = t - last_time;
        last_time = t;

        i2s_read(I2S_NUM_0, abuffer, chunk_size, &bytes_read, portMAX_DELAY);
        int16_t *pcm = (int16_t *)abuffer;printf("[%d, %d, %d, %d, %d, %d]\n", pcm[0],pcm[1],pcm[2],pcm[3],pcm[4],pcm[5]);
        size_t fft_len = 0;
        _fft_data_input_chunk(pcm, bytes_read, wind, y_cf, &fft_len);
        disp_fft(disp_buf, 0, 0, 240, 120, y_cf, fft_len);
    }

exit:
    if (wind) {
        heap_caps_free(wind);
    }
    if (y_cf) {
        heap_caps_free(y_cf);
    }
    if (disp_buf) {
        heap_caps_free(disp_buf);
    }
    if (abuffer) {
        heap_caps_free(abuffer);
    }
    vTaskDelete(NULL);
}

void ap_start(void)
{
    _lcd_init();
    xTaskCreate(fft_task, "fft_task", 4096, NULL, configMAX_PRIORITIES - 3, NULL);

    while (1) {
        ESP_LOGI(TAG, "fps=%.2f", 1000.0f / (float)(g_period / 1000));
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
