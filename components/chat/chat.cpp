/*
 * SPDX-FileCopyrightText: 2025 box-demo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "chat.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "chat";

// ==================== HTTP 状态 ====================
static TaskHandle_t s_task = nullptr;
static bool s_busy = false;
static bool s_result_ready = false;
static char s_reply[1024] = {0};
static uint8_t *s_audio = nullptr;
static int s_audio_len = 0;

// ==================== 对话文本缓冲 ====================
#define TEXT_BUF_SIZE 4096
static char s_text[TEXT_BUF_SIZE] = {0};

// ==================== HTTP 任务 ====================

static int _b64_decode(const char *src, int src_len, uint8_t *dst, int dst_max);

static void chat_task(void *arg) {
    char *send_text = (char *)arg;
    char *data = nullptr;
    esp_http_client_handle_t client = nullptr;
    esp_http_client_config_t cfg = {};
    esp_err_t err = ESP_FAIL;
    s_result_ready = false;
    s_reply[0] = '\0';
    if (s_audio) { free(s_audio); s_audio = nullptr; }
    s_audio_len = 0;

    // 构造 JSON: {"text":"..."}（手动转义）
    char json_buf[1500];
    int jp = 0;
    jp += snprintf(json_buf + jp, sizeof(json_buf) - jp, "{\"text\":\"");
    for (const char *s = send_text; *s && jp < (int)sizeof(json_buf) - 4; s++) {
        if (*s == '"' || *s == '\\') json_buf[jp++] = '\\';
        json_buf[jp++] = *s;
    }
    jp += snprintf(json_buf + jp, sizeof(json_buf) - jp, "\"}");
    int jlen = jp;
    if (jlen >= (int)sizeof(json_buf)) {
        ESP_LOGE(TAG, "JSON too long");
        goto done;
    }

    ESP_LOGI(TAG, "Sending to %s: %s", CHAT_SERVER_URL, send_text);

    cfg = (esp_http_client_config_t){};
    cfg.url = CHAT_SERVER_URL;
    cfg.method = HTTP_METHOD_POST;
    cfg.timeout_ms = 20000;

    client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_buf, jlen);

    err = esp_http_client_open(client, jlen);
    if (err == ESP_OK) {
        int wlen = esp_http_client_write(client, json_buf, jlen);
        ESP_LOGI(TAG, "POST wrote %d/%d bytes", wlen, jlen);
        int clen = esp_http_client_fetch_headers(client);
        ESP_LOGI(TAG, "HTTP %d, content-length=%d",
                 esp_http_client_get_status_code(client), clen);

        if (clen > 0) {
            data = (char *)malloc(clen + 1);
            if (data) {
                int total = 0;
                while (total < clen) {
                    int rd = esp_http_client_read(client, data + total, clen - total);
                    if (rd <= 0) break;
                    total += rd;
                }
                data[total] = '\0';
                ESP_LOGI(TAG, "Read %d bytes: %.100s", total, data);

                // 解析 reply
                const char *p = strstr(data, "\"reply\"");
                if (p) p = strchr(p, ':');
                if (p) { p++; while (*p == ' ' || *p == '"') p++; }
                if (p) {
                    char *end = strchr((char *)p, '"');
                    if (end) {
                        int len = end - p;
                        if (len >= (int)sizeof(s_reply)) len = sizeof(s_reply) - 1;
                        memcpy(s_reply, p, len);
                        s_reply[len] = '\0';
                        ESP_LOGI(TAG, "Reply: [%s]", s_reply);
                    }
                }

                // 解析 audio_b64
                p = strstr(data, "\"audio_b64\"");
                if (p) p = strchr(p, ':');
                if (p) { p++; while (*p == ' ' || *p == '"') p++; }
                if (p && *p) {
                    char *end = strchr((char *)p, '"');
                    if (end) {
                        int b64len = end - p;
                        // Base64 解码 (简易实现，只处理标准字符)
                        s_audio = (uint8_t *)malloc(b64len);  // 足够大
                        if (s_audio) {
                            s_audio_len = _b64_decode(p, b64len, s_audio, b64len);
                            ESP_LOGI(TAG, "Audio: %d bytes (from %d b64)", s_audio_len, b64len);
                        }
                    }
                }

                free(data);
            }
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST failed: %d", err);
    }

    esp_http_client_cleanup(client);
    s_result_ready = true;

done:
    free(send_text);  // 释放 strdup 的文本
    if (s_reply[0] == '\0') {
        strcpy(s_reply, "Server unreachable");
    }
    s_busy = false;
    s_task = nullptr;
    vTaskDelete(NULL);
}

// ==================== Base64 解码 ====================

static const char B64_TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int _b64_decode(const char *src, int src_len, uint8_t *dst, int dst_max) {
    int out = 0;
    int val = 0, bits = 0;
    for (int i = 0; i < src_len && out < dst_max; i++) {
        char c = src[i];
        if (c == '=') break;
        const char *p = strchr(B64_TABLE, c);
        if (!p) continue;
        val = (val << 6) | (int)(p - B64_TABLE);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            dst[out++] = (val >> bits) & 0xFF;
        }
    }
    return out;
}

// ==================== 公开 API ====================

bool chat_send(const char *text) {
    if (s_busy || !text || !text[0]) return false;
    s_busy = true;
    char *copy = strdup(text);
    if (xTaskCreate(chat_task, "chat", 8192, copy, 5, &s_task) != pdPASS) {
        free(copy);
        s_busy = false;
        return false;
    }
    return true;
}

bool chat_is_busy(void) { return s_busy; }
bool chat_result_ready(void) { return s_result_ready; }
const char *chat_get_reply(void) { return s_reply; }

int chat_get_audio_len(void) { return s_audio_len; }
const uint8_t *chat_get_audio(void) { return s_audio; }

// ==================== 对话文本缓冲 ====================

void chat_text_append_user(const char *text) {
    if (!text || !text[0]) return;
    int cur = strlen(s_text);
    if (cur > 0) {
        if (cur + 2 < TEXT_BUF_SIZE) { s_text[cur] = '\n'; cur++; }
    }
    int add = strlen(text);
    const char *prefix = "You: ";
    int plen = strlen(prefix);
    if (cur + plen < TEXT_BUF_SIZE) {
        memcpy(s_text + cur, prefix, plen); cur += plen;
    }
    for (int i = 0; i < add && cur < TEXT_BUF_SIZE - 1; i++)
        s_text[cur++] = text[i];
    s_text[cur] = '\0';
}

void chat_text_append_ai(const char *text) {
    if (!text || !text[0]) return;
    int cur = strlen(s_text);
    if (cur > 0) {
        if (cur + 2 < TEXT_BUF_SIZE) { s_text[cur] = '\n'; cur++; }
    }
    int add = strlen(text);
    const char *prefix = "AI: ";
    int plen = strlen(prefix);
    if (cur + plen < TEXT_BUF_SIZE) {
        memcpy(s_text + cur, prefix, plen); cur += plen;
    }
    for (int i = 0; i < add && cur < TEXT_BUF_SIZE - 1; i++)
        s_text[cur++] = text[i];
    s_text[cur] = '\0';
}

const char *chat_text_get(void) { return s_text; }

void chat_text_clear(void) { s_text[0] = '\0'; }

// ==================== UTF-8 工具 + 光标 ====================

static int _utf8_char_len(uint8_t c) {
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

int chat_text_len(void) {
    int count = 0;
    const char *p = s_text;
    while (*p) { count++; p += _utf8_char_len((uint8_t)*p); }
    return count;
}

int chat_cursor_prev(int byte_pos) {
    if (byte_pos <= 0) return 0;
    int p = byte_pos - 1;
    while (p > 0 && (s_text[p] & 0xC0) == 0x80) p--;
    return p;
}

int chat_cursor_next(int byte_pos) {
    if (byte_pos >= (int)strlen(s_text)) return byte_pos;
    return byte_pos + _utf8_char_len((uint8_t)s_text[byte_pos]);
}

void chat_text_delete(int byte_pos) {
    int len = strlen(s_text);
    if (byte_pos <= 0 || byte_pos > len) return;
    int prev = chat_cursor_prev(byte_pos);
    memmove(s_text + prev, s_text + byte_pos, len - byte_pos + 1);
}
