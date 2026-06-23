#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_spiffs.h"
#include "esp_heap_caps.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "lgfx_conf.h"

// ==================== 颜色定义 (RGB565) ====================
#define COLOR_RED      0xF800
#define COLOR_GREEN    0x07E0
#define COLOR_BLUE     0x001F
#define COLOR_YELLOW   0xFFE0
#define COLOR_WHITE    0xFFFF
#define COLOR_BLACK    0x0000
#define COLOR_CYAN     0x07FF
#define COLOR_MAGENTA  0xF81F
#define COLOR_GRAY     0x8410

// ==================== 按键 GPIO 定义 ====================
#define BTN_UP     GPIO_NUM_17
#define BTN_DOWN   GPIO_NUM_3
#define BTN_LEFT   GPIO_NUM_8
#define BTN_RIGHT  GPIO_NUM_18

// ==================== 按键返回值 ====================
#define BTN_NONE  0
#define BTN_U     1
#define BTN_D     2
#define BTN_L     3
#define BTN_R     4

// ==================== 状态枚举 ====================
enum AppState {
    STATE_MENU,       // 主菜单
    STATE_IMG,        // 图片浏览器
    STATE_MARQUEE,    // 图片走马灯
    STATE_GIF         // 简易GIF动图
};
static AppState current_state = STATE_MENU;

// ==================== 全局实例 ====================
static LGFX tft;
static const char* TAG = "box-demo";

// ==================== 按键状态 (边缘检测) ====================
static int prev_btn = BTN_NONE;

// ==================== 菜单状态 ====================
static int menu_selection = 0; // 0=图片浏览器, 1=图片走马灯, 2=简易GIF动图

// ==================== 图片浏览器状态 ====================
static int img_index = 0;
static int img_count = 0;

// ==================== 走马灯状态 ====================
static int scroll_offset = 0;

// ==================== GIF 状态 ====================
#define GIF_FRAME_COUNT 28
#define GIF_FRAME_W     200
#define GIF_FRAME_H     200
static LGFX_Sprite gif_frames[GIF_FRAME_COUNT];
static bool gif_loaded = false;
static int gif_speed = 10;
static int gif_frame_idx = 0;
static int64_t gif_last_frame_time = 0;

// ==================== 子页面退出弹窗状态 ====================
static bool img_exit_popup = false;
static bool marquee_exit_popup = false;
static bool gif_exit_popup = false;

// ==================== 音频播放状态 ====================
static bool audio_running = false;
static TaskHandle_t audio_task_handle = nullptr;
static i2s_chan_handle_t tx_chan = nullptr;
static void audio_playback_task(void* arg);

// ==================== 函数声明 ====================
static void init_buttons();
static int  read_buttons();
static void init_spiffs();
static int  detect_img_count();
static bool get_png_size(const char* path, int* w, int* h);
static void draw_exit_popup();
static void draw_menu();
static void handle_menu(int btn);
static void draw_img_browser();
static void handle_img(int btn);
static void draw_marquee_frame();
static void handle_marquee(int btn);
static void load_gif_frames();
static void free_gif_frames();
static void draw_gif_frame();
static void handle_gif(int btn);

// ==================== 按键初始化 ====================

static void init_buttons() {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pin_bit_mask = (1ULL << BTN_UP) | (1ULL << BTN_DOWN)
                         | (1ULL << BTN_LEFT) | (1ULL << BTN_RIGHT);
    gpio_config(&io_conf);
    ESP_LOGI(TAG, "Buttons: UP=17 DOWN=3 LEFT=8 RIGHT=18");
}

/// 边缘检测按键读取：仅在下沿（松→按）返回事件，长按不重复触发
static int read_buttons() {
    int curr = BTN_NONE;
    if (gpio_get_level(BTN_UP) == 0)    curr = BTN_U;
    if (gpio_get_level(BTN_DOWN) == 0)  curr = BTN_D;
    if (gpio_get_level(BTN_LEFT) == 0)  curr = BTN_L;
    if (gpio_get_level(BTN_RIGHT) == 0) curr = BTN_R;

    int event = (curr != BTN_NONE && curr != prev_btn) ? curr : BTN_NONE;
    prev_btn = curr;
    return event;
}

/// 长绘图后重新校准边缘检测（防止盲区丢失边沿）
static void sync_button_state() {
    prev_btn = BTN_NONE;
    if (gpio_get_level(BTN_UP) == 0)       prev_btn = BTN_U;
    else if (gpio_get_level(BTN_DOWN) == 0) prev_btn = BTN_D;
    else if (gpio_get_level(BTN_LEFT) == 0) prev_btn = BTN_L;
    else if (gpio_get_level(BTN_RIGHT) == 0) prev_btn = BTN_R;
}

// ==================== SPIFFS 初始化 ====================

static void init_spiffs() {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = false
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed (%s)", esp_err_to_name(ret));
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info("storage", &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS: total=%d used=%d", total, used);
    }
}

/// 检测图片浏览器可用图片数量 (0001~0099.png)
static int detect_img_count() {
    int count = 0;
    for (int i = 1; i <= 99; i++) {
        char path[32];
        snprintf(path, sizeof(path), "/spiffs/%04d.png", i);
        FILE* f = fopen(path, "r");
        if (f) {
            fclose(f);
            count = i;
        } else {
            break;
        }
    }
    ESP_LOGI(TAG, "Detected %d images", count);
    return count;
}

/// 解析 PNG 头部获取尺寸 (IHDR chunk)
static bool get_png_size(const char* path, int* w, int* h) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "get_png_size: fopen failed %s", path);
        return false;
    }

    uint8_t buf[24];
    if (fread(buf, 1, 24, f) != 24) {
        ESP_LOGE(TAG, "get_png_size: fread failed %s", path);
        fclose(f); return false;
    }
    fclose(f);

    if (buf[0] != 0x89 || buf[1] != 'P' || buf[2] != 'N' || buf[3] != 'G') {
        ESP_LOGE(TAG, "get_png_size: not PNG %s", path);
        return false;
    }
    if (buf[12] != 'I' || buf[13] != 'H' || buf[14] != 'D' || buf[15] != 'R') {
        ESP_LOGE(TAG, "get_png_size: no IHDR %s", path);
        return false;
    }

    *w = (buf[16] << 24) | (buf[17] << 16) | (buf[18] << 8) | buf[19];
    *h = (buf[20] << 24) | (buf[21] << 16) | (buf[22] << 8) | buf[23];
    return true;
}

// ==================== 退出确认弹窗 (复用菜单弹窗样式) ====================

static void draw_exit_popup() {
    const int PW = 240, PH = 110;
    const int PX = (320 - PW) / 2;
    const int PY = (240 - PH) / 2;

    tft.fillRect(PX, PY, PW, PH, 0x2104);
    tft.drawRect(PX, PY, PW, PH, COLOR_WHITE);
    tft.drawRect(PX+2, PY+2, PW-4, PH-4, COLOR_WHITE);

    tft.setTextColor(COLOR_WHITE);
    tft.setTextSize(2);
    tft.setTextDatum(textdatum_t::middle_center);
    tft.drawString("Exit to Menu?", 160, PY + 28);

    tft.fillRect(PX + 15, PY + 48, PW - 30, 2, COLOR_CYAN);

    tft.setTextSize(1.5f);
    tft.setTextColor(COLOR_CYAN);
    tft.drawString("Yes: [DOWN]", 160, PY + 74);
    tft.setTextColor(COLOR_GRAY);
    tft.drawString("No:  [UP]",   160, PY + 94);
}

// ==================== 主菜单 ====================

static const char* menu_items[] = { "IMG Browser", "IMG Marquee", "GIF Player" };
#define MENU_COUNT 3

// 表格布局常量
static const int TBL_X = 18, TBL_Y = 55, TBL_W = 284, TBL_H = 140;
static const int ROW_H = 45;
static const int TITLE_Y = 25;
static const int HINT_Y = 228;

static bool menu_popup = false;     // 确认弹窗状态

// ---------- 绘制确认弹窗 (60% 屏幕居中) ----------
static void draw_confirm_popup() {
    const int PW = 270, PH = 128;
    const int PX = (320 - PW) / 2;
    const int PY = (240 - PH) / 2;

    tft.fillRect(PX, PY, PW, PH, 0x0861);
    tft.drawRect(PX, PY, PW, PH, COLOR_WHITE);
    tft.drawRect(PX+2, PY+2, PW-4, PH-4, COLOR_WHITE);

    tft.setTextColor(COLOR_WHITE);
    tft.setTextSize(2);
    tft.setTextDatum(textdatum_t::middle_center);
    char buf[64];
    snprintf(buf, sizeof(buf), "Selected: %s", menu_items[menu_selection]);
    tft.drawString(buf, 160, PY + 24);

    tft.fillRect(PX + 15, PY + 44, PW - 30, 2, COLOR_CYAN);
    tft.drawString("Enter?", 160, PY + 68);

    tft.setTextSize(1.5f);
    tft.setTextColor(COLOR_CYAN);
    tft.drawString("Yes: ->", 100, PY + 100);
    tft.setTextColor(COLOR_GRAY);
    tft.drawString("No: <-", 220, PY + 100);
}

// ---------- 绘制主菜单 ----------
static void draw_menu() {
    tft.startWrite();
    tft.fillRect(0, 0, 320, 240, COLOR_BLACK);

    // 标题
    tft.setTextColor(COLOR_WHITE);
    tft.setTextSize(4);
    tft.setTextDatum(textdatum_t::middle_center);
    tft.drawString("box-demo", 160, TITLE_Y);

    // 表格外框
    tft.drawRect(TBL_X, TBL_Y, TBL_W, TBL_H, COLOR_WHITE);
    tft.drawRect(TBL_X+1, TBL_Y+1, TBL_W-2, TBL_H-2, COLOR_GRAY);

    for (int i = 0; i < MENU_COUNT; i++) {
        int ry = TBL_Y + i * ROW_H;

        if (i > 0)
            tft.fillRect(TBL_X + 1, ry, TBL_W - 2, 1, COLOR_GRAY);

        uint16_t row_bg = (i == menu_selection) ? 0x18E3 : 0x0000;
        tft.fillRect(TBL_X + 2, ry + 1, TBL_W - 4, ROW_H - 2, row_bg);

        tft.setTextDatum(textdatum_t::middle_center);
        if (i == menu_selection) {
            tft.setTextColor(COLOR_CYAN);
            tft.setTextSize(1);
            tft.drawString(">", TBL_X + 25, ry + ROW_H/2);
            tft.setTextSize(2);
            tft.setTextColor(COLOR_WHITE);
            tft.drawString(menu_items[i], 160, ry + ROW_H/2);
        } else {
            tft.setTextColor(COLOR_GRAY);
            tft.setTextSize(2);
            tft.drawString(menu_items[i], 160, ry + ROW_H/2);
        }
    }

    // 底部提示
    tft.setTextColor(0x632C);
    tft.setTextSize(1);
    tft.setTextDatum(textdatum_t::middle_center);
    tft.drawString("[UP][DOWN] Select    [RIGHT] Enter", 160, HINT_Y);

    if (menu_popup) draw_confirm_popup();
    tft.endWrite();
    sync_button_state();
}

// ---------- 处理菜单按键 ----------
static void handle_menu(int btn) {
    if (menu_popup) {
        if (btn == BTN_R) {
            menu_popup = false;
            current_state = (AppState)(STATE_IMG + menu_selection);
            ESP_LOGI(TAG, "Enter state %d", current_state);
            return;
        }
        if (btn == BTN_L) {
            menu_popup = false;
            draw_menu();
            return;
        }
        return;
    }

    switch (btn) {
        case BTN_U:
            menu_selection = (menu_selection - 1 + MENU_COUNT) % MENU_COUNT;
            draw_menu();
            break;
        case BTN_D:
            menu_selection = (menu_selection + 1) % MENU_COUNT;
            draw_menu();
            break;
        case BTN_R:
            menu_popup = true;
            draw_menu();
            break;
    }
}

// ==================== 功能一：图片浏览器 ====================

static bool img_need_init = true;
static int img_w_cache[100];
static int img_h_cache[100];

// 布局: 标题 y=0~13 (14px) + 图片 y=14~213 (200px) + 提示 y=214~239 (26px)
#define IMG_AREA_Y 14
#define IMG_AREA_H 200
#define IMG_HINT_Y 227

static void draw_img_browser() {
    int idx = img_index + 1;
    char path[32];
    snprintf(path, sizeof(path), "/spiffs/%04d.png", idx);
    ESP_LOGI(TAG, "IMG loading: %s", path);

    if (img_w_cache[img_index] == 0) {
        if (!get_png_size(path, &img_w_cache[img_index], &img_h_cache[img_index])) {
            tft.startWrite();
            tft.fillScreen(COLOR_BLACK);
            tft.setTextColor(COLOR_RED); tft.setTextSize(1);
            tft.setTextDatum(textdatum_t::middle_center);
            tft.drawString("Load failed!", 160, 120);
            tft.endWrite(); sync_button_state(); return;
        }
    }

    int img_w = img_w_cache[img_index];
    int img_h = img_h_cache[img_index];

    FILE* fp = fopen(path, "rb");
    if (!fp) {
        tft.startWrite(); tft.fillScreen(COLOR_BLACK);
        tft.setTextColor(COLOR_RED);
        tft.drawString("File open fail!", 160, 120);
        tft.endWrite(); sync_button_state(); return;
    }
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    uint8_t* png_buf = (uint8_t*)heap_caps_malloc(fsize, MALLOC_CAP_SPIRAM);
    if (!png_buf) { ESP_LOGE(TAG, "IMG PSRAM alloc(%ld) failed", fsize); fclose(fp); return; }
    fread(png_buf, 1, fsize, fp);
    fclose(fp);

    // 离屏 Sprite 合成 (PSRAM, 消除闪烁)
    LGFX_Sprite spr(&tft);
    spr.setPsram(true);
    spr.createSprite(320, IMG_AREA_H);
    spr.fillScreen(COLOR_BLACK);
    int sx = (320 - img_w) / 2;
    int sy = (IMG_AREA_H - img_h) / 2;
    spr.drawPng(png_buf, fsize, sx, sy);
    heap_caps_free(png_buf);

    // 关显示 → 写 GRAM（标题+图片+提示）→ 开显示，瞬间完整呈现
    tft.startWrite();
    tft.writeCommand(0x28);
    tft.fillScreen(COLOR_BLACK);

    char buf[64];
    snprintf(buf, sizeof(buf), "Browser[%04d/%04d]", idx, img_count);
    tft.setTextColor(COLOR_WHITE); tft.setTextSize(1);
    tft.setTextDatum(textdatum_t::middle_center);
    tft.drawString(buf, 160, 7);

    spr.pushSprite(0, IMG_AREA_Y);
    spr.deleteSprite();

    tft.setTextColor(0x632C);
    tft.drawString("  [LEFT] Prev  [RIGHT] Next  [DOWN] Back  ", 160, IMG_HINT_Y);

    if (img_exit_popup) draw_exit_popup();
    tft.writeCommand(0x29);
    tft.endWrite();
    sync_button_state();
}

static void handle_img(int btn) {
    if (img_need_init) {
        tft.fillScreen(COLOR_BLACK);
        tft.setTextColor(COLOR_WHITE); tft.setTextSize(2);
        tft.setTextDatum(textdatum_t::middle_center);
        tft.drawString("Loading...", 160, 120);

        img_count = detect_img_count();
        img_index = 0;
        img_need_init = false;
        img_exit_popup = false;
        memset(img_w_cache, 0, sizeof(img_w_cache));
        memset(img_h_cache, 0, sizeof(img_h_cache));

        if (!audio_running) {
            audio_running = true;
            i2s_channel_enable(tx_chan);
            xTaskCreate(audio_playback_task, "audio", 4096, NULL, 1, &audio_task_handle);
        }
        draw_img_browser();
        return;
    }

    if (img_exit_popup) {
        if (btn == BTN_D) {
            img_exit_popup = false;
            audio_running = false;
            i2s_channel_disable(tx_chan);
            current_state = STATE_MENU;
            img_need_init = true;
            return;
        }
        if (btn == BTN_U) {
            img_exit_popup = false;
            draw_img_browser();
            return;
        }
        return;
    }

    switch (btn) {
        case BTN_L:
            img_index = (img_index - 1 + img_count) % img_count;
            draw_img_browser();
            break;
        case BTN_R:
            img_index = (img_index + 1) % img_count;
            draw_img_browser();
            break;
        case BTN_D:
            img_exit_popup = true;
            draw_img_browser();
            break;
    }
}

// ==================== 功能二：图片走马灯 ====================

#define MARQUEE_W 500
#define MARQUEE_H 150

static bool marquee_need_init = true;
static uint16_t* marquee_raw = nullptr;

// 布局: 标题 y=0~13 (14px) + 走马灯 y=41~190 (150px居中) + 提示 y=218~239
#define MARQUEE_TOP_H 14
#define MARQUEE_IMG_Y 41   // 14 + (240-14-22-150)/2 = 41
#define MARQUEE_HINT_Y 229

static void draw_marquee_frame() {
    tft.startWrite();
    tft.fillRect(0, 0, 320, MARQUEE_TOP_H, COLOR_BLACK);

    tft.setTextColor(COLOR_WHITE); tft.setTextSize(1);
    tft.setTextDatum(textdatum_t::middle_center);
    tft.drawString("Marquee 500x150", 160, 7);

    if (marquee_raw) {
        tft.setSwapBytes(true);
        tft.pushImage(-scroll_offset, MARQUEE_IMG_Y, MARQUEE_W, MARQUEE_H, marquee_raw);
        tft.pushImage(MARQUEE_W - scroll_offset, MARQUEE_IMG_Y, MARQUEE_W, MARQUEE_H, marquee_raw);
        tft.setSwapBytes(false);
    }

    tft.fillRect(0, 218, 320, 22, COLOR_BLACK);
    tft.setTextColor(0x632C);
    tft.drawString("[DOWN] Back", 160, MARQUEE_HINT_Y);

    if (marquee_exit_popup) draw_exit_popup();
    tft.endWrite();
    sync_button_state();
}

static void handle_marquee(int btn) {
    if (marquee_need_init) {
        tft.fillScreen(COLOR_BLACK);
        tft.setTextColor(COLOR_WHITE); tft.setTextSize(2);
        tft.setTextDatum(textdatum_t::middle_center);
        tft.drawString("Loading...", 160, 120);

        marquee_exit_popup = false;
        scroll_offset = 0;
        if (!marquee_raw) {
            FILE* fp = fopen("/spiffs/500x150.raw", "rb");
            if (fp) {
                size_t sz = MARQUEE_W * MARQUEE_H * 2;
                marquee_raw = (uint16_t*)heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
                if (marquee_raw) {
                    fread(marquee_raw, 1, sz, fp);
                    ESP_LOGI(TAG, "Marquee raw loaded: %dx%d", MARQUEE_W, MARQUEE_H);
                }
                fclose(fp);
            } else {
                ESP_LOGE(TAG, "Marquee raw fopen failed");
            }
        }
        marquee_need_init = false;

        if (!audio_running) {
            audio_running = true;
            i2s_channel_enable(tx_chan);
            xTaskCreate(audio_playback_task, "audio", 4096, NULL, 1, &audio_task_handle);
        }
        draw_marquee_frame();
        return;
    }

    if (marquee_exit_popup) {
        if (btn == BTN_D) {
            marquee_exit_popup = false;
            audio_running = false;
            i2s_channel_disable(tx_chan);
            if (marquee_raw) { heap_caps_free(marquee_raw); marquee_raw = nullptr; }
            current_state = STATE_MENU;
            marquee_need_init = true;
            return;
        }
        if (btn == BTN_U) {
            marquee_exit_popup = false;
            draw_marquee_frame();
            return;
        }
        return;
    }

    if (btn == BTN_D) {
        marquee_exit_popup = true;
        draw_marquee_frame();
        return;
    }

    scroll_offset = (scroll_offset + 2) % MARQUEE_W;
    draw_marquee_frame();
}

// ==================== 功能三：简易GIF动图 ====================

static bool gif_need_init = true;

static void load_gif_frames() {
    ESP_LOGI(TAG, "Loading %d GIF frames...", GIF_FRAME_COUNT);
    int ok_count = 0;
    for (int i = 0; i < GIF_FRAME_COUNT; i++) {
        gif_frames[i].setPsram(true);
        gif_frames[i].createSprite(GIF_FRAME_W, GIF_FRAME_H);
        int sw = gif_frames[i].width();
        if (sw == 0) {
            ESP_LOGE(TAG, "GIF frame[%d] createSprite OOM", i);
            continue;
        }
        gif_frames[i].fillScreen(COLOR_BLACK);
        char path[32];
        snprintf(path, sizeof(path), "/spiffs/gif_%04d.png", i + 1);
        FILE* fp = fopen(path, "rb");
        if (fp) {
            fseek(fp, 0, SEEK_END);
            long fsize = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            uint8_t* buf = (uint8_t*)malloc(fsize);
            if (buf) {
                fread(buf, 1, fsize, fp);
                bool ok = gif_frames[i].drawPng(buf, fsize, 0, 0);
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
    gif_loaded = true;
    ESP_LOGI(TAG, "GIF: %d/%d frames OK", ok_count, GIF_FRAME_COUNT);
}

static void free_gif_frames() {
    for (int i = 0; i < GIF_FRAME_COUNT; i++) {
        gif_frames[i].deleteSprite();
    }
    gif_loaded = false;
}

// 布局: 标题 y=0~11 (12px) + 帧 y=14~213 (200px) + 提示 y=214~239
#define GIF_TOP_H 12
#define GIF_FRAME_Y 14
#define GIF_HINT_Y 227

static void draw_gif_frame() {
    tft.startWrite();
    tft.fillRect(0, 0, 320, GIF_TOP_H, COLOR_BLACK);

    // 帧号+Speed+速度条 同一行
    char buf[64];
    snprintf(buf, sizeof(buf), "GIF[%02d/%02d]  Speed:%d", gif_frame_idx + 1, GIF_FRAME_COUNT, gif_speed);
    tft.setTextColor(COLOR_WHITE); tft.setTextSize(1);
    tft.setTextDatum(textdatum_t::top_left);
    tft.drawString(buf, 6, 3);

    int filled = (gif_speed * 10) / 20;
    const int bx = 135, by = 5, bw = 8, bh = 4;
    for (int i = 0; i < filled; i++)
        tft.fillRect(bx + i * (bw + 1), by, bw, bh, COLOR_GREEN);
    for (int i = filled; i < 10; i++)
        tft.fillRect(bx + i * (bw + 1), by, bw, bh, COLOR_GRAY);

    int x = (320 - GIF_FRAME_W) / 2;
    gif_frames[gif_frame_idx].pushSprite(&tft, x, GIF_FRAME_Y);

    tft.fillRect(0, 214, 320, 26, COLOR_BLACK);
    tft.setTextColor(0x632C);
    tft.setTextDatum(textdatum_t::middle_center);
    tft.drawString("  [LEFT] Slower  [RIGHT] Faster  [DOWN] Back  ", 160, GIF_HINT_Y);

    if (gif_exit_popup) draw_exit_popup();
    tft.endWrite();
    sync_button_state();
}

static void handle_gif(int btn) {
    if (gif_need_init) {
        tft.fillScreen(COLOR_BLACK);
        tft.setTextColor(COLOR_WHITE); tft.setTextSize(2);
        tft.setTextDatum(textdatum_t::middle_center);
        tft.drawString("Loading...", 160, 120);

        gif_exit_popup = false;
        gif_frame_idx = 0;
        gif_speed = 10;
        gif_last_frame_time = esp_timer_get_time() / 1000;
        if (!gif_loaded) load_gif_frames();
        gif_need_init = false;

        if (!audio_running) {
            audio_running = true;
            i2s_channel_enable(tx_chan);
            xTaskCreate(audio_playback_task, "audio", 4096, NULL, 1, &audio_task_handle);
        }
        draw_gif_frame();
        return;
    }

    if (gif_exit_popup) {
        if (btn == BTN_D) {
            gif_exit_popup = false;
            audio_running = false;
            i2s_channel_disable(tx_chan);
            free_gif_frames();
            current_state = STATE_MENU;
            gif_need_init = true;
            return;
        }
        if (btn == BTN_U) {
            gif_exit_popup = false;
            draw_gif_frame();
            return;
        }
        return;
    }

    switch (btn) {
        case BTN_L:
            if (gif_speed > 1)  { gif_speed--; draw_gif_frame(); }
            break;
        case BTN_R:
            if (gif_speed < 20) { gif_speed++; draw_gif_frame(); }
            break;
        case BTN_D:
            gif_exit_popup = true;
            draw_gif_frame();
            return;
    }

    int delay_ms = 500 - (gif_speed - 1) * 25;
    int64_t now = esp_timer_get_time() / 1000;
    if (now - gif_last_frame_time >= delay_ms) {
        gif_frame_idx = (gif_frame_idx + 1) % GIF_FRAME_COUNT;
        gif_last_frame_time = now;
        draw_gif_frame();
    }
}

// ==================== 音频播放任务 ====================

static void audio_playback_task(void* arg) {
    const char* path = "/spiffs/music.wav";
    while (audio_running) {
        FILE* f = fopen(path, "rb");
        if (!f) { vTaskDelay(pdMS_TO_TICKS(1000)); continue; }
        fseek(f, 44, SEEK_SET);
        int16_t buf[512];
        size_t rd;
        while (audio_running && (rd = fread(buf, 1, sizeof(buf), f)) > 0) {
            size_t wr;
            i2s_channel_write(tx_chan, buf, rd, &wr, portMAX_DELAY);
        }
        fclose(f);
    }
    audio_task_handle = nullptr;
    vTaskDelete(NULL);
}

// ==================== 主入口 ====================

extern "C" void app_main() {
    ESP_LOGI(TAG, "===== box-demo Starting =====");

    tft.init();
    tft.setRotation(1);
    tft.setBrightness(255);
    ESP_LOGI(TAG, "TFT: %ldx%ld", tft.width(), tft.height());

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, &tx_chan, NULL);
    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(22050),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = { .mclk = I2S_GPIO_UNUSED, .bclk = GPIO_NUM_5, .ws = GPIO_NUM_4,
                      .dout = GPIO_NUM_6, .din = I2S_GPIO_UNUSED,
                      .invert_flags = { .mclk_inv = 0, .bclk_inv = 0, .ws_inv = 0 } },
    };
    i2s_channel_init_std_mode(tx_chan, &std_cfg);
    i2s_channel_enable(tx_chan);
    ESP_LOGI(TAG, "I2S audio: 22050Hz 16bit mono");

    init_buttons();
    init_spiffs();

    draw_menu();

    while (1) {
        int btn = read_buttons();

        switch (current_state) {
            case STATE_MENU:
            case STATE_IMG:
                // 菜单和图片浏览器: 只响应按键
                if (btn != BTN_NONE) {
                    if (current_state == STATE_MENU) handle_menu(btn);
                    else handle_img(btn);
                }
                vTaskDelay(pdMS_TO_TICKS(10));
                break;

            case STATE_MARQUEE:
                // 走马灯: 始终推进+渲染（含按键检测）
                handle_marquee(btn);
                vTaskDelay(pdMS_TO_TICKS(30));
                break;

            case STATE_GIF:
                // GIF: 始终推进+渲染（含按键检测）
                handle_gif(btn);
                vTaskDelay(pdMS_TO_TICKS(10));
                break;
        }

        // 状态切换回 MENU 时触发的重绘, 由 handle_menu 内部处理
    }
}

