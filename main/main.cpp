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
#include "esp_http_client.h"
#include "lwip/netdb.h"
#include "nvs_flash.h"

#include "buttons.h"
#include "display.h"
#include "storage.h"
#include "audio.h"
#include "network.h"
#include "record.h"
#include "asr.h"
#include "chat.h"
#include "sd_card.h"

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
    STATE_ASR,
    STATE_SD_CARD,
    STATE_CHAT,
};
static AppState s_current_state = STATE_MENU;

// ==================== 菜单状态 ====================
#define MENU_ITEMS 12
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

// ==================== ASR 状态 ====================
static bool s_asr_need_init = true;
static int  s_asr_cursor = 0;
static int  s_asr_scroll = 0;
static int  s_asr_phase = 0;  // 0=idle 1=preparing 2=recording 3=uploading
static bool s_asr_stop_pending = false;
static int64_t s_asr_stop_req_us = 0;

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
static void handle_asr(int btn);
static void handle_sd_card(int btn);
static void handle_chat(int btn);
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
                case 9: s_current_state = STATE_ASR;       break;
                case 10: s_current_state = STATE_SD_CARD;  break;
                case 11: s_current_state = STATE_CHAT;    break;
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
            snprintf(path, sizeof(path), "/sdcard/img/%04d.png", i + 1);
            get_png_size(path, &s_img_w_cache[i], &s_img_h_cache[i]);
        }

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
        default:
            if (btn == BTN_NONE) return;  // 无按键不重绘
            break;
    }
    draw_img_browser(s_img_index, s_img_count, s_img_w_cache, s_img_h_cache);
}

// ==================== MARQUEE Handler ====================

static void handle_marquee(int btn)
{
    if (btn == BTN_B) {
        s_marquee_need_init = true;
        audio_stop();
        if (s_marquee_raw) { heap_caps_free(s_marquee_raw); s_marquee_raw = nullptr; }
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
            FILE *fp = fopen("/sdcard/500x150.raw", "rb");
            if (fp) {
                size_t sz = 500UL * 150 * 2;
                s_marquee_raw = (uint16_t *)heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
                if (s_marquee_raw) fread(s_marquee_raw, 1, sz, fp);
                fclose(fp);
            }
        }
        s_marquee_need_init = false;

        draw_marquee_frame(s_marquee_raw, s_scroll_offset);
        return;
    }

    if (btn == BTN_U || btn == BTN_D) {
        s_marquee_paused = !s_marquee_paused;
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
        free_gif_frames();
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
        if (btn == BTN_S) { s_current_state = STATE_WIFI_CONFIG; return; }
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
        if (btn == BTN_S) { s_current_state = STATE_WIFI_CONFIG; return; }
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
        if (btn == BTN_S) { s_current_state = STATE_WIFI_CONFIG; return; }
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
    FILE *f = fopen("/sdcard/rec.wav", "rb");
    if (f) { fclose(f); return; }
    f = fopen("/sdcard/rec.wav", "wb");
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
        draw_record_capture(false, 15, NULL);
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
        draw_record_capture(false, 0, NULL);
        return;
    }

    // START 且不在录音 → 开始录音
    if (btn == BTN_S && !s_record_capturing) {
        ensure_recording_file();
        if (record_start("/sdcard/rec.wav", 15)) {
            s_record_capturing = true;
            s_record_time_left = 15;
            draw_record_capture(true, 15, NULL);
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
        draw_record_capture(s_record_capturing, s_record_time_left, NULL);
        return;
    }

    // 完毕态 → START 重新录音
    if (btn == BTN_S) {
        if (record_start("/sdcard/rec.wav", 15)) {
            s_record_capturing = true;
            s_record_time_left = 15;
            draw_record_capture(true, 15, NULL);
        }
        return;
    }

    draw_record_capture(false, s_record_time_left, NULL);
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
        if (record_play_start("/sdcard/rec.wav")) {
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
        if (record_play_start("/sdcard/rec.wav")) {
            s_record_playing = true;
            s_record_play_done = false;
            draw_record_playback(true, false);
        }
        return;
    }

    draw_record_playback(false, s_record_play_done);
}

// ==================== ASR Handler ====================

static void handle_asr(int btn)
{
    if (s_asr_need_init) {
        s_asr_need_init = false;
        s_asr_phase = 0;
        s_asr_cursor = strlen(asr_text_get());
        draw_asr_text(asr_text_get(), s_asr_cursor, &s_asr_scroll, NULL);
        return;
    }

    // Phase 0: 空闲 — 编辑文本 / 按 START 开始录音
    if (s_asr_phase == 0) {
        if (btn == BTN_B) {
            s_asr_need_init = true;
            asr_text_clear();
            s_asr_cursor = 0;
            s_asr_scroll = 0;
            s_current_state = STATE_MENU;
            draw_menu(s_menu_sel, s_menu_scroll);
            return;
        }
        if (btn == BTN_S) {
            ensure_recording_file();
            if (record_start("/sdcard/rec.wav", 15)) {
                s_asr_phase = 1;  // 进入准备中
                draw_asr_preparing();
            }
            return;
        }
        if (btn == BTN_L) { s_asr_cursor = asr_cursor_prev(s_asr_cursor); }
        if (btn == BTN_R) { s_asr_cursor = asr_cursor_next(s_asr_cursor); }
        if (btn == BTN_D) { asr_text_delete(s_asr_cursor);
                            if (s_asr_cursor > 0) s_asr_cursor = asr_cursor_prev(s_asr_cursor); }
        draw_asr_text(asr_text_get(), s_asr_cursor, &s_asr_scroll, NULL);
        return;
    }

    // Phase 1: 准备中 — 等待麦克风初始化完成
    if (s_asr_phase == 1) {
        if (btn == BTN_B) {
            // 取消录音
            record_stop();
            s_asr_phase = 0;
            draw_asr_text(asr_text_get(), s_asr_cursor, &s_asr_scroll, NULL);
            return;
        }
        if (record_is_capturing()) {
            // 麦克风已就绪，进入倒计时录音
            s_asr_phase = 2;
            s_asr_stop_pending = false;
            draw_record_capture(true, 15, NULL);
        }
        return;
    }

    // Phase 2: 录音中 — 倒计时 + 300ms 缓冲停止
    if (s_asr_phase == 2) {
        // 用户请求停止 → 启动缓冲（不立即停，给 300ms 尾音时间）
        if (btn == BTN_B && !s_asr_stop_pending) {
            s_asr_stop_pending = true;
            s_asr_stop_req_us = esp_timer_get_time();
            int elapsed = record_time_elapsed();
            int left = 15 - elapsed;
            if (left < 0) left = 0;
            draw_record_capture(true, left, "Stopping...");
            return;
        }
        // 缓冲等待中 → 300ms 后真正停止
        if (s_asr_stop_pending) {
            if (esp_timer_get_time() - s_asr_stop_req_us >= 300000) {
                record_stop();
                vTaskDelay(pdMS_TO_TICKS(200));
                s_asr_stop_pending = false;
                s_asr_phase = 3;
                if (!asr_upload("/sdcard/rec.wav")) {
                    ESP_LOGW(TAG, "ASR upload failed");
                    s_asr_phase = 0;
                }
                draw_asr_text(asr_text_get(), s_asr_cursor, &s_asr_scroll,
                              s_asr_phase == 3 ? "Uploading..." : "Upload failed");
                return;
            }
            // 仍在缓冲，继续显示倒计时
            int elapsed = record_time_elapsed();
            int left = 15 - elapsed;
            if (left < 0) left = 0;
            draw_record_capture(true, left, "Stopping...");
            return;
        }
        // 自然超时 → 停止并上传
        if (!record_is_recording()) {
            record_stop();
            vTaskDelay(pdMS_TO_TICKS(200));
            s_asr_phase = 3;
            if (!asr_upload("/sdcard/rec.wav")) {
                ESP_LOGW(TAG, "ASR upload failed");
                s_asr_phase = 0;
            }
            draw_asr_text(asr_text_get(), s_asr_cursor, &s_asr_scroll,
                          s_asr_phase == 3 ? "Uploading..." : "Upload failed");
            return;
        }
        // 正常录音中 → 更新倒计时
        int elapsed = record_time_elapsed();
        int left = 15 - elapsed;
        if (left < 0) left = 0;
        draw_record_capture(true, left, NULL);
        return;
    }

    // Phase 3: 上传中 — 等待服务器返回
    if (s_asr_phase == 3) {
        if (btn == BTN_B) {
            s_asr_phase = 0;
            draw_asr_text(asr_text_get(), s_asr_cursor, &s_asr_scroll, NULL);
            return;
        }
        if (asr_result_ready()) {
            asr_text_append(asr_get_result());
            s_asr_cursor = strlen(asr_text_get());
            s_asr_scroll = 999;  // 自动滚到最新行（draw 里 clamp）
            s_asr_phase = 0;
            remove("/sdcard/rec.wav");  // 上传成功，删除录音释放空间
        }
        draw_asr_text(asr_text_get(), s_asr_cursor, &s_asr_scroll,
                      s_asr_phase == 3 ? "Uploading & recognizing..." : NULL);
        return;
    }
}

// ==================== SD 卡 Handler ====================

static bool s_sd_browsing = false;
static char s_sd_cur_path[128] = "/sdcard";
static sd_card_entry_t s_sd_entries[64];
static int  s_sd_entry_count = 0;
static int  s_sd_browse_sel = 0;
static int  s_sd_browse_scroll = 0;

static bool s_sd_testing = false;
static int  s_sd_write_kbps = -1;
static int  s_sd_read_kbps = -1;

static void handle_sd_card(int btn)
{
    if (btn == BTN_B) {
        if (s_sd_browsing) {
            s_sd_browsing = false;  // 退出浏览回状态页
        } else {
            s_sd_testing = false;
            s_current_state = STATE_MENU;
            draw_menu(s_menu_sel, s_menu_scroll);
            return;
        }
    }

    // === 浏览模式 ===
    if (s_sd_browsing) {
        if (btn == BTN_L) {
            if (strcmp(s_sd_cur_path, "/sdcard") != 0) {
                char *slash = strrchr(s_sd_cur_path, '/');
                if (slash) *slash = '\0';
                if (s_sd_cur_path[0] == '\0') strcpy(s_sd_cur_path, "/sdcard");
                s_sd_entry_count = sd_card_list_dir(s_sd_cur_path, s_sd_entries, 64);
                s_sd_browse_sel = 0; s_sd_browse_scroll = 0;
                ESP_LOGI(TAG, "SD browse: '%s' (%d entries)", s_sd_cur_path, s_sd_entry_count);
                for (int i = 0; i < s_sd_entry_count; i++) {
                    ESP_LOGI(TAG, "  %s %s  %u B",
                             s_sd_entries[i].is_dir ? "[D]" : "[F]",
                             s_sd_entries[i].name, (unsigned)s_sd_entries[i].size);
                }
            }
        } else if (btn == BTN_R && s_sd_entry_count > 0) {
            int idx = s_sd_browse_sel;
            if (idx < s_sd_entry_count && s_sd_entries[idx].is_dir) {
                char tmp[256];
                snprintf(tmp, sizeof(tmp), "%s/%s",
                         s_sd_cur_path, s_sd_entries[idx].name);
                strcpy(s_sd_cur_path, tmp);
                s_sd_entry_count = sd_card_list_dir(s_sd_cur_path, s_sd_entries, 64);
                s_sd_browse_sel = 0; s_sd_browse_scroll = 0;
                ESP_LOGI(TAG, "SD browse: '%s' (%d entries)", s_sd_cur_path, s_sd_entry_count);
                for (int i = 0; i < s_sd_entry_count; i++) {
                    ESP_LOGI(TAG, "  %s %s  %u B",
                             s_sd_entries[i].is_dir ? "[D]" : "[F]",
                             s_sd_entries[i].name, (unsigned)s_sd_entries[i].size);
                }
            }
        } else if (btn == BTN_U && s_sd_browse_sel > 0) {
            s_sd_browse_sel--;
        } else if (btn == BTN_D && s_sd_browse_sel < s_sd_entry_count - 1) {
            s_sd_browse_sel++;
        }
        if (s_sd_browse_sel < s_sd_browse_scroll) s_sd_browse_scroll = s_sd_browse_sel;
        if (s_sd_browse_sel >= s_sd_browse_scroll + 6) s_sd_browse_scroll = s_sd_browse_sel - 5;
        draw_sd_card_browse(s_sd_cur_path, s_sd_entries, s_sd_entry_count,
                            s_sd_browse_sel, s_sd_browse_scroll);
        return;
    }

    // === 状态页模式 ===
    if (btn == BTN_R) {
        s_sd_browsing = true;
        strcpy(s_sd_cur_path, "/sdcard");
        s_sd_entry_count = sd_card_list_dir(s_sd_cur_path, s_sd_entries, 64);
        s_sd_browse_sel = 0; s_sd_browse_scroll = 0;
        ESP_LOGI(TAG, "SD browse: '%s' (%d entries)", s_sd_cur_path, s_sd_entry_count);
        for (int i = 0; i < s_sd_entry_count; i++) {
            ESP_LOGI(TAG, "  %s %s  %u B",
                     s_sd_entries[i].is_dir ? "[D]" : "[F]",
                     s_sd_entries[i].name, (unsigned)s_sd_entries[i].size);
        }
    }

    if (btn == BTN_S && !s_sd_testing) {
        s_sd_testing = true; s_sd_write_kbps = -1; s_sd_read_kbps = -1;
        sd_card_info_t info; sd_card_get_info(&info);
        draw_sd_card_status(sd_card_mounted(), info.name, info.fs_type,
                            (int)(info.total_bytes/(1024*1024)),
                            (int)(info.free_bytes/(1024*1024)), -1, -1, true);
        int w_ms = -1, r_ms = -1;
        sd_card_speed_test(&w_ms, &r_ms);
        s_sd_testing = false;
        if (w_ms > 0) s_sd_write_kbps = (int)(512LL * 1000 / w_ms);
        if (r_ms > 0) s_sd_read_kbps  = (int)(512LL * 1000 / r_ms);
    }

    sd_card_info_t info; sd_card_get_info(&info);
    draw_sd_card_status(sd_card_mounted(), info.name, info.fs_type,
                        (int)(info.total_bytes/(1024*1024)),
                        (int)(info.free_bytes/(1024*1024)),
                        s_sd_write_kbps, s_sd_read_kbps, s_sd_testing);
}

// ==================== Chat Handler ====================

static bool s_chat_need_init = true;
static int  s_chat_cursor = 0;
static int  s_chat_scroll = 0;
static int  s_chat_phase = 0;  // 0=idle 1=preparing 2=recording 3=uploading 4=chatting

static void handle_chat(int btn)
{
    if (s_chat_need_init) {
        s_chat_need_init = false;
        s_chat_phase = 0;
        s_chat_cursor = strlen(chat_text_get());
        draw_chat(chat_text_get(), s_chat_cursor, &s_chat_scroll, NULL);
        return;
    }

    // Phase 0: 空闲 — 编辑 / START 录音 / UP 发送到 AI
    if (s_chat_phase == 0) {
        if (btn == BTN_B) {
            s_chat_need_init = true;
            chat_text_clear();
            s_chat_cursor = 0;
            s_chat_scroll = 0;
            s_current_state = STATE_MENU;
            draw_menu(s_menu_sel, s_menu_scroll);
            return;
        }
        if (btn == BTN_S) {
            ensure_recording_file();
            if (record_start("/sdcard/rec.wav", 15)) {
                s_chat_phase = 1;
                draw_asr_preparing();
            }
            return;
        }
        if (btn == BTN_U) {
            const char *all = chat_text_get();
            if (all[0]) {
                // 取最后一条用户输入的文本（跳过 "You: " 前缀）
                const char *last = strrchr(all, '\n');
                const char *msg = last ? last + 1 : all;
                if (strncmp(msg, "You: ", 5) == 0) msg += 5;
                else if (strncmp(msg, "AI: ", 4) == 0) msg = all;  // 只有 AI 回复，发全部
                s_chat_phase = 4;
                chat_send(msg);
                draw_chat(chat_text_get(), s_chat_cursor, &s_chat_scroll, "Waiting AI...");
            }
            return;
        }
        if (btn == BTN_L) { s_chat_cursor = chat_cursor_prev(s_chat_cursor); }
        if (btn == BTN_R) { s_chat_cursor = chat_cursor_next(s_chat_cursor); }
        if (btn == BTN_D) { chat_text_delete(s_chat_cursor);
                            if (s_chat_cursor > 0) s_chat_cursor = chat_cursor_prev(s_chat_cursor); }
        draw_chat(chat_text_get(), s_chat_cursor, &s_chat_scroll, NULL);
        return;
    }

    // Phase 1-3: 同 ASR（准备中 → 录音 → 上传），但上传后发到 /chat
    if (s_chat_phase == 1) {
        if (btn == BTN_B) { record_stop(); s_chat_phase = 0;
            draw_chat(chat_text_get(), s_chat_cursor, &s_chat_scroll, NULL); return; }
        if (record_is_capturing()) { s_chat_phase = 2; draw_record_capture(true, 15, NULL); }
        return;
    }

    if (s_chat_phase == 2) {
        static bool s_stop_pending = false;
        static int64_t s_stop_req_us = 0;
        if (btn == BTN_B && !s_stop_pending) {
            s_stop_pending = true; s_stop_req_us = esp_timer_get_time();
            int left = 15 - record_time_elapsed(); if (left < 0) left = 0;
            draw_record_capture(true, left, "Stopping..."); return;
        }
        if (s_stop_pending) {
            if (esp_timer_get_time() - s_stop_req_us >= 300000) {
                record_stop(); vTaskDelay(pdMS_TO_TICKS(200)); s_stop_pending = false;
                s_chat_phase = 3;
                if (!asr_upload("/sdcard/rec.wav")) { s_chat_phase = 0; }
                draw_chat(chat_text_get(), s_chat_cursor, &s_chat_scroll,
                          s_chat_phase == 3 ? "Recognizing..." : "Upload failed");
                return;
            }
            int left = 15 - record_time_elapsed(); if (left < 0) left = 0;
            draw_record_capture(true, left, "Stopping..."); return;
        }
        if (!record_is_recording()) {
            record_stop(); vTaskDelay(pdMS_TO_TICKS(200));
            s_chat_phase = 3;
            if (!asr_upload("/sdcard/rec.wav")) { s_chat_phase = 0; }
            draw_chat(chat_text_get(), s_chat_cursor, &s_chat_scroll,
                      s_chat_phase == 3 ? "Recognizing..." : "Upload failed");
            return;
        }
        int left = 15 - record_time_elapsed(); if (left < 0) left = 0;
        draw_record_capture(true, left, NULL); return;
    }

    // Phase 3: ASR 上传等待
    if (s_chat_phase == 3) {
        if (btn == BTN_B) { s_chat_phase = 0;
            draw_chat(chat_text_get(), s_chat_cursor, &s_chat_scroll, NULL); return; }
        if (asr_result_ready()) {
            chat_text_append_user(asr_get_result());
            s_chat_cursor = strlen(chat_text_get());
            s_chat_scroll = 999;
            remove("/sdcard/rec.wav");
            s_chat_phase = 0;  // 回空闲，让用户编辑/继续录音
        }
        draw_chat(chat_text_get(), s_chat_cursor, &s_chat_scroll,
                  s_chat_phase == 3 ? "Recognizing..." : "Waiting AI...");
        return;
    }

    // Phase 4: 等待 AI 回复
    if (s_chat_phase == 4) {
        if (btn == BTN_B) { s_chat_phase = 0;
            draw_chat(chat_text_get(), s_chat_cursor, &s_chat_scroll, NULL); return; }
        if (chat_result_ready()) {
            chat_text_append_ai(chat_get_reply());
            s_chat_cursor = strlen(chat_text_get());
            s_chat_scroll = 999;
            // 播放 AI 回复音频
            int alen = chat_get_audio_len();
            if (alen > 44) {
                FILE *af = fopen("/sdcard/chat_reply.wav", "wb");
                if (af) {
                    fwrite(chat_get_audio(), 1, alen, af);
                    fclose(af);
                    record_play_start("/sdcard/chat_reply.wav");
                }
                chat_free_audio();  // 用后即清
            }
            s_chat_phase = 0;
        }
        draw_chat(chat_text_get(), s_chat_cursor, &s_chat_scroll,
                  s_chat_phase == 4 ? "Waiting AI..." : NULL);
        return;
    }
}

// ==================== WiFi 状态 Handler ====================

static bool s_wifi_confirm = false;

static void handle_wifi_status(int btn)
{
    if (btn == BTN_B) {
        if (s_wifi_confirm) {
            s_wifi_confirm = false;  // 取消确认
        } else {
            s_current_state = STATE_MENU;
            draw_menu(s_menu_sel, s_menu_scroll);
            return;
        }
    }

    if (btn == BTN_S) {
        if (s_wifi_confirm) {
            wifi_clear_credentials();
            s_wifi_confirm = false;
        } else {
            s_wifi_confirm = true;
        }
    }

    wifi_status_t st;
    wifi_get_status(&st);
    draw_wifi_status_big(st.ssid, st.ip, st.rssi, st.mac, s_wifi_confirm);
}

// ==================== WiFi Config Handler ====================

static int s_wifi_cfg_phase = 0;
static bool s_wifi_cfg_first = true;

// 连通性检查结果
static bool s_net_check_done = false;
static bool s_net_baidu_ok = false;
static bool s_net_asr_ok  = false;
static bool s_net_chat_ok = false;

static void net_check_task(void *arg)
{
    // 等待 DHCP 分配 IP（WiFi 链路通 ≠ IP 到手）
    wifi_status_t st;
    for (int i = 0; i < 30; i++) {
        wifi_get_status(&st);
        if (st.ip[0] && st.ip[0] != '0') break;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGI(TAG, "NetCheck: IP=%s, starting checks", st.ip);

    // 1. 检查公网：DNS 解析 baidu.com（比 HTTP 更可靠，不会被 HTTPS 重定向坑）
    struct hostent *h = gethostbyname("baidu.com");
    s_net_baidu_ok = (h != NULL);
    ESP_LOGI(TAG, "NetCheck: baidu.com DNS %s", s_net_baidu_ok ? "OK" : "FAIL");

    // 2. 检查 ASR 服务器（禁用自动重定向，避免 HTTPS 陷阱）
    esp_http_client_config_t cfg = {};
    cfg.url = wifi_get_asr_url();
    cfg.timeout_ms = 3000;
    cfg.disable_auto_redirect = true;
    esp_http_client_handle_t cli = esp_http_client_init(&cfg);
    esp_http_client_set_method(cli, HTTP_METHOD_HEAD);
    esp_err_t e = esp_http_client_perform(cli);
    int status = esp_http_client_get_status_code(cli);
    s_net_asr_ok = (e == ESP_OK && status > 0);
    ESP_LOGI(TAG, "NetCheck: ASR %s (e=%d status=%d)", s_net_asr_ok ? "OK" : "FAIL", e, status);
    esp_http_client_cleanup(cli);

    // 3. 检查 Chat 服务器
    cfg.url = wifi_get_chat_url();
    cfg.timeout_ms = 3000;
    cfg.disable_auto_redirect = true;
    cli = esp_http_client_init(&cfg);
    esp_http_client_set_method(cli, HTTP_METHOD_HEAD);
    e = esp_http_client_perform(cli);
    status = esp_http_client_get_status_code(cli);
    s_net_chat_ok = (e == ESP_OK && status > 0);
    ESP_LOGI(TAG, "NetCheck: Chat %s (e=%d status=%d)", s_net_chat_ok ? "OK" : "FAIL", e, status);
    esp_http_client_cleanup(cli);

    s_net_check_done = true;
    vTaskDelete(NULL);
}

static void handle_wifi_config(int btn)
{
    if (s_wifi_cfg_phase == 1 && wifi_prov_is_done()) {
        wifi_prov_web_stop();
        s_wifi_cfg_phase = 2;
    }
    if (s_wifi_cfg_phase == 2 && wifi_is_connected()) {
        s_wifi_cfg_phase = 3;
        s_net_check_done = false;
        xTaskCreate(net_check_task, "netchk", 4096, NULL, 1, NULL);
    }
    if (s_wifi_cfg_first) {
        s_wifi_cfg_first = false;
        if (wifi_is_connected()) {
            s_wifi_cfg_phase = 3;
            s_net_check_done = false;
            xTaskCreate(net_check_task, "netchk", 4096, NULL, 1, NULL);
        }
    }

    if (btn == BTN_B) {
        if (s_wifi_cfg_phase == 1) wifi_prov_web_stop();
        s_wifi_cfg_phase = 0;
        s_wifi_cfg_first = true;
        s_net_check_done = false;
        s_current_state = STATE_MENU;
        draw_menu(s_menu_sel, s_menu_scroll);
        return;
    }

    if (btn == BTN_S) {
        if (s_wifi_cfg_phase == 0) {
            wifi_prov_web_start();
            s_wifi_cfg_phase = 1;
        } else if (s_wifi_cfg_phase == 3) {
            wifi_clear_credentials();
            wifi_prov_web_start();
            s_wifi_cfg_phase = 1;
        }
    }

    wifi_status_t st;
    wifi_get_status(&st);
    draw_wifi_config_page(s_wifi_cfg_phase, wifi_is_connected(),
                          st.ip[0] ? st.ip : NULL,
                          s_net_check_done, s_net_baidu_ok,
                          s_net_asr_ok, s_net_chat_ok);
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
            case STATE_ASR:
                handle_asr(btn);
                vTaskDelay(pdMS_TO_TICKS(10));
                break;
            case STATE_SD_CARD:
                handle_sd_card(btn);
                vTaskDelay(pdMS_TO_TICKS(20));
                break;
            case STATE_CHAT:
                handle_chat(btn);
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
