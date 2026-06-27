/*
 * SPDX-FileCopyrightText: 2025 box-demo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file chat.h
 * @brief AI Chat 对话子系统 — HTTP POST /chat，获取 DeepSeek 回复 + TTS 音频
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 服务器地址（与 ASR 共用）
#define CHAT_SERVER_URL "http://192.168.5.63:8080/chat"

// ==================== 发送与结果 ====================

/** @brief 发送文本到 /chat（异步），返回 true 启动成功 */
bool chat_send(const char *text);

/** @brief 查询是否正在等待回复 */
bool chat_is_busy(void);

/** @brief 回复是否已到达 */
bool chat_result_ready(void);

/** @brief 获取 AI 回复文本 */
const char *chat_get_reply(void);

/** @brief 获取 TTS 音频数据（Base64 解码后的 WAV），返回长度；0=无音频 */
int chat_get_audio_len(void);

/** @brief 获取 TTS 音频数据指针 */
const uint8_t *chat_get_audio(void);

// ==================== 对话文本缓冲 ====================

void chat_text_append_user(const char *text);
void chat_text_append_ai(const char *text);
const char *chat_text_get(void);
void chat_text_clear(void);
int  chat_text_len(void);

// ==================== UTF-8 光标操作 ====================

int  chat_cursor_prev(int byte_pos);
int  chat_cursor_next(int byte_pos);
void chat_text_delete(int byte_pos);

#ifdef __cplusplus
}
#endif
