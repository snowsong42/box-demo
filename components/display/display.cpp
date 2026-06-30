/*
 * SPDX-FileCopyrightText: 2025 box-demo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "display.h"
#include "sd_card.h"
#include "network.h"
#include <lgfx/v1/lgfx_fonts.hpp>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "display";

// ==================== 全局实例 ====================
static LGFX s_tft;
static LGFX_Sprite s_backbuffer(&s_tft);  // 320×240 PSRAM
static bool s_cjk_font_loaded = false;     // 中文字体是否已加载

// ==================== 中文字体（用于 WiFi SSID 等） ====================

static void try_load_cjk_font(void)
{
    if (s_cjk_font_loaded) return;
    // 尝试从 SD 卡加载中文字体（需要预先用 LovyanGFX 工具生成）
    if (sd_card_mounted()) {
        FILE *f = fopen("/sdcard/font.bin", "rb");
        if (f) {
            fclose(f);
            s_tft.loadFont("/sdcard/font.bin");
            s_backbuffer.loadFont("/sdcard/font.bin");
            s_cjk_font_loaded = true;
            ESP_LOGI(TAG, "CJK font loaded from /sdcard/font.bin");
        }
    }
    if (!s_cjk_font_loaded) {
        ESP_LOGW(TAG, "No /sdcard/font.bin found, CJK chars will show as garbled");
    }
}

// ==================== GIF 帧存储 ====================
static LGFX_Sprite s_gif_frames[GIF_FRAME_COUNT];
static bool s_gif_loaded = false;

// ==================== 内存监控 overlay ====================

static void draw_mem_overlay(void) {
    s_backbuffer.fillRect(198, 0, 122, 14, COLOR_BLACK);
    char buf[32];
    snprintf(buf, sizeof(buf), "H:%zuK P:%zuK",
             (size_t)(esp_get_free_heap_size() / 1024),
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);
    s_backbuffer.setTextColor(0x39C7);
    s_backbuffer.setTextSize(1);
    s_backbuffer.setTextDatum(textdatum_t::top_right);
    s_backbuffer.drawString(buf, 318, 2);
}

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

// ==================== 主菜单（7 项滚动） ====================

static const char *s_menu_items[] = {
    "1. IMG Test",
    "2. Marquee",
    "3. GIF Test",
    "4. Record&Play",
    "5. Ping Test",
    "6. HTTP GET",
    "7. TCP Client",
    "8. WIFI Status",
    "9. WiFi Config",
    "10. ASR Voice",
    "11. SD Card",
    "12. AI Chat",
};
#define MENU_COUNT  12
#define MENU_VISIBLE 5
#define MENU_TBL_X  18
#define MENU_TBL_Y  50
#define MENU_TBL_W  284
#define MENU_ROW_H  34
#define MENU_HINT_Y 228

void draw_menu(int selection, int scroll_offset)
{
    s_backbuffer.fillScreen(COLOR_BLACK);

    // 标题
    s_backbuffer.setTextColor(COLOR_WHITE);
    s_backbuffer.setTextSize(4);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    s_backbuffer.drawString("box-demo", 160, 22);

    // 表格外框（固定高度 = 可见行数 * 行高）
    int tbl_h = MENU_VISIBLE * MENU_ROW_H;
    s_backbuffer.drawRect(MENU_TBL_X, MENU_TBL_Y, MENU_TBL_W, tbl_h, COLOR_WHITE);
    s_backbuffer.drawRect(MENU_TBL_X + 1, MENU_TBL_Y + 1, MENU_TBL_W - 2, tbl_h - 2, COLOR_GRAY);

    for (int i = 0; i < MENU_VISIBLE; i++) {
        int item_idx = scroll_offset + i;
        if (item_idx >= MENU_COUNT) break;

        int ry = MENU_TBL_Y + i * MENU_ROW_H;

        // 行分隔线
        if (i > 0)
            s_backbuffer.fillRect(MENU_TBL_X + 1, ry, MENU_TBL_W - 2, 1, COLOR_GRAY);

        // 高亮/普通行背景
        uint16_t row_bg = (item_idx == selection) ? 0x18E3 : 0x0000;
        s_backbuffer.fillRect(MENU_TBL_X + 2, ry + 1, MENU_TBL_W - 4, MENU_ROW_H - 2, row_bg);

        // 行号 + 文本
        s_backbuffer.setTextDatum(textdatum_t::middle_left);
        if (item_idx == selection) {
            s_backbuffer.setTextColor(COLOR_CYAN);
            s_backbuffer.setTextSize(1);
            s_backbuffer.drawString(">", MENU_TBL_X + 12, ry + MENU_ROW_H / 2);
            s_backbuffer.setTextSize(2);
            s_backbuffer.setTextColor(COLOR_WHITE);
        } else {
            s_backbuffer.setTextColor(COLOR_GRAY);
            s_backbuffer.setTextSize(2);
        }
        s_backbuffer.drawString(s_menu_items[item_idx], MENU_TBL_X + 32, ry + MENU_ROW_H / 2);

        // 滚动指示器
        if (scroll_offset > 0 && i == 0) {
            // 顶部有隐藏项 → 显示 ▲
            s_backbuffer.setTextColor(0x632C);
            s_backbuffer.setTextSize(1);
            s_backbuffer.setTextDatum(textdatum_t::top_right);
            s_backbuffer.drawString("▲", MENU_TBL_X + MENU_TBL_W - 8, ry + 2);
        }
        if (scroll_offset + MENU_VISIBLE < MENU_COUNT && i == MENU_VISIBLE - 1) {
            // 底部有隐藏项 → 显示 ▼
            s_backbuffer.setTextColor(0x632C);
            s_backbuffer.setTextSize(1);
            s_backbuffer.setTextDatum(textdatum_t::bottom_right);
            s_backbuffer.drawString("▼", MENU_TBL_X + MENU_TBL_W - 8, ry + MENU_ROW_H - 2);
        }
    }

    // 底部提示
    s_backbuffer.setTextColor(0x632C);
    s_backbuffer.setTextSize(1);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    s_backbuffer.drawString("[UP][DOWN] Select    [START] Enter", 160, MENU_HINT_Y);

    // 推屏
    draw_mem_overlay();
    s_tft.startWrite();
    s_backbuffer.pushSprite(&s_tft, 0, 0);
    s_tft.endWrite();
}

// ==================== 图片浏览器 ====================

#define IMG_AREA_Y  14
#define IMG_AREA_H  200
#define IMG_HINT_Y  227

void draw_img_browser(int img_index, int img_count,
                      int *w_cache, int *h_cache)
{
    int idx = img_index + 1;
    char path[32];
    snprintf(path, sizeof(path), "/sdcard/img/%04d.png", idx);
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
    s_backbuffer.drawString("[UP][DOWN] Prev/Next  [BACK] Reset", 160, IMG_HINT_Y);

    draw_mem_overlay();
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

void draw_marquee_frame(uint16_t *raw_data, int scroll_offset)
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
    s_backbuffer.drawString("[UP][DOWN] Pause  [BACK] Reset", 160, MARQUEE_HINT_Y);

    draw_mem_overlay();
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
        snprintf(path, sizeof(path), "/sdcard/gif/frame_%02d.png", i);
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

void draw_gif_frame(int frame_idx, int gif_speed)
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
    s_backbuffer.drawString("[UP][DOWN] Speed  [START] Audio  [BACK] Reset", 160, GIF_HINT_Y);

    draw_mem_overlay();
    s_tft.startWrite();
    s_backbuffer.pushSprite(&s_tft, 0, 0);
    s_tft.endWrite();
}

// ==================== Tab 导航栏 ====================

#define TAB_COUNT   4
#define TAB_H       22
#define TAB_SEP_X   80

static const char *tab_labels[TAB_COUNT] = {
    "IMG", "MARQUEE", "GIF", "NET"
};

void draw_tab_bar(int active_tab)
{
    int tab_w = 320 / TAB_COUNT;

    for (int i = 0; i < TAB_COUNT; i++) {
        int tx = i * tab_w;
        uint16_t bg = (i == active_tab) ? 0x18E3 : 0x1082;
        uint16_t fg = (i == active_tab) ? COLOR_WHITE : COLOR_GRAY;

        s_backbuffer.fillRect(tx, 0, tab_w, TAB_H, bg);
        if (i > 0) {
            s_backbuffer.fillRect(tx, 2, 1, TAB_H - 4, 0x39C7);
        }

        // 激活 Tab 下方高亮条
        if (i == active_tab) {
            s_backbuffer.fillRect(tx + 3, TAB_H - 3, tab_w - 6, 3, COLOR_CYAN);
        }

        s_backbuffer.setTextColor(fg);
        s_backbuffer.setTextSize(1);
        s_backbuffer.setTextDatum(textdatum_t::middle_center);
        s_backbuffer.drawString(tab_labels[i], tx + tab_w / 2, TAB_H / 2);
    }

    // Tab 下方分隔线
    s_backbuffer.fillRect(0, TAB_H, 320, 1, COLOR_WHITE);
}

// ==================== 网络测试子菜单 ====================

#define NET_MENU_Y  (TAB_H + 4)
#define NET_ROW_H   34
#define NET_MENU_W  280
#define NET_MENU_X  ((320 - NET_MENU_W) / 2)

static const char *net_sub_items[] = {
    "Ping Test",
    "HTTP GET",
    "TCP Client",
    "WiFi Status"
};
#define NET_SUB_COUNT 4

void draw_network_menu(int sub_selection, bool wifi_connected,
                       const char *wifi_ssid, bool prov_mode)
{
    s_backbuffer.fillScreen(COLOR_BLACK);
    int y0 = 4;

    // WiFi 状态指示条
    s_backbuffer.fillRect(0, y0, 320, 16, 0x1082);
    s_backbuffer.setTextSize(1);
    if (!wifi_connected) {
        s_backbuffer.setTextColor(COLOR_YELLOW);
        if (prov_mode) {
            s_backbuffer.drawString("Provisioning: connect to box-demo-config",
                                    6, y0 + 2);
        } else {
            s_backbuffer.drawString("WiFi not connected. Press START to config.",
                                    6, y0 + 2);
        }
    } else {
        s_backbuffer.setTextColor(COLOR_GREEN);
        char buf[64];
        snprintf(buf, sizeof(buf), "WiFi: %s", wifi_ssid ? wifi_ssid : "Connected");
        s_backbuffer.drawString(buf, 6, y0 + 2);
    }
    y0 += 18;

    // 子菜单项
    for (int i = 0; i < NET_SUB_COUNT; i++) {
        int ry = y0 + i * NET_ROW_H;
        uint16_t row_bg = (i == sub_selection) ? 0x18E3 : 0x0000;
        s_backbuffer.fillRect(NET_MENU_X, ry, NET_MENU_W, NET_ROW_H - 1, row_bg);
        s_backbuffer.drawRect(NET_MENU_X, ry, NET_MENU_W, NET_ROW_H - 1, 0x39C7);

        s_backbuffer.setTextDatum(textdatum_t::middle_center);
        if (i == sub_selection) {
            s_backbuffer.setTextColor(COLOR_CYAN);
            s_backbuffer.setTextSize(1);
            s_backbuffer.drawString(">", NET_MENU_X + 16, ry + NET_ROW_H / 2);
            s_backbuffer.setTextSize(2);
            s_backbuffer.setTextColor(COLOR_WHITE);
        } else {
            s_backbuffer.setTextColor(COLOR_GRAY);
            s_backbuffer.setTextSize(2);
        }
        s_backbuffer.drawString(net_sub_items[i], 160, ry + NET_ROW_H / 2);
    }

    // 底部提示
    y0 += NET_SUB_COUNT * NET_ROW_H + 4;
    s_backbuffer.setTextColor(0x632C);
    s_backbuffer.setTextSize(1);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    if (prov_mode) {
        s_backbuffer.drawString("[UP][DOWN] Select    [START] Config    [BACK] Cancel", 160, y0 + 2);
    } else {
        s_backbuffer.drawString("[UP][DOWN] Select    [START] Enter", 160, y0 + 2);
    }

    s_tft.startWrite();
    s_backbuffer.pushSprite(&s_tft, 0, 0);
    s_tft.endWrite();
}

// ==================== 网络测试结果页 ====================

#define RESULT_TITLE_Y  (TAB_H + 4)
#define RESULT_BODY_Y   (TAB_H + 22)
#define RESULT_LINE_H   18
#define RESULT_HINT_Y   228

void draw_network_result(const char *title, const char *body,
                         int scroll_offset, int max_lines, bool running)
{
    s_backbuffer.fillScreen(COLOR_BLACK);

    // 标题
    s_backbuffer.fillRect(0, RESULT_TITLE_Y, 320, 16, 0x1082);
    s_backbuffer.setTextColor(COLOR_CYAN);
    s_backbuffer.setTextSize(1);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    char title_buf[64];
    snprintf(title_buf, sizeof(title_buf), "%s %s", title, running ? "..." : "[Done]");
    s_backbuffer.drawString(title_buf, 160, RESULT_TITLE_Y + 8);

    // 结果内容（滚动文本 — 自动换行）
    if (body && body[0]) {
        const int MAX_CHARS = 30;  // 每行最多字符数（1.5 号字体约 30 字/行）

        // 先将所有源行展开为显示行（含换行处理）
        char *display_lines[120];
        int total_lines = 0;
        const char *p = body;

        while (*p && total_lines < 120) {
            // 找到下一段（到 \n 或 \0）
            const char *seg_end = p;
            while (*seg_end && *seg_end != '\n') seg_end++;
            int seg_len = seg_end - p;

            // 将这一段按 MAX_CHARS 分成多行
            int pos = 0;
            while (pos < seg_len && total_lines < 120) {
                int chunk = seg_len - pos;
                if (chunk > MAX_CHARS) chunk = MAX_CHARS;
                char *line = (char *)malloc(chunk + 1);
                memcpy(line, p + pos, chunk);
                line[chunk] = '\0';
                display_lines[total_lines++] = line;
                pos += chunk;
            }
            p = seg_end;
            if (*p == '\n') p++;
        }

        if (scroll_offset > total_lines - max_lines)
            scroll_offset = (total_lines > max_lines) ? total_lines - max_lines : 0;
        if (scroll_offset < 0) scroll_offset = 0;

        s_backbuffer.setTextColor(COLOR_WHITE);
        s_backbuffer.setTextSize(1.5f);
        s_backbuffer.setTextDatum(textdatum_t::top_left);

        for (int i = 0; i < max_lines && (i + scroll_offset) < total_lines; i++) {
            s_backbuffer.drawString(display_lines[i + scroll_offset],
                                    4, RESULT_BODY_Y + i * RESULT_LINE_H);
        }

        // 释放临时行
        for (int i = 0; i < total_lines; i++) free(display_lines[i]);
    }

    // 底部提示
    s_backbuffer.setTextColor(0x632C);
    s_backbuffer.setTextSize(1);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    if (running) {
        s_backbuffer.drawString("[BACK] Stop & Return", 160, RESULT_HINT_Y);
    } else {
        s_backbuffer.drawString("[UP][DOWN] Scroll    [START] Rerun    [BACK] Return", 160, RESULT_HINT_Y);
    }

    s_tft.startWrite();
    s_backbuffer.pushSprite(&s_tft, 0, 0);
    s_tft.endWrite();
}

// ==================== WiFi 未连接提示页 ====================

void draw_wifi_not_connected(void)
{
    s_backbuffer.fillScreen(COLOR_BLACK);

    s_backbuffer.setTextColor(COLOR_YELLOW);
    s_backbuffer.setTextSize(2);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    s_backbuffer.drawString("WiFi Not Connected", 160, 60);

    s_backbuffer.setTextColor(COLOR_WHITE);
    s_backbuffer.setTextSize(2);
    s_backbuffer.drawString("Press", 160, 110);
    s_backbuffer.setTextColor(COLOR_CYAN);
    s_backbuffer.drawString("[START]", 160, 140);
    s_backbuffer.setTextColor(COLOR_WHITE);
    s_backbuffer.drawString("to configure WiFi", 160, 170);

    s_backbuffer.setTextColor(0x632C);
    s_backbuffer.setTextSize(1);
    s_backbuffer.drawString("[BACK] Return to menu", 160, 225);

    s_tft.startWrite();
    s_backbuffer.pushSprite(&s_tft, 0, 0);
    s_tft.endWrite();
}

// ==================== 启动画面 ====================

void draw_boot_screen(void)
{
    s_backbuffer.fillScreen(COLOR_BLACK);

    s_backbuffer.setTextColor(COLOR_WHITE);
    s_backbuffer.setTextSize(4);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    s_backbuffer.drawString("box-demo", 160, 80);

    s_backbuffer.setTextColor(0x632C);
    s_backbuffer.setTextSize(2);
    s_backbuffer.drawString("Starting...", 160, 140);

    s_tft.startWrite();
    s_backbuffer.pushSprite(&s_tft, 0, 0);
    s_tft.endWrite();
}

// ==================== WiFi 状态大字体面板 ====================

void draw_wifi_status_big(const char *ssid, const char *ip, int rssi, const char *mac, bool confirm)
{
    s_backbuffer.fillScreen(COLOR_BLACK);

    try_load_cjk_font();

    s_backbuffer.setTextColor(COLOR_CYAN);
    s_backbuffer.setTextSize(2);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    s_backbuffer.drawString("WiFi Status", 160, 16);

    s_backbuffer.fillRect(20, 34, 280, 2, COLOR_WHITE);

    char buf[48];
    int y = 50;

    s_backbuffer.setTextColor(COLOR_GRAY);
    s_backbuffer.setTextSize(1);
    s_backbuffer.setTextDatum(textdatum_t::top_left);
    s_backbuffer.drawString("SSID:", 24, y);
    s_backbuffer.setTextColor(COLOR_WHITE);
    if (s_cjk_font_loaded) s_backbuffer.setTextSize(1);
    else s_backbuffer.setTextSize(2);
    snprintf(buf, sizeof(buf), "%s", ssid[0] ? ssid : "--");
    s_backbuffer.drawString(buf, 24, y + 12);
    s_backbuffer.setTextSize(2);  // 恢复

    y += 48;
    s_backbuffer.setTextColor(COLOR_GRAY);
    s_backbuffer.setTextSize(1);
    s_backbuffer.drawString("IP Address:", 24, y);
    s_backbuffer.setTextColor(COLOR_GREEN);
    s_backbuffer.setTextSize(2);
    snprintf(buf, sizeof(buf), "%s", ip[0] ? ip : "0.0.0.0");
    s_backbuffer.drawString(buf, 24, y + 12);

    y += 48;
    s_backbuffer.setTextColor(COLOR_GRAY);
    s_backbuffer.setTextSize(1);
    s_backbuffer.drawString("Signal:", 24, y);
    uint16_t rssi_color = (rssi > -60) ? COLOR_GREEN : (rssi > -75) ? COLOR_YELLOW : COLOR_RED;
    s_backbuffer.setTextColor(rssi_color);
    s_backbuffer.setTextSize(2);
    snprintf(buf, sizeof(buf), "%d dBm", rssi);
    s_backbuffer.drawString(buf, 24, y + 12);

    y += 48;
    s_backbuffer.setTextColor(COLOR_GRAY);
    s_backbuffer.setTextSize(1);
    s_backbuffer.drawString("MAC:", 24, y);
    s_backbuffer.setTextColor(COLOR_WHITE);
    s_backbuffer.setTextSize(2);
    s_backbuffer.drawString(mac, 24, y + 12);

    // 服务器地址（小字灰色）
    y += 48;
    s_backbuffer.setTextColor(COLOR_GRAY);
    s_backbuffer.setTextSize(1);
    s_backbuffer.drawString("ASR:", 24, y);
    const char *asr = wifi_get_asr_url();
    s_backbuffer.drawString(asr, 60, y);

    s_backbuffer.setTextColor(0x632C);
    s_backbuffer.setTextSize(1);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    if (confirm) {
        s_backbuffer.setTextColor(COLOR_YELLOW);
        s_backbuffer.drawString("Clear WiFi config?", 160, 200);
        s_backbuffer.setTextColor(0x632C);
        s_backbuffer.drawString("[START] Confirm  [BACK] Cancel", 160, 228);
    } else {
        s_backbuffer.drawString("[START] Clear WiFi  [BACK] Return", 160, 228);
    }

    s_tft.startWrite();
    s_backbuffer.pushSprite(&s_tft, 0, 0);
    s_tft.endWrite();
}

// ==================== WiFi 配置引导页 ====================

void draw_wifi_config_page(int phase, bool connected, const char *ip,
                           bool check_done, bool baidu_ok,
                           bool asr_ok, bool chat_ok)
{
    s_backbuffer.fillScreen(COLOR_BLACK);

    s_backbuffer.setTextColor(COLOR_CYAN);
    s_backbuffer.setTextSize(2);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    s_backbuffer.drawString("WiFi Config", 160, 20);
    s_backbuffer.fillRect(20, 38, 280, 2, COLOR_WHITE);

    if (phase == 1) {
        s_backbuffer.setTextColor(COLOR_WHITE);
        s_backbuffer.setTextSize(2);
        s_backbuffer.drawString("1. Connect phone to:", 160, 62);
        s_backbuffer.setTextColor(COLOR_YELLOW);
        s_backbuffer.setTextSize(3);
        s_backbuffer.drawString("box-demo", 160, 92);
        s_backbuffer.setTextColor(COLOR_WHITE);
        s_backbuffer.setTextSize(2);
        s_backbuffer.drawString("2. Open browser:", 160, 130);
        s_backbuffer.setTextColor(COLOR_GREEN);
        s_backbuffer.setTextSize(3);
        s_backbuffer.drawString("192.168.4.1", 160, 160);
        s_backbuffer.setTextColor(0x632C);
        s_backbuffer.setTextSize(1);
        s_backbuffer.drawString("[BACK] Cancel", 160, 228);
    } else if (phase == 2) {
        s_backbuffer.setTextColor(COLOR_YELLOW);
        s_backbuffer.setTextSize(2);
        s_backbuffer.drawString("Connecting...", 160, 90);
        s_backbuffer.setTextColor(COLOR_GRAY);
        s_backbuffer.setTextSize(1);
        s_backbuffer.drawString("Please wait", 160, 120);
        s_backbuffer.setTextColor(0x632C);
        s_backbuffer.setTextSize(1);
        s_backbuffer.drawString("[BACK] Return to menu", 160, 228);
    } else if (phase == 3 && connected) {
        char buf[48];
        s_backbuffer.setTextColor(COLOR_GREEN);
        s_backbuffer.setTextSize(3);
        s_backbuffer.drawString("Connected!", 160, 64);
        s_backbuffer.setTextColor(COLOR_WHITE);
        s_backbuffer.setTextSize(2);
        snprintf(buf, sizeof(buf), "IP: %s", ip ? ip : "N/A");
        s_backbuffer.drawString(buf, 160, 98);

        // 连通性检查结果
        s_backbuffer.setTextDatum(textdatum_t::top_left);
        int cy = 128;
        s_backbuffer.setTextSize(1);
        if (!check_done) {
            s_backbuffer.setTextColor(COLOR_GRAY);
            s_backbuffer.drawString("Checking...", 48, cy);
        } else {
            // Internet
            s_backbuffer.setTextColor(baidu_ok ? COLOR_GREEN : COLOR_RED);
            s_backbuffer.drawString(baidu_ok ? "[OK] Internet" : "[X] Internet", 48, cy);
            cy += 18;
            // ASR Server
            s_backbuffer.setTextColor(asr_ok ? COLOR_GREEN : COLOR_RED);
            s_backbuffer.drawString(asr_ok ? "[OK] ASR Server" : "[X] ASR Server", 48, cy);
            cy += 18;
            // Chat Server
            s_backbuffer.setTextColor(chat_ok ? COLOR_GREEN : COLOR_RED);
            s_backbuffer.drawString(chat_ok ? "[OK] Chat Server" : "[X] Chat Server", 48, cy);
        }

        s_backbuffer.setTextDatum(textdatum_t::middle_center);
        s_backbuffer.setTextColor(0x632C);
        s_backbuffer.setTextSize(1);
        s_backbuffer.drawString("[START] Reconfigure  [BACK] Menu", 160, 228);
    } else {
        s_backbuffer.setTextColor(COLOR_WHITE);
        s_backbuffer.setTextSize(2);
        s_backbuffer.drawString("Press [START]", 160, 90);
        s_backbuffer.setTextColor(COLOR_CYAN);
        s_backbuffer.drawString("to begin WiFi setup", 160, 120);
        s_backbuffer.setTextColor(0x632C);
        s_backbuffer.setTextSize(1);
        s_backbuffer.drawString("[BACK] Return to menu", 160, 228);
    }

    s_tft.startWrite();
    s_backbuffer.pushSprite(&s_tft, 0, 0);
    s_tft.endWrite();
}

// ==================== 录音播放主界面 ====================

void draw_record_main(void)
{
    s_backbuffer.fillScreen(COLOR_BLACK);

    s_backbuffer.setTextColor(COLOR_CYAN);
    s_backbuffer.setTextSize(2);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    s_backbuffer.drawString("Record & Play", 160, 20);

    s_backbuffer.fillRect(60, 38, 200, 2, COLOR_WHITE);

    // 录音卡片
    int y = 52;
    s_backbuffer.setTextColor(COLOR_CYAN);
    s_backbuffer.setTextSize(2);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    s_backbuffer.drawString("[LEFT]  Record", 160, y + 16);
    s_backbuffer.setTextColor(COLOR_GRAY);
    s_backbuffer.setTextSize(1);
    s_backbuffer.drawString("Start voice recording", 160, y + 38);

    // 播放卡片
    y = 118;
    s_backbuffer.setTextColor(COLOR_CYAN);
    s_backbuffer.setTextSize(2);
    s_backbuffer.drawString("[RIGHT] Play", 160, y + 16);
    s_backbuffer.setTextColor(COLOR_GRAY);
    s_backbuffer.setTextSize(1);
    s_backbuffer.drawString("Playback saved audio", 160, y + 38);

    s_backbuffer.setTextColor(0x632C);
    s_backbuffer.setTextSize(1);
    s_backbuffer.drawString("[BACK] Return to menu", 160, 228);

    s_tft.startWrite();
    s_backbuffer.pushSprite(&s_tft, 0, 0);
    s_tft.endWrite();
}

// ==================== 录音界面 ====================

void draw_record_capture(bool recording, int time_left, const char *status)
{
    s_backbuffer.fillScreen(COLOR_BLACK);

    if (recording) {
        // === 录音中 ===
        s_backbuffer.setTextColor(COLOR_RED);
        s_backbuffer.setTextSize(2);
        s_backbuffer.setTextDatum(textdatum_t::middle_center);
        s_backbuffer.drawString("[REC]", 160, 18);

        s_backbuffer.fillRect(60, 36, 200, 2, COLOR_WHITE);

        s_backbuffer.setTextColor(COLOR_GRAY);
        s_backbuffer.setTextSize(1);
        s_backbuffer.drawString("Time left", 160, 56);

        s_backbuffer.setTextColor(COLOR_RED);
        s_backbuffer.setTextSize(6);
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", time_left);
        s_backbuffer.drawString(buf, 160, 100);

        s_backbuffer.setTextColor(COLOR_WHITE);
        s_backbuffer.setTextSize(2);
        s_backbuffer.drawString("seconds", 160, 148);

        // 可选状态文本
        if (status) {
            s_backbuffer.setTextColor(COLOR_YELLOW);
            s_backbuffer.setTextSize(1);
            s_backbuffer.drawString(status, 160, 195);
        }

        s_backbuffer.setTextColor(0x632C);
        s_backbuffer.setTextSize(1);
        s_backbuffer.drawString("[BACK] Stop recording", 160, 228);

    } else if (time_left == 0) {
        // === 录音完毕 ===
        s_backbuffer.setTextColor(COLOR_CYAN);
        s_backbuffer.setTextSize(2);
        s_backbuffer.setTextDatum(textdatum_t::middle_center);
        s_backbuffer.drawString("Record", 160, 20);

        s_backbuffer.fillRect(60, 38, 200, 2, COLOR_WHITE);

        s_backbuffer.setTextColor(COLOR_GREEN);
        s_backbuffer.setTextSize(3);
        s_backbuffer.drawString("Done!", 160, 96);

        s_backbuffer.setTextColor(COLOR_GRAY);
        s_backbuffer.setTextSize(1.5f);
        s_backbuffer.drawString("File saved to SPIFFS", 160, 136);

        s_backbuffer.setTextColor(0x632C);
        s_backbuffer.setTextSize(1);
        s_backbuffer.drawString("[START] Record again  [BACK] Return", 160, 228);

    } else {
        // === 空闲 ===
        s_backbuffer.setTextColor(COLOR_CYAN);
        s_backbuffer.setTextSize(2);
        s_backbuffer.setTextDatum(textdatum_t::middle_center);
        s_backbuffer.drawString("Record", 160, 20);

        s_backbuffer.fillRect(60, 38, 200, 2, COLOR_WHITE);

        s_backbuffer.setTextColor(COLOR_WHITE);
        s_backbuffer.setTextSize(2);
        s_backbuffer.drawString("Press [START] to record", 160, 100);

        s_backbuffer.setTextColor(COLOR_GRAY);
        s_backbuffer.setTextSize(1);
        s_backbuffer.drawString("Max 15 seconds", 160, 132);

        s_backbuffer.setTextColor(0x632C);
        s_backbuffer.setTextSize(1);
        s_backbuffer.drawString("[BACK] Return", 160, 228);
    }

    s_tft.startWrite();
    s_backbuffer.pushSprite(&s_tft, 0, 0);
    s_tft.endWrite();
}

// ==================== 播放界面 ====================

void draw_record_playback(bool playing, bool finished)
{
    s_backbuffer.fillScreen(COLOR_BLACK);

    // 读取 WAV 文件时长
    int dur = 0;
    FILE *f = fopen("/sdcard/rec.wav", "rb");
    if (f) {
        fseek(f, 4, SEEK_SET);
        uint32_t fsize = 0; fread(&fsize, 4, 1, f);
        dur = (int)((fsize - 36) / 44100);
        if (dur < 0) dur = 0;
        fclose(f);
    }
    char info[32];
    snprintf(info, sizeof(info), "recording.wav %ds", dur);

    if (playing) {
        s_backbuffer.setTextColor(COLOR_GREEN);
        s_backbuffer.setTextSize(2);
        s_backbuffer.setTextDatum(textdatum_t::middle_center);
        s_backbuffer.drawString("[PLAY]", 160, 18);

        s_backbuffer.fillRect(60, 36, 200, 2, COLOR_WHITE);

        s_backbuffer.setTextColor(COLOR_GRAY);
        s_backbuffer.setTextSize(1.5f);
        s_backbuffer.drawString(info, 160, 72);

        s_backbuffer.setTextColor(COLOR_CYAN);
        s_backbuffer.setTextSize(3);
        s_backbuffer.drawString("~  ~  ~", 160, 124);

        s_backbuffer.setTextColor(COLOR_WHITE);
        s_backbuffer.setTextSize(2);
        s_backbuffer.drawString("Playing...", 160, 168);

        s_backbuffer.setTextColor(0x632C);
        s_backbuffer.setTextSize(1);
        s_backbuffer.drawString("[BACK] Stop playback", 160, 228);

    } else if (finished) {
        s_backbuffer.setTextColor(COLOR_CYAN);
        s_backbuffer.setTextSize(2);
        s_backbuffer.setTextDatum(textdatum_t::middle_center);
        s_backbuffer.drawString("Play", 160, 20);

        s_backbuffer.fillRect(60, 38, 200, 2, COLOR_WHITE);

        s_backbuffer.setTextColor(COLOR_GREEN);
        s_backbuffer.setTextSize(3);
        s_backbuffer.drawString("Done!", 160, 96);

        s_backbuffer.setTextColor(COLOR_GRAY);
        s_backbuffer.setTextSize(1.5f);
        s_backbuffer.drawString(info, 160, 136);

        s_backbuffer.setTextColor(0x632C);
        s_backbuffer.setTextSize(1);
        s_backbuffer.drawString("[START] Play again  [BACK] Return", 160, 228);

    } else {
        s_backbuffer.setTextColor(COLOR_CYAN);
        s_backbuffer.setTextSize(2);
        s_backbuffer.setTextDatum(textdatum_t::middle_center);
        s_backbuffer.drawString("Play", 160, 20);

        s_backbuffer.fillRect(60, 38, 200, 2, COLOR_WHITE);

        s_backbuffer.setTextColor(COLOR_GRAY);
        s_backbuffer.setTextSize(1.5f);
        s_backbuffer.drawString(info, 160, 72);

        s_backbuffer.setTextColor(COLOR_CYAN);
        s_backbuffer.setTextSize(3);
        s_backbuffer.drawString(">", 160, 114);

        s_backbuffer.setTextColor(COLOR_WHITE);
        s_backbuffer.setTextSize(2);
        s_backbuffer.drawString("Press [START] to play", 160, 164);

        s_backbuffer.setTextColor(0x632C);
        s_backbuffer.setTextSize(1);
        s_backbuffer.drawString("[BACK] Return", 160, 228);
    }

    s_tft.startWrite();
    s_backbuffer.pushSprite(&s_tft, 0, 0);
    s_tft.endWrite();
}

// ==================== SD 卡状态页面 ====================

void draw_sd_card_status(bool mounted, const char *name, const char *fs_type,
                         int total_mb, int free_mb,
                         int write_kbps, int read_kbps, bool testing)
{
    s_backbuffer.fillScreen(COLOR_BLACK);

    s_backbuffer.setTextColor(COLOR_CYAN);
    s_backbuffer.setTextSize(2);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    s_backbuffer.drawString("SD Card Status", 160, 16);
    s_backbuffer.fillRect(20, 34, 280, 2, COLOR_WHITE);

    char buf[48];
    int y = 46;
    s_backbuffer.setTextDatum(textdatum_t::top_left);

    if (!mounted) {
        s_backbuffer.setTextColor(COLOR_RED);
        s_backbuffer.setTextSize(3);
        s_backbuffer.setTextDatum(textdatum_t::middle_center);
        s_backbuffer.drawString("No SD Card", 160, 100);
        s_backbuffer.setTextSize(1);
        s_backbuffer.setTextColor(COLOR_GRAY);
        s_backbuffer.drawString("Insert FAT32 SD card & reboot", 160, 140);
    } else {
        s_backbuffer.setTextColor(COLOR_GRAY);
        s_backbuffer.setTextSize(2);
        s_backbuffer.drawString("Status:", 24, y);
        s_backbuffer.setTextColor(COLOR_GREEN);
        s_backbuffer.drawString("Mounted", 120, y);
        y += 28;

        s_backbuffer.setTextColor(COLOR_GRAY);
        s_backbuffer.drawString("Card:", 24, y);
        s_backbuffer.setTextColor(COLOR_WHITE);
        snprintf(buf, sizeof(buf), "%s  [%s]", name, fs_type);
        s_backbuffer.drawString(buf, 120, y);
        y += 28;

        s_backbuffer.setTextColor(COLOR_GRAY);
        s_backbuffer.drawString("Total:", 24, y);
        s_backbuffer.setTextColor(COLOR_WHITE);
        if (total_mb >= 1024) {
            snprintf(buf, sizeof(buf), "%.1f GB", total_mb / 1024.0f);
        } else {
            snprintf(buf, sizeof(buf), "%d MB", total_mb);
        }
        s_backbuffer.drawString(buf, 120, y);
        y += 26;

        s_backbuffer.setTextColor(COLOR_GRAY);
        s_backbuffer.drawString("Free:", 24, y);
        s_backbuffer.setTextColor(COLOR_GREEN);
        if (free_mb >= 1024) {
            snprintf(buf, sizeof(buf), "%.1f GB", free_mb / 1024.0f);
        } else {
            snprintf(buf, sizeof(buf), "%d MB", free_mb);
        }
        s_backbuffer.drawString(buf, 120, y);
        y += 32;

        s_backbuffer.fillRect(20, y, 280, 1, 0x2104);
        y += 8;

        s_backbuffer.setTextColor(COLOR_GRAY);
        s_backbuffer.drawString("Write:", 24, y);
        if (write_kbps >= 0) {
            s_backbuffer.setTextColor(COLOR_YELLOW);
            snprintf(buf, sizeof(buf), "%d KB/s", write_kbps);
        } else if (testing) {
            s_backbuffer.setTextColor(COLOR_CYAN);
            snprintf(buf, sizeof(buf), "Testing...");
        } else {
            s_backbuffer.setTextColor(COLOR_GRAY);
            snprintf(buf, sizeof(buf), "-- (press START)");
        }
        s_backbuffer.drawString(buf, 120, y);
        y += 28;

        s_backbuffer.setTextColor(COLOR_GRAY);
        s_backbuffer.drawString("Read:", 24, y);
        if (read_kbps >= 0) {
            s_backbuffer.setTextColor(COLOR_YELLOW);
            snprintf(buf, sizeof(buf), "%d KB/s", read_kbps);
        } else if (testing) {
            s_backbuffer.setTextColor(COLOR_CYAN);
            snprintf(buf, sizeof(buf), "Testing...");
        } else {
            s_backbuffer.setTextColor(COLOR_GRAY);
            snprintf(buf, sizeof(buf), "-- (press START)");
        }
        s_backbuffer.drawString(buf, 120, y);
    }

    s_backbuffer.setTextColor(0x632C);
    s_backbuffer.setTextSize(1);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    s_backbuffer.drawString("[START] Speed Test  [BACK] Return", 160, 220);

    s_tft.startWrite();
    s_backbuffer.pushSprite(&s_tft, 0, 0);
    s_tft.endWrite();
}

// ==================== SD 卡文件浏览器 ====================

void draw_sd_card_browse(const char *path, void *ventries,
                         int count, int selection, int scroll)
{
    sd_card_entry_t *entries = (sd_card_entry_t *)ventries;
    s_backbuffer.fillScreen(COLOR_BLACK);

    s_backbuffer.setTextColor(COLOR_CYAN);
    s_backbuffer.setTextSize(2);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    s_backbuffer.drawString("SD Card Browse", 160, 16);
    s_backbuffer.fillRect(20, 34, 280, 2, COLOR_WHITE);

    // 当前路径
    s_backbuffer.setTextColor(0x632C);
    s_backbuffer.setTextSize(1);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    const char *show = path;
    if (strlen(path) > 28) show = path + strlen(path) - 28;
    s_backbuffer.drawString(show, 160, 42);

    const int ROW_H = 24, MAX_VIS = 6, TX = 20;
    int y = 50;

    if (count <= 0) {
        s_backbuffer.setTextColor(COLOR_GRAY);
        s_backbuffer.setTextSize(2);
        s_backbuffer.drawString("(empty)", 160, 110);
    } else {
        if (scroll > count - MAX_VIS) scroll = count - MAX_VIS;
        if (scroll < 0) scroll = 0;

        s_backbuffer.setTextDatum(textdatum_t::top_left);
        for (int i = 0; i < MAX_VIS && (i + scroll) < count; i++) {
            int idx = i + scroll;
            int ry = y + i * ROW_H;
            bool sel = (idx == selection);

            if (sel) s_backbuffer.fillRect(TX - 2, ry, 278, ROW_H, 0x18E3);

            s_backbuffer.setTextSize(2);
            if (entries[idx].is_dir) {
                s_backbuffer.setTextColor(COLOR_YELLOW);
                s_backbuffer.drawString("[D]", TX, ry + 2);
                s_backbuffer.setTextColor(sel ? COLOR_WHITE : COLOR_CYAN);
                s_backbuffer.drawString(entries[idx].name, TX + 48, ry + 2);
            } else {
                char buf[32];
                if (entries[idx].size >= 1024 * 1024)
                    snprintf(buf, sizeof(buf), "%.1fM", entries[idx].size / (1024.0f * 1024.0f));
                else if (entries[idx].size >= 1024)
                    snprintf(buf, sizeof(buf), "%uK", (unsigned)(entries[idx].size / 1024));
                else
                    snprintf(buf, sizeof(buf), "%uB", (unsigned)entries[idx].size);
                s_backbuffer.setTextColor(COLOR_GRAY);
                s_backbuffer.drawString(buf, TX, ry + 2);
                s_backbuffer.setTextColor(sel ? COLOR_WHITE : 0xC618);
                s_backbuffer.drawString(entries[idx].name, TX + 72, ry + 2);
            }
        }
    }

    s_backbuffer.setTextColor(0x632C);
    s_backbuffer.setTextSize(1);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    s_backbuffer.drawString("[L] Up  [R] Enter  [U/D] Move  [BACK] Status", 160, 220);

    s_tft.startWrite();
    s_backbuffer.pushSprite(&s_tft, 0, 0);
    s_tft.endWrite();
}

// ==================== AI Chat 对话页面 ====================

void draw_chat(const char *text, int cursor_byte, int *scroll_line, const char *status)
{
    s_backbuffer.fillScreen(COLOR_BLACK);

    s_backbuffer.setTextColor(COLOR_CYAN);
    s_backbuffer.setTextSize(2);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    s_backbuffer.drawString("AI Chat", 160, 16);
    s_backbuffer.fillRect(20, 32, 280, 2, COLOR_WHITE);

    int y = 38;
    if (status) {
        s_backbuffer.setTextColor(COLOR_GRAY);
        s_backbuffer.setTextSize(1);
        s_backbuffer.drawString(status, 160, y + 6);
        s_backbuffer.fillRect(20, y + 16, 280, 1, 0x2104);
        y += 20;
    }

    const int LINE_H = 20, UNITS_PER_LINE = 36, MAX_VIS = 6, TX = 16, FW = 8;
    const char *lines[80];
    int total = 0;
    const char *p = text;
    while (*p && total < 80) {
        lines[total++] = p;
        int units = 0;
        while (*p && *p != '\n') {
            int cl = 1, cw = FW;
            if ((*p & 0x80) != 0) {
                cl = 1; while ((p[cl] & 0xC0) == 0x80) cl++;
                cw = FW * 2;
            }
            if (units + cw > UNITS_PER_LINE * FW) break;
            p += cl; units += cw;
        }
        if (*p == '\n') p++;
    }

    // 计算光标所在行
    int cl = 0, cpx = 0, bc = 0; p = text;
    while (*p && bc < cursor_byte) {
        int clen = 1, cw = FW;
        if ((*p & 0x80) != 0) {
            clen = 1; while ((p[clen] & 0xC0) == 0x80) clen++;
            cw = FW * 2;
        }
        if (cpx + cw > UNITS_PER_LINE * FW || *p == '\n') {
            cpx = 0; cl++;
            if (*p == '\n') { p++; bc++; continue; }
        }
        bc += clen; cpx += cw; p += clen;
    }

    // 自动滚动
    int sl_scroll = *scroll_line;
    if (cl < sl_scroll) sl_scroll = cl;
    if (cl >= sl_scroll + MAX_VIS) sl_scroll = cl - MAX_VIS + 1;
    if (sl_scroll > total - MAX_VIS) sl_scroll = total - MAX_VIS;
    if (sl_scroll < 0) sl_scroll = 0;
    *scroll_line = sl_scroll;

    s_backbuffer.setTextSize(1);
    s_backbuffer.setFont(&fonts::efontCN_16);
    uint16_t cur_color = COLOR_WHITE;  // 跟踪当前发言者颜色，续行继承
    for (int i = 0; i < MAX_VIS && (i + sl_scroll) < total; i++) {
        int li = i + sl_scroll;
        const char *s = lines[li], *e;
        if (li + 1 < total) { e = lines[li + 1]; if (*(e - 1) == '\n') e--; }
        else e = s + strlen(s);
        int len = e - s; if (len > 63) len = 63;
        char lb[64] = {0}; memcpy(lb, s, len);
        if (strncmp(lb, "You:", 4) == 0)
            cur_color = COLOR_GREEN;
        else if (strncmp(lb, "AI:", 3) == 0)
            cur_color = COLOR_CYAN;
        s_backbuffer.setTextColor(cur_color);
        s_backbuffer.setTextDatum(textdatum_t::top_left);
        s_backbuffer.drawString(lb, TX, y + i * LINE_H);
    }

    int sl = cl - sl_scroll;
    if (sl >= 0 && sl < MAX_VIS) {
        s_backbuffer.fillRect(TX + cpx, y + sl * LINE_H, 2, LINE_H, COLOR_RED);
    }

    s_backbuffer.setFont(nullptr);
    s_backbuffer.setTextColor(0x632C);
    s_backbuffer.setTextSize(1);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    s_backbuffer.drawString("[START] Rec  [UP] Send  [L/R] Move  [DOWN] Del  [BACK]", 160, 220);

    draw_mem_overlay();
    s_tft.startWrite();
    s_backbuffer.pushSprite(&s_tft, 0, 0);
    s_tft.endWrite();
}

// ==================== ASR 准备中画面 ====================

void draw_asr_preparing(void)
{
    s_backbuffer.fillScreen(COLOR_BLACK);

    s_backbuffer.setTextColor(COLOR_CYAN);
    s_backbuffer.setTextSize(2);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    s_backbuffer.drawString("ASR Voice", 160, 16);
    s_backbuffer.fillRect(20, 32, 280, 2, COLOR_WHITE);

    s_backbuffer.setTextColor(COLOR_YELLOW);
    s_backbuffer.setTextSize(2);
    s_backbuffer.drawString("Preparing...", 160, 100);

    s_backbuffer.setTextColor(COLOR_GRAY);
    s_backbuffer.setTextSize(1);
    s_backbuffer.drawString("Initializing microphone", 160, 136);

    s_backbuffer.setTextColor(0x632C);
    s_backbuffer.setTextSize(1);
    s_backbuffer.drawString("[BACK] Cancel", 160, 228);

    s_tft.startWrite();
    s_backbuffer.pushSprite(&s_tft, 0, 0);
    s_tft.endWrite();
}

// ==================== ASR 文本编辑器 ====================

void draw_asr_text(const char *text, int cursor_byte, int *scroll_line, const char *status)
{
    s_backbuffer.fillScreen(COLOR_BLACK);

    s_backbuffer.setTextColor(COLOR_CYAN);
    s_backbuffer.setTextSize(2);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    s_backbuffer.drawString("ASR Voice", 160, 16);
    s_backbuffer.fillRect(20, 32, 280, 2, COLOR_WHITE);

    int y = 38;
    if (status) {
        s_backbuffer.setTextColor(COLOR_GRAY);
        s_backbuffer.setTextSize(1);
        s_backbuffer.drawString(status, 160, y + 6);
        s_backbuffer.fillRect(20, y + 16, 280, 1, 0x2104);
        y += 20;
    }

    const int LINE_H = 20, UNITS_PER_LINE = 36, MAX_VIS = 6, TX = 16, FW = 8;
    const char *lines[80];
    int total = 0;
    const char *p = text;
    while (*p && total < 80) {
        lines[total++] = p;
        int units = 0;
        while (*p && *p != '\n') {
            int cl = 1, cw = FW;  // ASCII: 1 unit (8px)
            if ((*p & 0x80) != 0) {
                cl = 1; while ((p[cl] & 0xC0) == 0x80) cl++;
                cw = FW * 2;       // CJK: 2 units (16px)
            }
            if (units + cw > UNITS_PER_LINE * FW) break;
            p += cl; units += cw;
        }
        if (*p == '\n') p++;
    }

    // 计算光标所在行（像素宽度感知）
    int cl = 0, cpx = 0, bc = 0; p = text;
    while (*p && bc < cursor_byte) {
        int clen = 1, cw = FW;
        if ((*p & 0x80) != 0) {
            clen = 1; while ((p[clen] & 0xC0) == 0x80) clen++;
            cw = FW * 2;
        }
        if (cpx + cw > UNITS_PER_LINE * FW || *p == '\n') {
            cpx = 0; cl++;
            if (*p == '\n') { p++; bc++; continue; }
        }
        bc += clen; cpx += cw; p += clen;
    }

    // 自动滚动：确保光标行在可见范围内
    int sl_scroll = *scroll_line;
    if (cl < sl_scroll) sl_scroll = cl;
    if (cl >= sl_scroll + MAX_VIS) sl_scroll = cl - MAX_VIS + 1;
    if (sl_scroll > total - MAX_VIS) sl_scroll = total - MAX_VIS;
    if (sl_scroll < 0) sl_scroll = 0;
    *scroll_line = sl_scroll;

    s_backbuffer.setTextSize(1);
    s_backbuffer.setFont(&fonts::efontCN_16);
    for (int i = 0; i < MAX_VIS && (i + sl_scroll) < total; i++) {
        int li = i + sl_scroll;
        const char *s = lines[li], *e;
        if (li + 1 < total) { e = lines[li + 1]; if (*(e - 1) == '\n') e--; }
        else e = s + strlen(s);
        int len = e - s; if (len > 63) len = 63;
        char lb[64] = {0}; memcpy(lb, s, len);
        s_backbuffer.setTextColor(COLOR_WHITE);
        s_backbuffer.setTextDatum(textdatum_t::top_left);
        s_backbuffer.drawString(lb, TX, y + i * LINE_H);
    }

    // 绘制光标
    int sl = cl - sl_scroll;
    if (sl >= 0 && sl < MAX_VIS) {
        s_backbuffer.fillRect(TX + cpx, y + sl * LINE_H, 2, LINE_H, COLOR_RED);
    }

    s_backbuffer.setFont(nullptr);
    s_backbuffer.setTextColor(0x632C);
    s_backbuffer.setTextSize(1);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    s_backbuffer.drawString("[START] Rec  [L/R] Move  [DOWN] Del  [BACK]", 160, 228);

    draw_mem_overlay();
    s_tft.startWrite();
    s_backbuffer.pushSprite(&s_tft, 0, 0);
    s_tft.endWrite();
}
