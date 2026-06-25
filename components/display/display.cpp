/*
 * SPDX-FileCopyrightText: 2025 box-demo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "display.h"
#include <stdio.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "display";

// ==================== 全局实例 ====================
static LGFX s_tft;
static LGFX_Sprite s_backbuffer(&s_tft);  // 320×240 PSRAM

// ==================== GIF 帧存储 ====================
static LGFX_Sprite s_gif_frames[GIF_FRAME_COUNT];
static bool s_gif_loaded = false;

// ==================== 初始化 ====================

void display_init(void)
{
    s_tft.init();
    s_tft.setRotation(3);
    s_tft.setBrightness(255);
    ESP_LOGI(TAG, "TFT: %ldx%ld", s_tft.width(), s_tft.height());

    s_backbuffer.setPsram(true);
    s_backbuffer.createSprite(320, 240);
    ESP_LOGI(TAG, "Backbuffer: %dx%d", s_backbuffer.width(), s_backbuffer.height());
}

LGFX *display_tft(void)
{
    return &s_tft;
}

LGFX_Sprite *display_backbuffer(void)
{
    return &s_backbuffer;
}

// ==================== 退出确认弹窗（子页面共用） ====================

void draw_exit_popup(void)
{
    const int PW = 240, PH = 110;
    const int PX = (320 - PW) / 2;
    const int PY = (240 - PH) / 2;

    s_backbuffer.fillRect(PX, PY, PW, PH, 0x2104);
    s_backbuffer.drawRect(PX, PY, PW, PH, COLOR_WHITE);
    s_backbuffer.drawRect(PX + 2, PY + 2, PW - 4, PH - 4, COLOR_WHITE);

    s_backbuffer.setTextColor(COLOR_WHITE);
    s_backbuffer.setTextSize(2);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    s_backbuffer.drawString("Exit to Menu?", 160, PY + 28);

    s_backbuffer.fillRect(PX + 15, PY + 48, PW - 30, 2, COLOR_CYAN);

    s_backbuffer.setTextSize(1.5f);
    s_backbuffer.setTextColor(COLOR_CYAN);
    s_backbuffer.drawString("Yes: [DOWN]", 160, PY + 74);
    s_backbuffer.setTextColor(COLOR_GRAY);
    s_backbuffer.drawString("No:  [UP]", 160, PY + 94);
}

// ==================== 主菜单 ====================

static const int TBL_X = 18, TBL_Y = 55, TBL_W = 284, TBL_H = 140;
static const int ROW_H = 45;
static const int TITLE_Y = 25;
static const int HINT_Y = 228;

static const char *menu_items[] = { "IMG Browser", "IMG Marquee", "GIF Player" };
#define MENU_COUNT 3

void draw_confirm_popup(int selection, const char *item_name)
{
    const int PW = 270, PH = 128;
    const int PX = (320 - PW) / 2;
    const int PY = (240 - PH) / 2;

    s_backbuffer.fillRect(PX, PY, PW, PH, 0x0861);
    s_backbuffer.drawRect(PX, PY, PW, PH, COLOR_WHITE);
    s_backbuffer.drawRect(PX + 2, PY + 2, PW - 4, PH - 4, COLOR_WHITE);

    s_backbuffer.setTextColor(COLOR_WHITE);
    s_backbuffer.setTextSize(2);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    char buf[64];
    snprintf(buf, sizeof(buf), "Selected: %s", item_name);
    s_backbuffer.drawString(buf, 160, PY + 24);

    s_backbuffer.fillRect(PX + 15, PY + 44, PW - 30, 2, COLOR_CYAN);
    s_backbuffer.drawString("Enter?", 160, PY + 68);

    s_backbuffer.setTextSize(1.5f);
    s_backbuffer.setTextColor(COLOR_CYAN);
    s_backbuffer.drawString("Yes: ->", 100, PY + 100);
    s_backbuffer.setTextColor(COLOR_GRAY);
    s_backbuffer.drawString("No: <-", 220, PY + 100);
}

void draw_menu(int selection, bool menu_popup)
{
    s_backbuffer.fillScreen(COLOR_BLACK);

    // 标题
    s_backbuffer.setTextColor(COLOR_WHITE);
    s_backbuffer.setTextSize(4);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    s_backbuffer.drawString("box-demo", 160, TITLE_Y);

    // 表格外框
    s_backbuffer.drawRect(TBL_X, TBL_Y, TBL_W, TBL_H, COLOR_WHITE);
    s_backbuffer.drawRect(TBL_X + 1, TBL_Y + 1, TBL_W - 2, TBL_H - 2, COLOR_GRAY);

    for (int i = 0; i < MENU_COUNT; i++) {
        int ry = TBL_Y + i * ROW_H;

        if (i > 0)
            s_backbuffer.fillRect(TBL_X + 1, ry, TBL_W - 2, 1, COLOR_GRAY);

        uint16_t row_bg = (i == selection) ? 0x18E3 : 0x0000;
        s_backbuffer.fillRect(TBL_X + 2, ry + 1, TBL_W - 4, ROW_H - 2, row_bg);

        s_backbuffer.setTextDatum(textdatum_t::middle_center);
        if (i == selection) {
            s_backbuffer.setTextColor(COLOR_CYAN);
            s_backbuffer.setTextSize(1);
            s_backbuffer.drawString(">", TBL_X + 25, ry + ROW_H / 2);
            s_backbuffer.setTextSize(2);
            s_backbuffer.setTextColor(COLOR_WHITE);
            s_backbuffer.drawString(menu_items[i], 160, ry + ROW_H / 2);
        } else {
            s_backbuffer.setTextColor(COLOR_GRAY);
            s_backbuffer.setTextSize(2);
            s_backbuffer.drawString(menu_items[i], 160, ry + ROW_H / 2);
        }
    }

    // 底部提示
    s_backbuffer.setTextColor(0x632C);
    s_backbuffer.setTextSize(1);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    s_backbuffer.drawString("[UP][DOWN] Select    [RIGHT] Enter", 160, HINT_Y);

    if (menu_popup) draw_confirm_popup(selection, menu_items[selection]);

    // 一次性推屏
    s_tft.startWrite();
    s_backbuffer.pushSprite(&s_tft, 0, 0);
    s_tft.endWrite();
}

// ==================== 图片浏览器 ====================

#define IMG_AREA_Y  14
#define IMG_AREA_H  200
#define IMG_HINT_Y  227

void draw_img_browser(int img_index, int img_count,
                      int *w_cache, int *h_cache, bool exit_popup)
{
    int idx = img_index + 1;
    char path[32];
    snprintf(path, sizeof(path), "/spiffs/%04d.png", idx);
    ESP_LOGI(TAG, "IMG loading: %s", path);

    // 缓存 PNG 尺寸
    if (w_cache[img_index] == 0) {
        // Deferred to main via get_png_size — but we still need fallback display
    }

    int img_w = w_cache[img_index];
    int img_h = h_cache[img_index];

    if (img_w == 0) {
        s_backbuffer.fillScreen(COLOR_BLACK);
        s_backbuffer.setTextColor(COLOR_RED);
        s_backbuffer.setTextSize(1);
        s_backbuffer.setTextDatum(textdatum_t::middle_center);
        s_backbuffer.drawString("Load failed!", 160, 120);
        s_tft.startWrite();
        s_backbuffer.pushSprite(&s_tft, 0, 0);
        s_tft.endWrite();
        return;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        s_backbuffer.fillScreen(COLOR_BLACK);
        s_backbuffer.setTextColor(COLOR_RED);
        s_backbuffer.drawString("File open fail!", 160, 120);
        s_tft.startWrite();
        s_backbuffer.pushSprite(&s_tft, 0, 0);
        s_tft.endWrite();
        return;
    }
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    uint8_t *png_buf = (uint8_t *)heap_caps_malloc(fsize, MALLOC_CAP_SPIRAM);
    if (!png_buf) {
        ESP_LOGE(TAG, "IMG PSRAM alloc(%ld) failed", fsize);
        fclose(fp);
        return;
    }
    fread(png_buf, 1, fsize, fp);
    fclose(fp);

    // 所有内容绘入后缓冲
    s_backbuffer.fillScreen(COLOR_BLACK);

    char buf[64];
    snprintf(buf, sizeof(buf), "Browser[%04d/%04d]", idx, img_count);
    s_backbuffer.setTextColor(COLOR_WHITE);
    s_backbuffer.setTextSize(1);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    s_backbuffer.drawString(buf, 160, 7);

    int sx = (320 - img_w) / 2;
    int sy = IMG_AREA_Y + (IMG_AREA_H - img_h) / 2;
    s_backbuffer.drawPng(png_buf, fsize, sx, sy);
    heap_caps_free(png_buf);

    s_backbuffer.setTextColor(0x632C);
    s_backbuffer.drawString("  [LEFT] Prev  [RIGHT] Next  [DOWN] Back  ", 160, IMG_HINT_Y);

    if (exit_popup) draw_exit_popup();

    s_tft.startWrite();
    s_backbuffer.pushSprite(&s_tft, 0, 0);
    s_tft.endWrite();
}

// ==================== 走马灯 ====================

#define MARQUEE_W    500
#define MARQUEE_H    150
#define MARQUEE_TOP_H 14
#define MARQUEE_IMG_Y 41
#define MARQUEE_HINT_Y 229

void draw_marquee_frame(uint16_t *raw_data, int scroll_offset, bool exit_popup)
{
    s_backbuffer.fillRect(0, 0, 320, MARQUEE_TOP_H, COLOR_BLACK);

    s_backbuffer.setTextColor(COLOR_WHITE);
    s_backbuffer.setTextSize(1);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    s_backbuffer.drawString("Marquee 500x150", 160, 7);

    if (raw_data) {
        s_backbuffer.setSwapBytes(true);
        s_backbuffer.pushImage(-scroll_offset, MARQUEE_IMG_Y, MARQUEE_W, MARQUEE_H, raw_data);
        s_backbuffer.pushImage(MARQUEE_W - scroll_offset, MARQUEE_IMG_Y, MARQUEE_W, MARQUEE_H, raw_data);
        s_backbuffer.setSwapBytes(false);
    }

    s_backbuffer.fillRect(0, 218, 320, 22, COLOR_BLACK);
    s_backbuffer.setTextColor(0x632C);
    s_backbuffer.drawString("[DOWN] Back", 160, MARQUEE_HINT_Y);

    if (exit_popup) draw_exit_popup();

    s_tft.startWrite();
    s_backbuffer.pushSprite(&s_tft, 0, 0);
    s_tft.endWrite();
}

// ==================== GIF 帧管理 ====================

int load_gif_frames(void)
{
    ESP_LOGI(TAG, "Loading %d GIF frames...", GIF_FRAME_COUNT);
    int ok_count = 0;
    for (int i = 0; i < GIF_FRAME_COUNT; i++) {
        s_gif_frames[i].setPsram(true);
        s_gif_frames[i].createSprite(GIF_FRAME_W, GIF_FRAME_H);
        if (s_gif_frames[i].width() == 0) {
            ESP_LOGE(TAG, "GIF frame[%d] createSprite OOM", i);
            continue;
        }
        s_gif_frames[i].fillScreen(COLOR_BLACK);

        char path[32];
        snprintf(path, sizeof(path), "/spiffs/gif_%04d.png", i + 1);
        FILE *fp = fopen(path, "rb");
        if (fp) {
            fseek(fp, 0, SEEK_END);
            long fsize = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            uint8_t *buf = (uint8_t *)malloc(fsize);
            if (buf) {
                fread(buf, 1, fsize, fp);
                bool ok = s_gif_frames[i].drawPng(buf, fsize, 0, 0);
                if (!ok) ESP_LOGE(TAG, "GIF frame[%d] drawPng fail", i);
                else ok_count++;
                free(buf);
            } else {
                ESP_LOGE(TAG, "GIF frame[%d] malloc(%ld) fail", i, fsize);
            }
            fclose(fp);
        } else {
            ESP_LOGE(TAG, "GIF frame[%d] fopen fail", i);
        }
    }
    s_gif_loaded = true;
    ESP_LOGI(TAG, "GIF: %d/%d frames OK", ok_count, GIF_FRAME_COUNT);
    return ok_count;
}

void free_gif_frames(void)
{
    for (int i = 0; i < GIF_FRAME_COUNT; i++) {
        s_gif_frames[i].deleteSprite();
    }
    s_gif_loaded = false;
}

bool gif_frames_loaded(void)
{
    return s_gif_loaded;
}

// ==================== GIF 绘制 ====================

#define GIF_TOP_H  12
#define GIF_FRAME_Y 14
#define GIF_HINT_Y  227

void draw_gif_frame(int frame_idx, int gif_speed, bool exit_popup)
{
    s_backbuffer.fillRect(0, 0, 320, GIF_TOP_H, COLOR_BLACK);

    char buf[64];
    snprintf(buf, sizeof(buf), "GIF[%02d/%02d]  Speed:%d", frame_idx + 1, GIF_FRAME_COUNT, gif_speed);
    s_backbuffer.setTextColor(COLOR_WHITE);
    s_backbuffer.setTextSize(1);
    s_backbuffer.setTextDatum(textdatum_t::top_left);
    s_backbuffer.drawString(buf, 6, 3);

    int filled = (gif_speed * 10) / 20;
    const int bx = 135, by = 5, bw = 8, bh = 4;
    for (int i = 0; i < filled; i++)
        s_backbuffer.fillRect(bx + i * (bw + 1), by, bw, bh, COLOR_GREEN);
    for (int i = filled; i < 10; i++)
        s_backbuffer.fillRect(bx + i * (bw + 1), by, bw, bh, COLOR_GRAY);

    // GIF 帧 → 后缓冲
    int x = (320 - GIF_FRAME_W) / 2;
    s_gif_frames[frame_idx].pushSprite(&s_backbuffer, x, GIF_FRAME_Y);

    s_backbuffer.fillRect(0, 214, 320, 26, COLOR_BLACK);
    s_backbuffer.setTextColor(0x632C);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    s_backbuffer.drawString("  [LEFT] Slower  [RIGHT] Faster  [DOWN] Back  ", 160, GIF_HINT_Y);

    if (exit_popup) draw_exit_popup();

    s_tft.startWrite();
    s_backbuffer.pushSprite(&s_tft, 0, 0);
    s_tft.endWrite();
}
