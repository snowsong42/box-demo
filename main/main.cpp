#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_spiffs.h"
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

// ==================== 防抖参数 ====================
static const int64_t BUTTON_DEBOUNCE_MS = 200;

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
static const char* TAG = "BUTT-demo";

// ==================== 按键状态 ====================
static int64_t last_button_time = 0;

// ==================== 菜单状态 ====================
static int menu_selection = 0; // 0=图片浏览器, 1=图片走马灯, 2=简易GIF动图

// ==================== 图片浏览器状态 ====================
static int img_index = 0;
static int img_count = 0;

// ==================== 走马灯状态 ====================
static LGFX_Sprite marquee_spr;
static bool marquee_spr_created = false;
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

/// 等待指定按键释放（防止长按穿透到下一状态）
static void wait_button_release(gpio_num_t btn) {
    while (gpio_get_level(btn) == 0) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    last_button_time = esp_timer_get_time() / 1000; // 释放后重置防抖
}

/// 带防抖的按键读取，返回 BTN_U/D/L/R 或 BTN_NONE
static int read_buttons() {
    int64_t now = esp_timer_get_time() / 1000;
    if (now - last_button_time < BUTTON_DEBOUNCE_MS) {
        return BTN_NONE;
    }

    int pressed = BTN_NONE;
    if (gpio_get_level(BTN_UP) == 0)    pressed = BTN_U;
    if (gpio_get_level(BTN_DOWN) == 0)  pressed = BTN_D;
    if (gpio_get_level(BTN_LEFT) == 0)  pressed = BTN_L;
    if (gpio_get_level(BTN_RIGHT) == 0) pressed = BTN_R;

    if (pressed != BTN_NONE) {
        last_button_time = now;
    }
    return pressed;
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
    tft.fillScreen(COLOR_BLACK);

    // 标题
    tft.setTextColor(COLOR_WHITE);
    tft.setTextSize(4);
    tft.setTextDatum(textdatum_t::middle_center);
    tft.drawString("BUTT-demo", 160, TITLE_Y);

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
            wait_button_release(BTN_RIGHT);
            break;
    }
}

// ==================== 功能一：图片浏览器 ====================

static bool img_need_init = true;
static int img_w_cache[100];
static int img_h_cache[100];

static void draw_img_browser() {
    tft.fillScreen(COLOR_BLACK);

    int idx = img_index + 1;
    char buf[64];
    snprintf(buf, sizeof(buf), "Browser [%04d/%04d]", idx, img_count);
    tft.setTextColor(COLOR_WHITE);
    tft.setTextSize(1);
    tft.setTextDatum(textdatum_t::middle_center);
    tft.drawString(buf, 160, 12);

    char path[32];
    snprintf(path, sizeof(path), "/spiffs/%04d.png", idx);
    ESP_LOGI(TAG, "IMG loading: %s", path);

    if (img_w_cache[img_index] == 0) {
        if (!get_png_size(path, &img_w_cache[img_index], &img_h_cache[img_index])) {
            ESP_LOGE(TAG, "IMG get_png_size failed for %s", path);
            tft.setTextColor(COLOR_RED);
            tft.setTextSize(1);
            tft.drawString("Load failed!", 160, 120);
            goto draw_hint;
        }
    }

    {
        int img_w = img_w_cache[img_index];
        int img_h = img_h_cache[img_index];
        int x = (320 - img_w) / 2;
        int y = 20 + (200 - img_h) / 2;
        ESP_LOGI(TAG, "IMG pos: %s (%dx%d) -> (%d,%d)", path, img_w, img_h, x, y);

        FILE* fp = fopen(path, "rb");
        if (!fp) {
            ESP_LOGE(TAG, "IMG fopen failed: %s", path);
            tft.setTextColor(COLOR_RED);
            tft.drawString("File open fail!", 160, 120);
        } else {
            fseek(fp, 0, SEEK_END);
            long fsize = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            uint8_t* png_buf = (uint8_t*)malloc(fsize);
            if (png_buf) {
                fread(png_buf, 1, fsize, fp);
                if (!tft.drawPng(png_buf, fsize, x, y)) {
                    ESP_LOGE(TAG, "IMG drawPng failed: %s", path);
                    tft.setTextColor(COLOR_RED);
                    tft.drawString("PNG decode fail!", 160, 120);
                }
                free(png_buf);
            } else {
                ESP_LOGE(TAG, "IMG malloc(%ld) failed", fsize);
            }
            fclose(fp);
        }
    }

draw_hint:
    tft.setTextColor(COLOR_GRAY);
    tft.setTextSize(1);
    tft.setTextDatum(textdatum_t::middle_center);
    tft.drawString("[LEFT]Prev [RIGHT]Next [DOWN]Exit", 160, 230);

    if (img_exit_popup) draw_exit_popup();
}

static void handle_img(int btn) {
    if (img_need_init) {
        img_count = detect_img_count();
        img_index = 0;
        img_need_init = false;
        img_exit_popup = false;
        memset(img_w_cache, 0, sizeof(img_w_cache));
        memset(img_h_cache, 0, sizeof(img_h_cache));
        draw_img_browser();
        return;
    }

    if (img_exit_popup) {
        if (btn == BTN_D) {
            img_exit_popup = false;
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
            wait_button_release(BTN_DOWN);
            break;
    }
}

// ==================== 功能二：图片走马灯 ====================

static bool marquee_need_init = true;

static void draw_marquee_frame() {
    tft.fillScreen(COLOR_BLACK);

    marquee_spr.pushSprite(&tft, -scroll_offset, 45, COLOR_BLACK);
    marquee_spr.pushSprite(&tft, 500 - scroll_offset, 45, COLOR_BLACK);

    tft.setTextColor(COLOR_WHITE);
    tft.setTextSize(1);
    tft.setTextDatum(textdatum_t::middle_center);
    tft.drawString("Marquee 500x150 scrolling...", 160, 12);

    tft.setTextColor(COLOR_GRAY);
    tft.drawString("[DOWN]Exit", 160, 230);

    if (marquee_exit_popup) draw_exit_popup();
}

static void handle_marquee(int btn) {
    if (marquee_need_init) {
        marquee_exit_popup = false;
        scroll_offset = 0;
        if (marquee_spr_created) { marquee_spr.deleteSprite(); marquee_spr_created = false; }
        marquee_spr.createSprite(500, 150);
        marquee_spr_created = true;
        marquee_spr.drawJpgFile("/spiffs/500x150.jpg", 0, 0);
        marquee_need_init = false;
        draw_marquee_frame();
        return;
    }

    if (marquee_exit_popup) {
        if (btn == BTN_D) {
            marquee_exit_popup = false;
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
        wait_button_release(BTN_DOWN);
        return;
    }

    scroll_offset = (scroll_offset + 2) % 500;
    draw_marquee_frame();
}

// ==================== 功能三：简易GIF动图 ====================

static bool gif_need_init = true;

static void load_gif_frames() {
    ESP_LOGI(TAG, "Loading %d GIF frames...", GIF_FRAME_COUNT);
    for (int i = 0; i < GIF_FRAME_COUNT; i++) {
        gif_frames[i].createSprite(GIF_FRAME_W, GIF_FRAME_H);
        char path[32];
        snprintf(path, sizeof(path), "/spiffs/gif_%04d.png", i + 1);
        gif_frames[i].drawPngFile((const char*)path, 0, 0);
    }
    gif_loaded = true;
    ESP_LOGI(TAG, "GIF frames loaded");
}

[[maybe_unused]] static void free_gif_frames() {
    for (int i = 0; i < GIF_FRAME_COUNT; i++) {
        gif_frames[i].deleteSprite();
    }
    gif_loaded = false;
}

static void draw_gif_frame() {
    tft.fillScreen(COLOR_BLACK);

    int x = (320 - GIF_FRAME_W) / 2;
    int y = 45;
    gif_frames[gif_frame_idx].pushSprite(&tft, x, y, COLOR_BLACK);

    char buf[64];
    snprintf(buf, sizeof(buf), "GIF Frame[%02d/%02d]", gif_frame_idx + 1, GIF_FRAME_COUNT);
    tft.setTextColor(COLOR_WHITE);
    tft.setTextSize(1);
    tft.setTextDatum(textdatum_t::middle_center);
    tft.drawString(buf, 160, 12);

    int filled = (gif_speed * 10) / 20;
    for (int i = 0; i < filled; i++)
        tft.fillRect(120 + i * 6, 25, 5, 6, COLOR_GREEN);
    for (int i = filled; i < 10; i++)
        tft.fillRect(120 + i * 6, 25, 5, 6, COLOR_GRAY);

    snprintf(buf, sizeof(buf), "Speed: (%d/20)", gif_speed);
    tft.drawString(buf, 160, 38);

    tft.setTextColor(COLOR_GRAY);
    tft.drawString("[LEFT]Slower [RIGHT]Faster [DOWN]Exit", 160, 230);

    if (gif_exit_popup) draw_exit_popup();
}

static void handle_gif(int btn) {
    if (gif_need_init) {
        gif_exit_popup = false;
        gif_frame_idx = 0;
        gif_speed = 10;
        gif_last_frame_time = esp_timer_get_time() / 1000;
        if (!gif_loaded) load_gif_frames();
        gif_need_init = false;
        draw_gif_frame();
        return;
    }

    if (gif_exit_popup) {
        if (btn == BTN_D) {
            gif_exit_popup = false;
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
            wait_button_release(BTN_DOWN);
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

// ==================== 主入口 ====================

extern "C" void app_main() {
    ESP_LOGI(TAG, "===== BUTT-demo Starting =====");

    tft.init();
    tft.setRotation(1);
    tft.setBrightness(255);
    ESP_LOGI(TAG, "TFT: %ldx%ld", tft.width(), tft.height());

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

