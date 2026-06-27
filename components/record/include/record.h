/*
 * SPDX-FileCopyrightText: 2025 box-demo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file record.h
 * @brief I2S1 麦克风录音 + I2S0 文件播放子系统
 *
 * 硬件：I2S1 RX → MEMS 麦克风 (BCLK=GPIO7, WS=GPIO15, DIN=GPIO1)
 *       GPIO2 → MAX98357A SD 静音控制
 *       文件播放借用 audio 组件的 I2S0 TX 通道
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 I2S1 RX 通道 + GPIO2 功放静音控制
 * @return true 成功
 */
bool record_init(void);

/**
 * @brief 开始录音到 WAV 文件（覆盖写入）
 * @param filepath 目标路径
 * @param max_sec  最大录音时长（秒）
 * @return true 成功启动
 */
bool record_start(const char *filepath, int max_sec);

/** @brief 停止录音 */
void record_stop(void);

/** @brief 查询是否正在录音 */
bool record_is_recording(void);

/** @brief 返回已录音秒数 */
int  record_time_elapsed(void);

/** @brief 查询麦克风是否已开始采集音频（I2S 已使能且过了稳定延迟） */
bool record_is_capturing(void);

/**
 * @brief 播放指定 WAV 文件一次（非循环，使用 I2S0 TX）
 * @param filepath WAV 文件路径
 * @return true 成功启动
 */
bool record_play_start(const char *filepath);

/** @brief 停止文件播放 */
void record_play_stop(void);

/** @brief 查询是否正在播放文件 */
bool record_is_playing(void);

#ifdef __cplusplus
}
#endif
