/*
 * SPDX-FileCopyrightText: 2025 box-demo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "asr.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "asr";

// ==================== HTTP 上传状态 ====================
static TaskHandle_t s_upload_task = nullptr;
static bool s_busy = false;
static bool s_result_ready = false;
static char s_last_result[512] = {0};

// ==================== 文本缓冲 ====================
#define TEXT_BUF_SIZE 2048
static char s_text[TEXT_BUF_SIZE] = {0};

// ==================== UTF-8 工具 ====================

static int utf8_char_len(uint8_t c) {
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

static int utf8_str_len(const char *s) {
    int count = 0;
    while (*s) { count++; s += utf8_char_len((uint8_t)*s); }
    return count;
}

// ==================== HTTP 上传任务 ====================

static void upload_task(void *arg) {
    const char *path = (const char *)arg;
    char *data = nullptr;
    esp_http_client_handle_t client = nullptr;
    esp_http_client_config_t cfg = {};
    int fsize = 0;
    esp_err_t err = ESP_FAIL;
    s_result_ready = false;
    s_last_result[0] = '\0';

    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Upload: cannot open %s", path);
        goto done;
    }
    fseek(f, 0, SEEK_END);
    fsize = (int)ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 44) {
        ESP_LOGW(TAG, "Upload: file too small (%d bytes)", fsize);
        fclose(f);
        goto done;
    }
    data = (char *)malloc(fsize);
    if (!data) { fclose(f); goto done; }
    fread(data, 1, fsize, f);
    fclose(f);

    ESP_LOGI(TAG, "Uploading %d bytes to %s", fsize, ASR_SERVER_URL);

    cfg = (esp_http_client_config_t){};
    cfg.url = ASR_SERVER_URL;
    cfg.method = HTTP_METHOD_POST;
    cfg.timeout_ms = 15000;

    client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "Content-Type", "audio/wav");
    esp_http_client_set_post_field(client, data, fsize);

    err = esp_http_client_open(client, fsize);
    if (err == ESP_OK) {
        int wlen = esp_http_client_write(client, data, fsize);
        ESP_LOGI(TAG, "POST wrote %d/%d bytes", wlen, fsize);
        int clen = esp_http_client_fetch_headers(client);
        ESP_LOGI(TAG, "HTTP %d, content-length=%d",
                 esp_http_client_get_status_code(client), clen);

        char buf[512] = {0};
        int total = 0;
        while (total < clen && total < (int)sizeof(buf) - 1) {
            int rd = esp_http_client_read(client, buf + total, clen - total);
            if (rd <= 0) break;
            total += rd;
        }
        ESP_LOGI(TAG, "Read %d bytes: %s", total, buf[0] ? buf : "(empty)");
        const char *p = strstr(buf, "\"text\"");
        if (p) p = strchr(p, ':');
        if (p) { p++; while (*p == ' ' || *p == '"') p++; }
        if (p) {
            char *end = strchr((char *)p, '"');
            if (end) {
                int text_len = end - p;
                if (text_len >= sizeof(s_last_result)) text_len = sizeof(s_last_result) - 1;
                memcpy(s_last_result, p, text_len);
                s_last_result[text_len] = '\0';
                ESP_LOGI(TAG, "Parsed text: [%s]", s_last_result);
            }
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST failed: %d", err);
    }

    esp_http_client_cleanup(client);
    free(data);
    s_result_ready = true;

done:
    free((void*)path);       // 释放 strdup 的路径副本
    if (s_last_result[0] == '\0') {
        strcpy(s_last_result, "Server unreachable");
    }
    s_busy = false;
    s_upload_task = nullptr;
    vTaskDelete(NULL);
}

// ==================== 公开 API ====================

bool asr_upload(const char *filepath) {
    if (s_busy) return false;
    s_busy = true;
    char *path_copy = strdup(filepath);
    if (xTaskCreate(upload_task, "asr_up", 8192, path_copy, 5, &s_upload_task) != pdPASS) {
        free(path_copy);
        s_busy = false;
        return false;
    }
    return true;
}

bool asr_is_busy(void) { return s_busy; }
bool asr_result_ready(void) { return s_result_ready; }
const char *asr_get_result(void) { return s_last_result; }

// ==================== 文本缓冲 ====================

void asr_text_append(const char *text) {
    if (!text || !text[0]) return;
    int cur = strlen(s_text);
    int add = strlen(text);
    if (cur > 0 && s_text[cur - 1] != '\n') {
        // 非首次追加时加空格分隔
        if (cur + 2 < TEXT_BUF_SIZE) {
            s_text[cur] = ' ';
            cur++;
        }
    }
    for (int i = 0; i < add && cur < TEXT_BUF_SIZE - 1; i++) {
        s_text[cur++] = text[i];
    }
    s_text[cur] = '\0';
}

const char *asr_text_get(void) { return s_text; }

void asr_text_clear(void) { s_text[0] = '\0'; }

int asr_text_len(void) { return utf8_str_len(s_text); }

// ==================== UTF-8 光标 ====================

int asr_cursor_prev(int byte_pos) {
    if (byte_pos <= 0) return 0;
    // 向左跳过 UTF-8 续字节 (10xxxxxx)
    int p = byte_pos - 1;
    while (p > 0 && (s_text[p] & 0xC0) == 0x80) p--;
    return p;
}

int asr_cursor_next(int byte_pos) {
    if (byte_pos >= (int)strlen(s_text)) return byte_pos;
    return byte_pos + utf8_char_len((uint8_t)s_text[byte_pos]);
}

void asr_text_delete(int byte_pos) {
    int len = strlen(s_text);
    if (byte_pos <= 0 || byte_pos > len) return;
    int prev = asr_cursor_prev(byte_pos);
    memmove(s_text + prev, s_text + byte_pos, len - byte_pos + 1);
}
