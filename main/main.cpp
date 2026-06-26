/*
 * SPDX-FileCopyrightText: 2025 box-demo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.cpp
 * @brief 应用入口 + 竖直菜单状态机
 *
 * 7 项菜单：IMG / Marquee / GIF / Ping / HTTP / TCP / WiFi
 * UP/DOWN 选择（带滚动），START 进入，BACK 返回。
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "buttons.h"
#include "display.h"
#include "storage.h"
#include "audio.h"
#include "network.h"
#include "record.h"

static const char *TAG = "main";

// ==================== 状态枚举 ====================
enum AppState {
    STATE_MENU,
    STATE_IMG,
    STATE_MARQUEE,
    STATE_GIF,
    STATE_RECORD,
    STATE_RECORD_CAPTURE,
    STATE_RECORD_PLAYBACK,
    STATE_PING,
    STATE_HTTP,
    STATE_TCP,
    STATE_WIFI_STATUS,
    STATE_WIFI_CONFIG,
};
static AppState s_current_state = STATE_MENU;

// ==================== 菜单状态 ====================
#define MENU_ITEMS 9
#define MENU_VISIBLE 5          // 可见行数
static int s_menu_sel = 0;      // 当前选中项 (0..6)
static int s_menu_scroll = 0;   // 滚动偏移（顶部显示的项索引）

// ==================== IMG 状态 ====================
static int s_img_index = 0;
static int s_img_count = 0;
static int s_img_w_cache[100];
static int s_img_h_cache[100];
static bool s_img_need_init = true;

// ==================== MARQUEE 状态 ====================
static bool s_marquee_need_init = true;
static uint16_t *s_marquee_raw = nullptr;
static int s_scroll_offset = 0;
static bool s_marquee_paused = false;

// ==================== GIF 状态 ====================
static bool s_gif_need_init = true;
static int s_gif_speed = 18;
static int s_gif_frame_idx = 0;
static int64_t s_gif_last_frame_time = 0;

// ==================== 录音播放状态 ====================
static bool s_record_need_init = true;
static bool s_record_capturing = false;
static bool s_record_playing = false;
static bool s_record_play_done = false;
static int  s_record_time_left = 15;

// ==================== 网络测试共用状态 ====================
static bool s_net_prov_mode = false;
static int s_net_scroll = 0;

// ==================== 函数声明 ====================
static void handle_menu(int btn);
static void handle_img(int btn);
static void handle_marquee(int btn);
static void handle_gif(int btn);
static void handle_ping(int btn);
static void handle_http(int btn);
static void handle_tcp(int btn);
static void handle_record(int btn);
static void handle_record_capture(int btn);
static void handle_record_playback(int btn);
static void handle_wifi_status(int btn);
static void handle_wifi_config(int btn);

// ==================== 主菜单 Handler ====================

/**
 * @brief 7 项滚动菜单：UP/DOWN 选，START 进入
 */
static void handle_menu(int btn)
{
    switch (btn) {
        case BTN_U:
            if (s_menu_sel > 0) {
                s_menu_sel--;
                if (s_menu_sel < s_menu_scroll) s_menu_scroll = s_menu_sel;
            }
            break;
        case BTN_D:
            if (s_menu_sel < MENU_ITEMS - 1) {
                s_menu_sel++;
                if (s_menu_sel >= s_menu_scroll + MENU_VISIBLE)
                    s_menu_scroll = s_menu_sel - MENU_VISIBLE + 1;
            }
            break;
        case BTN_S:
            ESP_LOGI(TAG, "Menu enter: %d", s_menu_sel);
            switch (s_menu_sel) {
                case 0: s_current_state = STATE_IMG;    break;
                case 1: s_current_state = STATE_MARQUEE; break;
                case 2: s_current_state = STATE_GIF;     break;
                case 3: s_current_state = STATE_RECORD;  break;
                case 4: s_current_state = STATE_PING;    break;
                case 5: s_current_state = STATE_HTTP;    break;
                case 6: s_current_state = STATE_TCP;     break;
                case 7: s_current_state = STATE_WIFI_STATUS; break;
                case 8: s_current_state = STATE_WIFI_CONFIG; break;
            }
            return;
        default: break;
    }
    draw_menu(s_menu_sel, s_menu_scroll);
}

// ==================== IMG Handler ====================

static void handle_img(int btn)
{
    if (btn == BTN_B) {
        s_img_need_init = true;
        audio_stop();
        s_current_state = STATE_MENU;
        draw_menu(s_menu_sel, s_menu_scroll);
        return;
    }

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
        s_img_index = 0;
        s_img_need_init = false;
        memset(s_img_w_cache, 0, sizeof(s_img_w_cache));
        memset(s_img_h_cache, 0, sizeof(s_img_h_cache));

        for (int i = 0; i < s_img_count; i++) {
            char path[32];
            snprintf(path, sizeof(path), "/spiffs/%04d.png", i + 1);
            get_png_size(path, &s_img_w_cache[i], &s_img_h_cache[i]);
        }

        if (!audio_is_running()) audio_start();
        draw_img_browser(s_img_index, s_img_count, s_img_w_cache, s_img_h_cache);
        return;
    }

    switch (btn) {
        case BTN_U:
            s_img_index = (s_img_index - 1 + s_img_count) % s_img_count;
            break;
        case BTN_D:
            s_img_index = (s_img_index + 1) % s_img_count;
            break;
        case BTN_S:
            if (audio_is_running()) audio_stop(); else audio_start();
            break;
        default: break;
    }
    draw_img_browser(s_img_index, s_img_count, s_img_w_cache, s_img_h_cache);
}

// ==================== MARQUEE Handler ====================

static void handle_marquee(int btn)
{
    if (btn == BTN_B) {
        s_marquee_need_init = true;
        audio_stop();
        s_current_state = STATE_MENU;
        draw_menu(s_menu_sel, s_menu_scroll);
        return;
    }

    if (s_marquee_need_init) {
        LGFX_Sprite *bb = display_backbuffer();
        bb->fillScreen(COLOR_BLACK);
        bb->setTextColor(COLOR_WHITE); bb->setTextSize(2);
        bb->setTextDatum(textdatum_t::middle_center);
        bb->drawString("Loading...", 160, 120);
        display_tft()->startWrite();
        bb->pushSprite(display_tft(), 0, 0);
        display_tft()->endWrite();

        s_scroll_offset = 0;
        s_marquee_paused = false;
        if (!s_marquee_raw) {
            FILE *fp = fopen("/spiffs/500x150.raw", "rb");
            if (fp) {
                size_t sz = 500UL * 150 * 2;
                s_marquee_raw = (uint16_t *)heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
                if (s_marquee_raw) fread(s_marquee_raw, 1, sz, fp);
                fclose(fp);
            }
        }
        s_marquee_need_init = false;

        if (!audio_is_running()) audio_start();
        draw_marquee_frame(s_marquee_raw, s_scroll_offset);
        return;
    }

    if (btn == BTN_U || btn == BTN_D) {
        s_marquee_paused = !s_marquee_paused;
    } else if (btn == BTN_S) {
        if (audio_is_running()) audio_stop(); else audio_start();
    }

    if (!s_marquee_paused) {
        s_scroll_offset = (s_scroll_offset + 2) % 500;
    }
    draw_marquee_frame(s_marquee_raw, s_scroll_offset);
}

// ==================== GIF Handler ====================

static void handle_gif(int btn)
{
    if (btn == BTN_B) {
        s_gif_need_init = true;
        audio_stop();
        s_current_state = STATE_MENU;
        draw_menu(s_menu_sel, s_menu_scroll);
        return;
    }

    if (s_gif_need_init) {
        LGFX_Sprite *bb = display_backbuffer();
        bb->fillScreen(COLOR_BLACK);
        bb->setTextColor(COLOR_WHITE); bb->setTextSize(2);
        bb->setTextDatum(textdatum_t::middle_center);
        bb->drawString("Loading...", 160, 120);
        display_tft()->startWrite();
        bb->pushSprite(display_tft(), 0, 0);
        display_tft()->endWrite();

        s_gif_frame_idx = 0;
        s_gif_speed = 18;
        s_gif_last_frame_time = esp_timer_get_time() / 1000;
        if (!gif_frames_loaded()) load_gif_frames();
        s_gif_need_init = false;

        if (!audio_is_running()) audio_start();
        draw_gif_frame(s_gif_frame_idx, s_gif_speed);
        return;
    }

    bool redraw = false;
    switch (btn) {
        case BTN_U: if (s_gif_speed < 20) { s_gif_speed++; redraw = true; } break;
        case BTN_D: if (s_gif_speed > 1)  { s_gif_speed--; redraw = true; } break;
        case BTN_S:
            if (audio_is_running()) audio_stop(); else audio_start();
            redraw = true;
            break;
        default: break;
    }
    if (redraw) {
        draw_gif_frame(s_gif_frame_idx, s_gif_speed);
        return;
    }

    int delay_ms = 500 - (s_gif_speed - 1) * 25;
    int64_t now = esp_timer_get_time() / 1000;
    if (now - s_gif_last_frame_time >= delay_ms) {
        s_gif_frame_idx = (s_gif_frame_idx + 1) % GIF_FRAME_COUNT;
        s_gif_last_frame_time = now;
        draw_gif_frame(s_gif_frame_idx, s_gif_speed);
    }
}

// ==================== Ping Handler ====================

static bool s_ping_started = false;

static void handle_ping(int btn)
{
    if (btn == BTN_B) {
        network_test_stop();
        s_ping_started = false;
        if (s_net_prov_mode) { wifi_prov_web_stop(); s_net_prov_mode = false; }
        s_current_state = STATE_MENU;
        draw_menu(s_menu_sel, s_menu_scroll);
        return;
    }

    if (!wifi_is_connected()) {
        if (btn == BTN_S) { s_net_prov_mode = true; wifi_prov_web_start(); }
        draw_wifi_not_connected();
        return;
    }

    switch (btn) {
        case BTN_S:
            if (network_test_running()) { network_test_stop(); vTaskDelay(pdMS_TO_TICKS(100)); }
            s_ping_started = false; s_net_scroll = 0;
            break;
        case BTN_U: if (s_net_scroll > 0) s_net_scroll--; break;
        case BTN_D: s_net_scroll++; break;
        default: break;
    }

    if (!s_ping_started && !network_test_running()) {
        ping_start(CONFIG_NETWORK_DEFAULT_HOST, 4, NULL);
        s_ping_started = true;
    }

    draw_network_result("Ping Test", network_get_results(),
                        s_net_scroll, 7, network_test_running());
}

// ==================== HTTP Handler ====================

static bool s_http_started = false;

static void handle_http(int btn)
{
    if (btn == BTN_B) {
        network_test_stop();
        s_http_started = false;
        if (s_net_prov_mode) { wifi_prov_web_stop(); s_net_prov_mode = false; }
        s_current_state = STATE_MENU;
        draw_menu(s_menu_sel, s_menu_scroll);
        return;
    }

    if (!wifi_is_connected()) {
        if (btn == BTN_S) { s_net_prov_mode = true; wifi_prov_web_start(); }
        draw_wifi_not_connected();
        return;
    }

    switch (btn) {
        case BTN_S:
            if (network_test_running()) { network_test_stop(); vTaskDelay(pdMS_TO_TICKS(100)); }
            s_http_started = false; s_net_scroll = 0;
            break;
        case BTN_U: if (s_net_scroll > 0) s_net_scroll--; break;
        case BTN_D: s_net_scroll++; break;
        default: break;
    }

    if (!s_http_started && !network_test_running()) {
        http_get_start("http://baidu.com", NULL);
        s_http_started = true;
    }

    draw_network_result("HTTP GET", network_get_results(),
                        s_net_scroll, 7, network_test_running());
}

// ==================== TCP Handler ====================

static bool s_tcp_started = false;

static void handle_tcp(int btn)
{
    if (btn == BTN_B) {
        network_test_stop();
        s_tcp_started = false;
        if (s_net_prov_mode) { wifi_prov_web_stop(); s_net_prov_mode = false; }
        s_current_state = STATE_MENU;
        draw_menu(s_menu_sel, s_menu_scroll);
        return;
    }

    if (!wifi_is_connected()) {
        if (btn == BTN_S) { s_net_prov_mode = true; wifi_prov_web_start(); }
        draw_wifi_not_connected();
        return;
    }

    switch (btn) {
        case BTN_S:
            if (network_test_running()) { network_test_stop(); vTaskDelay(pdMS_TO_TICKS(100)); }
            s_tcp_started = false; s_net_scroll = 0;
            break;
        case BTN_U: if (s_net_scroll > 0) s_net_scroll--; break;
        case BTN_D: s_net_scroll++; break;
        default: break;
    }

    if (!s_tcp_started && !network_test_running()) {
        tcp_client_start(CONFIG_NETWORK_DEFAULT_HOST, 80, NULL, NULL);
        s_tcp_started = true;
    }

    draw_network_result("TCP Client", network_get_results(),
                        s_net_scroll, 7, network_test_running());
}

// ==================== 录音播放 Handler ====================

static void ensure_recording_file(void)
{
    FILE *f = fopen("/spiffs/recording.wav", "rb");
    if (f) { fclose(f); return; }
    f = fopen("/spiffs/recording.wav", "wb");
    if (!f) return;
    uint32_t data_size = 0, riff_size = 36, fmt_size = 16;
    uint16_t fmt_tag = 1, channels = 1, block_align = 2, bits = 16;
    uint32_t sr = 22050, br = 44100, dc = 0x61746164;
    fwrite("RIFF",1,4,f); fwrite(&riff_size,4,1,f);
    fwrite("WAVE",1,4,f); fwrite("fmt ",1,4,f);
    fwrite(&fmt_size,4,1,f); fwrite(&fmt_tag,2,1,f);
    fwrite(&channels,2,1,f); fwrite(&sr,4,1,f);
    fwrite(&br,4,1,f); fwrite(&block_align,2,1,f);
    fwrite(&bits,2,1,f); fwrite(&dc,4,1,f);
    fwrite(&data_size,4,1,f);
    fclose(f);
}

static void handle_record(int btn)
{
    if (btn == BTN_B) {
        s_record_need_init = true;
        audio_stop();
        s_current_state = STATE_MENU;
        draw_menu(s_menu_sel, s_menu_scroll);
        return;
    }
    if (btn == BTN_L) {
        s_record_need_init = true;
        s_record_capturing = false;
        s_record_time_left = 15;
        s_current_state = STATE_RECORD_CAPTURE;
        return;
    }
    if (btn == BTN_R) {
        s_record_need_init = true;
        s_record_playing = false;
        s_record_play_done = false;
        s_current_state = STATE_RECORD_PLAYBACK;
        return;
    }
    draw_record_main();
}

static void handle_record_capture(int btn)
{
    // 首次进入
    if (s_record_need_init) {
        s_record_need_init = false;
        s_record_capturing = false;
        s_record_time_left = 15;
        ensure_recording_file();
        draw_record_capture(false, 15);
        return;
    }

    // BACK 且不在录音 → 回主界面
    if (btn == BTN_B && !s_record_capturing) {
        s_current_state = STATE_RECORD;
        draw_record_main();
        return;
    }

    // BACK 且在录音中 → 停止录音
    if (btn == BTN_B && s_record_capturing) {
        record_stop();
        s_record_capturing = false;
        s_record_time_left = 0;
        draw_record_capture(false, 0);
        return;
    }

    // START 且不在录音 → 开始录音
    if (btn == BTN_S && !s_record_capturing) {
        ensure_recording_file();
        if (record_start("/spiffs/recording.wav", 15)) {
            s_record_capturing = true;
            s_record_time_left = 15;
            draw_record_capture(true, 15);
        }
        return;
    }

    // 录音中 → 检测自然结束
    if (s_record_capturing) {
        if (!record_is_recording()) {
            s_record_capturing = false;
            s_record_time_left = 0;
        } else {
            int elapsed = record_time_elapsed();
            int left = 15 - elapsed;
            if (left < 0) left = 0;
            s_record_time_left = left;
        }
        draw_record_capture(s_record_capturing, s_record_time_left);
        return;
    }

    // 完毕态 → START 重新录音
    if (btn == BTN_S) {
        if (record_start("/spiffs/recording.wav", 15)) {
            s_record_capturing = true;
            s_record_time_left = 15;
            draw_record_capture(true, 15);
        }
        return;
    }

    draw_record_capture(false, s_record_time_left);
}

static void handle_record_playback(int btn)
{
    // 首次进入
    if (s_record_need_init) {
        s_record_need_init = false;
        s_record_playing = false;
        s_record_play_done = false;
        ensure_recording_file();
        draw_record_playback(false, false);
        return;
    }

    // BACK 且不在播放 → 回主界面
    if (btn == BTN_B && !s_record_playing) {
        s_current_state = STATE_RECORD;
        draw_record_main();
        return;
    }

    // BACK 且在播放中 → 停止播放
    if (btn == BTN_B && s_record_playing) {
        record_play_stop();
        s_record_playing = false;
        s_record_play_done = false;
        draw_record_playback(false, false);
        return;
    }

    // START 且不在播放 → 开始播放
    if (btn == BTN_S && !s_record_playing && !s_record_play_done) {
        audio_stop();
        if (record_play_start("/spiffs/recording.wav")) {
            s_record_playing = true;
            s_record_play_done = false;
            draw_record_playback(true, false);
        }
        return;
    }

    // 播放中 → 检测是否播完
    if (s_record_playing) {
        if (!record_is_playing()) {
            s_record_playing = false;
            s_record_play_done = true;
        }
        draw_record_playback(s_record_playing, s_record_play_done);
        return;
    }

    // 完毕态 → START 重新播放
    if (btn == BTN_S && s_record_play_done) {
        audio_stop();
        if (record_play_start("/spiffs/recording.wav")) {
            s_record_playing = true;
            s_record_play_done = false;
            draw_record_playback(true, false);
        }
        return;
    }

    draw_record_playback(false, s_record_play_done);
}

// ==================== WiFi 状态 Handler ====================

static void handle_wifi_status(int btn)
{
    if (btn == BTN_B) {
        s_current_state = STATE_MENU;
        draw_menu(s_menu_sel, s_menu_scroll);
        return;
    }

    wifi_status_t st;
    wifi_get_status(&st);
    draw_wifi_status_big(st.ssid, st.ip, st.rssi, st.mac);
}

// ==================== WiFi Config Handler ====================

static bool s_wifi_cfg_ap_active = false;

static void handle_wifi_config(int btn)
{
    // 配网完成（网页端提交了凭据）→ 清理 AP/HTTP Server
    if (wifi_prov_is_done() && s_wifi_cfg_ap_active) {
        wifi_prov_web_stop();
        s_wifi_cfg_ap_active = false;
    }

    if (btn == BTN_B) {
        if (s_wifi_cfg_ap_active) {
            wifi_prov_web_stop();
            s_wifi_cfg_ap_active = false;
        }
        s_current_state = STATE_MENU;
        draw_menu(s_menu_sel, s_menu_scroll);
        return;
    }

    if (btn == BTN_S && !s_wifi_cfg_ap_active) {
        wifi_prov_web_start();
        s_wifi_cfg_ap_active = true;
    }

    wifi_status_t st;
    wifi_get_status(&st);
    draw_wifi_config_page(s_wifi_cfg_ap_active, wifi_is_connected(),
                          st.ip[0] ? st.ip : NULL);
}

// ==================== 主入口 ====================

extern "C" void app_main()
{
    ESP_LOGI(TAG, "===== box-demo Starting =====");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    display_init();
    draw_boot_screen();       // ← 立即显示启动画面
    audio_init();
    record_init();
    buttons_init();
    storage_init();
    wifi_init();              // 非阻塞，后台连接

    draw_menu(s_menu_sel, s_menu_scroll);

    while (1) {
        int btn = read_buttons();

        switch (s_current_state) {
            case STATE_MENU:
                handle_menu(btn);
                vTaskDelay(pdMS_TO_TICKS(15));
                break;
            case STATE_IMG:
                handle_img(btn);
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
            case STATE_RECORD:
                handle_record(btn);
                vTaskDelay(pdMS_TO_TICKS(10));
                break;
            case STATE_RECORD_CAPTURE:
                handle_record_capture(btn);
                vTaskDelay(pdMS_TO_TICKS(10));
                break;
            case STATE_RECORD_PLAYBACK:
                handle_record_playback(btn);
                vTaskDelay(pdMS_TO_TICKS(10));
                break;
            case STATE_PING:
                handle_ping(btn);
                vTaskDelay(pdMS_TO_TICKS(20));
                break;
            case STATE_HTTP:
                handle_http(btn);
                vTaskDelay(pdMS_TO_TICKS(20));
                break;
            case STATE_TCP:
                handle_tcp(btn);
                vTaskDelay(pdMS_TO_TICKS(20));
                break;
            case STATE_WIFI_STATUS:
                handle_wifi_status(btn);
                vTaskDelay(pdMS_TO_TICKS(10));
                break;
            case STATE_WIFI_CONFIG:
                handle_wifi_config(btn);
                vTaskDelay(pdMS_TO_TICKS(10));
                break;
        }
    }
}
