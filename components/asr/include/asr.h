/*
 * SPDX-FileCopyrightText: 2025 box-demo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "network.h"

// 服务器地址（从 NVS 读取，可在 WiFi 配网时设置）
#define ASR_SERVER_URL wifi_get_asr_url()

// ==================== 上传与结果 ====================

bool asr_upload(const char *filepath);
bool asr_is_busy(void);
bool asr_result_ready(void);
const char *asr_get_result(void);

// ==================== 文本缓冲 ====================

void asr_text_append(const char *text);
const char *asr_text_get(void);
void asr_text_clear(void);
int  asr_text_len(void);

// ==================== UTF-8 光标操作（参数/返回值均为字节偏移）====================

int  asr_cursor_prev(int byte_pos);
int  asr_cursor_next(int byte_pos);
void asr_text_delete(int byte_pos);

#ifdef __cplusplus
}
#endif
