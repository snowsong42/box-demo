/*
 * SPDX-FileCopyrightText: 2025 box-demo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file audio.h
 * @brief I2S 音频播放子系统（MAX98357A，22050Hz 16-bit Mono）
 */

#pragma once

#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 I2S0 标准模式 TX 通道
 *
 * GPIO: BCLK=5, LRCLK=4, DOUT=6
 */
void audio_init(void);

/**
 * @brief 启动背景音乐循环播放任务
 *
 * 循环播放 /spiffs/music.wav (PCM 16bit 22050Hz Mono)。
 * 任务在 audio_running 为 false 时退出。
 */
void audio_start(void);

/**
 * @brief 停止音频播放并释放任务
 */
void audio_stop(void);

/**
 * @brief 反初始化 I2S 通道
 */
void audio_deinit(void);

/**
 * @brief 获取播放状态
 *
 * @return true 正在播放
 */
bool audio_is_running(void);

// ==================== 录音 (RX) ====================

/**
 * @brief 开始录音到 WAV 文件（覆盖写入）
 * @param filepath  目标文件路径（如 "/spiffs/recording.wav"）
 * @param max_sec   最大录音时长（秒），到时自动停止
 * @return true 成功启动，false 正在录音/播放/循环中
 */
bool audio_record_start(const char *filepath, int max_sec);

/** @brief 停止录音并写入正确的 WAV 头部信息 */
void audio_record_stop(void);

/** @brief 查询是否正在录音 */
bool audio_is_recording(void);

/** @brief 返回已录音秒数（用于 UI 倒计时显示） */
int  audio_record_time_elapsed(void);

// ==================== 文件播放 (TX, 单次) ====================

/**
 * @brief 播放指定 WAV 文件一次（非循环）
 * @param filepath  WAV 文件路径
 * @return true 成功启动，false 正在播放/录音/循环中
 */
bool audio_play_file_start(const char *filepath);

/** @brief 停止文件播放 */
void audio_play_file_stop(void);

/** @brief 查询是否正在播放文件 */
bool audio_is_playing_file(void);

#ifdef __cplusplus
}
#endif
