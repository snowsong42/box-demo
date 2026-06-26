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

/**
 * @brief 获取 I2S0 TX 通道句柄（供 record 组件播放录音文件）
 */
i2s_chan_handle_t audio_get_tx_handle(void);

#ifdef __cplusplus
}
#endif
