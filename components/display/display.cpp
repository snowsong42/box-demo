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

// ==================== 主菜单（7 项滚动） ====================

static const char *s_menu_items[] = {
    "1. IMG Test",
    "2. Marquee",
    "3. GIF Test",
    "4. Ping Test",
    "5. HTTP GET",
    "6. TCP Client",
    "7. WIFI Status",
    "8. WiFi Config",
};
#define MENU_COUNT  8
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
    s_backbuffer.drawString("[UP][DOWN] Prev/Next  [START] Audio  [BACK] Reset", 160, IMG_HINT_Y);

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
    s_backbuffer.drawString("[UP][DOWN] Pause  [START] Audio  [BACK] Reset", 160, MARQUEE_HINT_Y);

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
#define RESULT_BODY_Y   (TAB_H + 24)
#define RESULT_LINE_H   16
#define RESULT_HINT_Y   230

void draw_network_result(const char *title, const char *body,
                         int scroll_offset, int max_lines, bool running)
{
    s_backbuffer.fillScreen(COLOR_BLACK);

    // 标题
    s_backbuffer.fillRect(0, RESULT_TITLE_Y, 320, 18, 0x1082);
    s_backbuffer.setTextColor(COLOR_CYAN);
    s_backbuffer.setTextSize(1);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    char title_buf[64];
    snprintf(title_buf, sizeof(title_buf), "%s %s", title, running ? "..." : "[Done]");
    s_backbuffer.drawString(title_buf, 160, RESULT_TITLE_Y + 9);

    // 结果内容（滚动文本）
    if (body) {
        // 分割行并绘制
        const char *lines[80];
        int line_count = 0;
        const char *p = body;
        const char *line_start = p;

        while (*p && line_count < 80) {
            if (*p == '\n') {
                lines[line_count++] = line_start;
                line_start = p + 1;
            }
            p++;
        }
        if (line_start < p && line_count < 80) {
            lines[line_count++] = line_start;
        }

        if (scroll_offset > line_count - max_lines) {
            scroll_offset = (line_count > max_lines) ? line_count - max_lines : 0;
        }
        if (scroll_offset < 0) scroll_offset = 0;

        s_backbuffer.setTextColor(COLOR_WHITE);
        s_backbuffer.setTextSize(1);
        s_backbuffer.setTextDatum(textdatum_t::top_left);

        for (int i = 0; i < max_lines && (i + scroll_offset) < line_count; i++) {
            int li = i + scroll_offset;
            int len = 0;
            const char *lp = lines[li];
            while (lp[len] && lp[len] != '\n') len++;
            if (len > 42) len = 42;

            char line_buf[43];
            memcpy(line_buf, lines[li], len);
            line_buf[len] = '\0';
            s_backbuffer.drawString(line_buf, 4, RESULT_BODY_Y + i * RESULT_LINE_H);
        }
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

void draw_wifi_status_big(const char *ssid, const char *ip, int rssi, const char *mac)
{
    s_backbuffer.fillScreen(COLOR_BLACK);

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
    s_backbuffer.setTextSize(2);
    snprintf(buf, sizeof(buf), "%s", ssid[0] ? ssid : "--");
    s_backbuffer.drawString(buf, 24, y + 12);

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

    s_backbuffer.setTextColor(0x632C);
    s_backbuffer.setTextSize(1);
    s_backbuffer.setTextDatum(textdatum_t::middle_center);
    s_backbuffer.drawString("[BACK] Return", 160, 228);

    s_tft.startWrite();
    s_backbuffer.pushSprite(&s_tft, 0, 0);
    s_tft.endWrite();
}

// ==================== WiFi 配置引导页 ====================

void draw_wifi_config_page(bool ap_active, bool connected, const char *ip)
{
    s_backbuffer.fillScreen(COLOR_BLACK);

    if (connected) {
        s_backbuffer.setTextColor(COLOR_GREEN);
        s_backbuffer.setTextSize(3);
        s_backbuffer.setTextDatum(textdatum_t::middle_center);
        s_backbuffer.drawString("Connected!", 160, 80);

        char buf[48];
        s_backbuffer.setTextColor(COLOR_WHITE);
        s_backbuffer.setTextSize(2);
        snprintf(buf, sizeof(buf), "IP: %s", ip ? ip : "N/A");
        s_backbuffer.drawString(buf, 160, 130);

        s_backbuffer.setTextColor(0x632C);
        s_backbuffer.setTextSize(1);
        s_backbuffer.drawString("[BACK] Return to menu", 160, 225);
    } else if (ap_active) {
        s_backbuffer.setTextColor(COLOR_CYAN);
        s_backbuffer.setTextSize(2);
        s_backbuffer.setTextDatum(textdatum_t::middle_center);
        s_backbuffer.drawString("WiFi Setup Active", 160, 30);

        s_backbuffer.setTextColor(COLOR_WHITE);
        s_backbuffer.setTextSize(2);
        s_backbuffer.drawString("1. Connect phone to:", 160, 70);
        s_backbuffer.setTextColor(COLOR_YELLOW);
        s_backbuffer.setTextSize(3);
        s_backbuffer.drawString("box-demo", 160, 100);

        s_backbuffer.setTextColor(COLOR_WHITE);
        s_backbuffer.setTextSize(2);
        s_backbuffer.drawString("2. Open browser:", 160, 140);
        s_backbuffer.setTextColor(COLOR_GREEN);
        s_backbuffer.setTextSize(3);
        s_backbuffer.drawString("192.168.4.1", 160, 170);

        s_backbuffer.setTextColor(0x632C);
        s_backbuffer.setTextSize(1);
        s_backbuffer.drawString("[BACK] Cancel", 160, 225);
    } else {
        s_backbuffer.setTextColor(COLOR_WHITE);
        s_backbuffer.setTextSize(2);
        s_backbuffer.setTextDatum(textdatum_t::middle_center);
        s_backbuffer.drawString("Press", 160, 90);
        s_backbuffer.setTextColor(COLOR_CYAN);
        s_backbuffer.drawString("[START]", 160, 125);
        s_backbuffer.setTextColor(COLOR_WHITE);
        s_backbuffer.drawString("to begin WiFi setup", 160, 160);

        s_backbuffer.setTextColor(0x632C);
        s_backbuffer.setTextSize(1);
        s_backbuffer.drawString("[BACK] Return to menu", 160, 225);
    }

    s_tft.startWrite();
    s_backbuffer.pushSprite(&s_tft, 0, 0);
    s_tft.endWrite();
}
