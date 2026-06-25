/*
 * SPDX-FileCopyrightText: 2025 box-demo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.cpp
 * @brief 应用入口 + 状态机调度
 *
 * 组件化架构：buttons / display / storage / audio 各司其职，
 * main 仅负责初始化编排和状态机分发。
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "buttons.h"
#include "display.h"
#include "storage.h"
#include "audio.h"

static const char *TAG = "box-demo";

// ==================== 状态枚举 ====================
enum AppState {
    STATE_MENU,
    STATE_IMG,
    STATE_MARQUEE,
    STATE_GIF,
};
static AppState s_current_state = STATE_MENU;

// ==================== 全局动作冷却 ====================
static int64_t s_last_action_time = 0;

// ==================== 菜单状态 ====================
static int s_menu_selection = 0;
static bool s_menu_popup = false;
static bool s_menu_popup_armed = false;

// ==================== 图片浏览器状态 ====================
static int s_img_index = 0;
static int s_img_count = 0;
static int s_img_w_cache[100];
static int s_img_h_cache[100];
static bool s_img_need_init = true;
static bool s_img_exit_popup = false;
static bool s_img_exit_armed = false;

// ==================== 走马灯状态 ====================
static bool s_marquee_need_init = true;
static uint16_t *s_marquee_raw = nullptr;
static int s_scroll_offset = 0;
static bool s_marquee_exit_popup = false;
static bool s_marquee_exit_armed = false;

// ==================== GIF 状态 ====================
static bool s_gif_need_init = true;
static int s_gif_speed = 18;
static int s_gif_frame_idx = 0;
static int64_t s_gif_last_frame_time = 0;
static bool s_gif_exit_popup = false;
static bool s_gif_exit_armed = false;

// ==================== 函数声明 ====================
static void handle_menu(int btn);
static void handle_img(int btn);
static void handle_marquee(int btn);
static void handle_gif(int btn);

// ==================== 主菜单 Handler ====================

static void handle_menu(int btn)
{
    if (s_menu_popup) {
        bool yes = (gpio_get_level(BTN_RIGHT) == 0);
        bool no  = (gpio_get_level(BTN_LEFT) == 0);
        if (!yes && !no) s_menu_popup_armed = false;
        if (s_menu_popup_armed) { draw_menu(s_menu_selection, s_menu_popup); return; }

        int64_t now = esp_timer_get_time() / 1000;
        if (yes && (now - s_last_action_time >= 200)) {
            s_last_action_time = now;
            ESP_LOGI(TAG, "ENTER: %s", s_menu_selection == 0 ? "IMG Browser"
                              : s_menu_selection == 1 ? "IMG Marquee" : "GIF Player");
            s_menu_popup = false;
            s_current_state = (AppState)(STATE_IMG + s_menu_selection);
            draw_menu(s_menu_selection, s_menu_popup);
        } else if (no && (now - s_last_action_time >= 200)) {
            s_last_action_time = now;
            ESP_LOGI(TAG, "CANCEL");
            s_menu_popup = false;
            draw_menu(s_menu_selection, s_menu_popup);
        } else {
            draw_menu(s_menu_selection, s_menu_popup);
        }
        return;
    }

    switch (btn) {
        case BTN_U:
            s_menu_selection = (s_menu_selection - 1 + 3) % 3;
            draw_menu(s_menu_selection, s_menu_popup);
            break;
        case BTN_D:
            s_menu_selection = (s_menu_selection + 1) % 3;
            draw_menu(s_menu_selection, s_menu_popup);
            break;
        case BTN_R:
            ESP_LOGI(TAG, "POPUP: enter %s", s_menu_selection == 0 ? "IMG Browser"
                                : s_menu_selection == 1 ? "IMG Marquee" : "GIF Player");
            s_menu_popup = true;
            s_menu_popup_armed = true;
            draw_menu(s_menu_selection, s_menu_popup);
            break;
    }
}

// ==================== 图片浏览器 Handler ====================

static void handle_img(int btn)
{
    if (s_img_need_init) {
        LGFX_Sprite *bb = display_backbuffer();
        bb->fillScreen(COLOR_BLACK);
        bb->setTextColor(COLOR_WHITE); bb->setTextSize(2);
        bb->setTextDatum(textdatum_t::middle_center);
        bb->drawString("Loading...", 160, 120);
        display_tft()->startWrite();
        bb->pushSprite(display_tft(), 0, 0);
        display_tft()->endWrite();

        s_img_count = detect_img_count();
        ESP_LOGI(TAG, "INIT: IMG browser (%d images)", s_img_count);
        s_img_index = 0;
        s_img_need_init = false;
        s_img_exit_popup = false;
        memset(s_img_w_cache, 0, sizeof(s_img_w_cache));
        memset(s_img_h_cache, 0, sizeof(s_img_h_cache));

        // 缓存 PNG 尺寸
        for (int i = 0; i < s_img_count; i++) {
            char path[32];
            snprintf(path, sizeof(path), "/spiffs/%04d.png", i + 1);
            get_png_size(path, &s_img_w_cache[i], &s_img_h_cache[i]);
        }

        if (!audio_is_running()) audio_start();
        draw_img_browser(s_img_index, s_img_count, s_img_w_cache, s_img_h_cache, s_img_exit_popup);
        return;
    }

    if (s_img_exit_popup) {
        bool yes = (gpio_get_level(BTN_DOWN) == 0);
        bool no  = (gpio_get_level(BTN_UP) == 0);
        if (!yes && !no) s_img_exit_armed = false;
        if (s_img_exit_armed) { draw_img_browser(s_img_index, s_img_count, s_img_w_cache, s_img_h_cache, s_img_exit_popup); return; }

        int64_t now = esp_timer_get_time() / 1000;
        if (yes && (now - s_last_action_time >= 200)) {
            s_last_action_time = now;
            ESP_LOGI(TAG, "EXIT: IMG -> MENU");
            s_img_exit_popup = false;
            s_img_need_init = true;
            audio_stop();
            s_current_state = STATE_MENU;
            draw_menu(s_menu_selection, s_menu_popup);
        } else if (no && (now - s_last_action_time >= 200)) {
            s_last_action_time = now;
            ESP_LOGI(TAG, "CANCEL");
            s_img_exit_popup = false;
            draw_img_browser(s_img_index, s_img_count, s_img_w_cache, s_img_h_cache, s_img_exit_popup);
        } else {
            draw_img_browser(s_img_index, s_img_count, s_img_w_cache, s_img_h_cache, s_img_exit_popup);
        }
        return;
    }

    switch (btn) {
        case BTN_L:
            s_img_index = (s_img_index - 1 + s_img_count) % s_img_count;
            draw_img_browser(s_img_index, s_img_count, s_img_w_cache, s_img_h_cache, s_img_exit_popup);
            break;
        case BTN_R:
            s_img_index = (s_img_index + 1) % s_img_count;
            draw_img_browser(s_img_index, s_img_count, s_img_w_cache, s_img_h_cache, s_img_exit_popup);
            break;
        case BTN_D:
            ESP_LOGI(TAG, "POPUP: exit IMG?");
            s_img_exit_popup = true;
            s_img_exit_armed = true;
            draw_img_browser(s_img_index, s_img_count, s_img_w_cache, s_img_h_cache, s_img_exit_popup);
            break;
    }
}

// ==================== 走马灯 Handler ====================

static void handle_marquee(int btn)
{
    if (s_marquee_need_init) {
        LGFX_Sprite *bb = display_backbuffer();
        bb->fillScreen(COLOR_BLACK);
        bb->setTextColor(COLOR_WHITE); bb->setTextSize(2);
        bb->setTextDatum(textdatum_t::middle_center);
        bb->drawString("Loading...", 160, 120);
        display_tft()->startWrite();
        bb->pushSprite(display_tft(), 0, 0);
        display_tft()->endWrite();

        ESP_LOGI(TAG, "INIT: Marquee 500x150");
        s_marquee_exit_popup = false;
        s_scroll_offset = 0;
        if (!s_marquee_raw) {
            FILE *fp = fopen("/spiffs/500x150.raw", "rb");
            if (fp) {
                size_t sz = 500UL * 150 * 2;
                s_marquee_raw = (uint16_t *)heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
                if (s_marquee_raw) {
                    fread(s_marquee_raw, 1, sz, fp);
                    ESP_LOGI(TAG, "Marquee raw loaded: 500x150");
                }
                fclose(fp);
            } else {
                ESP_LOGE(TAG, "Marquee raw fopen failed");
            }
        }
        s_marquee_need_init = false;

        if (!audio_is_running()) audio_start();
        draw_marquee_frame(s_marquee_raw, s_scroll_offset, s_marquee_exit_popup);
        return;
    }

    if (s_marquee_exit_popup) {
        bool yes = (gpio_get_level(BTN_DOWN) == 0);
        bool no  = (gpio_get_level(BTN_UP) == 0);
        if (!yes && !no) s_marquee_exit_armed = false;
        if (s_marquee_exit_armed) { draw_marquee_frame(s_marquee_raw, s_scroll_offset, s_marquee_exit_popup); return; }

        int64_t now = esp_timer_get_time() / 1000;
        if (yes && (now - s_last_action_time >= 200)) {
            s_last_action_time = now;
            ESP_LOGI(TAG, "EXIT: Marquee -> MENU");
            s_marquee_exit_popup = false;
            s_marquee_need_init = true;
            audio_stop();
            if (s_marquee_raw) { heap_caps_free(s_marquee_raw); s_marquee_raw = nullptr; }
            s_current_state = STATE_MENU;
            draw_menu(s_menu_selection, s_menu_popup);
        } else if (no && (now - s_last_action_time >= 200)) {
            s_last_action_time = now;
            ESP_LOGI(TAG, "CANCEL");
            s_marquee_exit_popup = false;
            draw_marquee_frame(s_marquee_raw, s_scroll_offset, s_marquee_exit_popup);
        } else {
            draw_marquee_frame(s_marquee_raw, s_scroll_offset, s_marquee_exit_popup);
        }
        return;
    }

    if (btn == BTN_D) {
        ESP_LOGI(TAG, "POPUP: exit Marquee?");
        s_marquee_exit_popup = true;
        s_marquee_exit_armed = true;
        draw_marquee_frame(s_marquee_raw, s_scroll_offset, s_marquee_exit_popup);
        return;
    }

    s_scroll_offset = (s_scroll_offset + 2) % 500;
    draw_marquee_frame(s_marquee_raw, s_scroll_offset, s_marquee_exit_popup);
}

// ==================== GIF 播放器 Handler ====================

static void handle_gif(int btn)
{
    if (s_gif_need_init) {
        LGFX_Sprite *bb = display_backbuffer();
        bb->fillScreen(COLOR_BLACK);
        bb->setTextColor(COLOR_WHITE); bb->setTextSize(2);
        bb->setTextDatum(textdatum_t::middle_center);
        bb->drawString("Loading...", 160, 120);
        display_tft()->startWrite();
        bb->pushSprite(display_tft(), 0, 0);
        display_tft()->endWrite();

        ESP_LOGI(TAG, "INIT: GIF player (%d frames)", GIF_FRAME_COUNT);
        s_gif_exit_popup = false;
        s_gif_frame_idx = 0;
        s_gif_speed = 18;
        s_gif_last_frame_time = esp_timer_get_time() / 1000;
        if (!gif_frames_loaded()) load_gif_frames();
        s_gif_need_init = false;

        if (!audio_is_running()) audio_start();
        draw_gif_frame(s_gif_frame_idx, s_gif_speed, s_gif_exit_popup);
        return;
    }

    if (s_gif_exit_popup) {
        bool yes = (gpio_get_level(BTN_DOWN) == 0);
        bool no  = (gpio_get_level(BTN_UP) == 0);
        if (!yes && !no) s_gif_exit_armed = false;
        if (s_gif_exit_armed) { draw_gif_frame(s_gif_frame_idx, s_gif_speed, s_gif_exit_popup); return; }

        int64_t now = esp_timer_get_time() / 1000;
        if (yes && (now - s_last_action_time >= 200)) {
            s_last_action_time = now;
            ESP_LOGI(TAG, "EXIT: GIF -> MENU");
            s_gif_exit_popup = false;
            s_gif_need_init = true;
            audio_stop();
            free_gif_frames();
            s_current_state = STATE_MENU;
            draw_menu(s_menu_selection, s_menu_popup);
        } else if (no && (now - s_last_action_time >= 200)) {
            s_last_action_time = now;
            ESP_LOGI(TAG, "CANCEL");
            s_gif_exit_popup = false;
            draw_gif_frame(s_gif_frame_idx, s_gif_speed, s_gif_exit_popup);
        } else {
            draw_gif_frame(s_gif_frame_idx, s_gif_speed, s_gif_exit_popup);
        }
        return;
    }

    switch (btn) {
        case BTN_L:
            if (s_gif_speed > 1)  { s_gif_speed--; draw_gif_frame(s_gif_frame_idx, s_gif_speed, s_gif_exit_popup); }
            break;
        case BTN_R:
            if (s_gif_speed < 20) { s_gif_speed++; draw_gif_frame(s_gif_frame_idx, s_gif_speed, s_gif_exit_popup); }
            break;
        case BTN_D:
            ESP_LOGI(TAG, "POPUP: exit GIF?");
            s_gif_exit_popup = true;
            s_gif_exit_armed = true;
            draw_gif_frame(s_gif_frame_idx, s_gif_speed, s_gif_exit_popup);
            return;
    }

    int delay_ms = 500 - (s_gif_speed - 1) * 25;
    int64_t now = esp_timer_get_time() / 1000;
    if (now - s_gif_last_frame_time >= delay_ms) {
        s_gif_frame_idx = (s_gif_frame_idx + 1) % GIF_FRAME_COUNT;
        s_gif_last_frame_time = now;
        draw_gif_frame(s_gif_frame_idx, s_gif_speed, s_gif_exit_popup);
    }
}

// ==================== 主入口 ====================

extern "C" void app_main()
{
    ESP_LOGI(TAG, "===== box-demo Starting =====");

    display_init();
    audio_init();
    buttons_init();
    storage_init();

    draw_menu(s_menu_selection, s_menu_popup);

    while (1) {
        int btn = read_buttons();

        switch (s_current_state) {
            case STATE_MENU:
            case STATE_IMG:
                if (s_current_state == STATE_MENU) handle_menu(btn);
                else handle_img(btn);
                vTaskDelay(pdMS_TO_TICKS(10));
                break;

            case STATE_MARQUEE:
                handle_marquee(btn);
                vTaskDelay(pdMS_TO_TICKS(30));
                break;

            case STATE_GIF:
                handle_gif(btn);
                vTaskDelay(pdMS_TO_TICKS(10));
                break;
        }
    }
}
