/*
 * SPDX-FileCopyrightText: 2025 box-demo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file buttons.h
 * @brief 按键 GPIO 初始化和边缘检测读取
 *
 * 4 键 INPUT_PULLUP，边缘检测（仅下沿触发），长按不重复。
 */

#pragma once

#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// 按键 GPIO
#define BTN_UP     GPIO_NUM_17
#define BTN_DOWN   GPIO_NUM_3
#define BTN_LEFT   GPIO_NUM_8
#define BTN_RIGHT  GPIO_NUM_18

// 按键返回值
#define BTN_NONE  0
#define BTN_U     1
#define BTN_D     2
#define BTN_L     3
#define BTN_R     4

/**
 * @brief 初始化 4 个按键 GPIO（上拉输入，无中断，轮询模式）
 */
void buttons_init(void);

/**
 * @brief 边缘检测按键读取
 *
 * 仅在"松→按"的下降沿返回事件，长按不重复触发。
 *
 * @return BTN_U / BTN_D / BTN_L / BTN_R 按下事件，BTN_NONE 无事件
 */
int read_buttons(void);

#ifdef __cplusplus
}
#endif
