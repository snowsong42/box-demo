/*
 * SPDX-FileCopyrightText: 2025 box-demo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file display.h
 * @brief 显示子系统：LGFX 实例 + 全帧后缓冲 + UI 绘制 + GIF 帧管理
 *
 * 所有绘制先进 backbuffer → 一次性 pushSprite → 无闪烁。
 */

#pragma once

#include "lgfx_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 颜色定义 (RGB565) ====================
#define COLOR_RED      0xF800
#define COLOR_GREEN    0x07E0
#define COLOR_BLUE     0x001F
#define COLOR_YELLOW   0xFFE0
#define COLOR_WHITE    0xFFFF
#define COLOR_BLACK    0x0000
#define COLOR_CYAN     0x07FF
#define COLOR_MAGENTA  0xF81F
#define COLOR_GRAY     0x8410

// GIF 参数
#define GIF_FRAME_COUNT 28
#define GIF_FRAME_W     200
#define GIF_FRAME_H     200

/**
 * @brief 初始化 TFT 和后缓冲（PSRAM 320×240）
 */
void display_init(void);

/**
 * @brief 获取 LGFX TFT 实例引用
 */
LGFX *display_tft(void);

/**
 * @brief 获取后缓冲 Sprite 引用
 */
LGFX_Sprite *display_backbuffer(void);

// ==================== UI 绘制函数 ====================

/**
 * @brief 绘制退出确认弹窗（子页面共用）
 */
void draw_exit_popup(void);

/**
 * @brief 绘制主菜单确认弹窗
 *
 * @param selection 当前选中的菜单项索引（0/1/2）
 * @param item_name 菜单项名称
 */
void draw_confirm_popup(int selection, const char *item_name);

/**
 * @brief 绘制主菜单
 *
 * @param selection 当前选中行
 * @param menu_popup 是否显示确认弹窗
 */
void draw_menu(int selection, bool menu_popup);

/**
 * @brief 绘制图片浏览器帧
 *
 * @param img_index 当前图片索引
 * @param img_count 图片总数
 * @param w_cache   宽缓存数组
 * @param h_cache   高缓存数组
 * @param exit_popup 是否显示退出弹窗
 */
void draw_img_browser(int img_index, int img_count,
                      int *w_cache, int *h_cache, bool exit_popup);

/**
 * @brief 绘制走马灯帧
 *
 * @param raw_data     RGB565 图像数据（已加载到 PSRAM）
 * @param scroll_offset 当前滚动偏移
 * @param exit_popup   是否显示退出弹窗
 */
void draw_marquee_frame(uint16_t *raw_data, int scroll_offset, bool exit_popup);

/**
 * @brief 绘制 GIF 帧
 *
 * @param frame_idx 当前帧索引（0~27）
 * @param gif_speed 播放速度（1~20）
 * @param exit_popup 是否显示退出弹窗
 */
void draw_gif_frame(int frame_idx, int gif_speed, bool exit_popup);

// ==================== GIF 帧管理 ====================

/**
 * @brief 从 SPIFFS 加载 28 帧 PNG 到 PSRAM Sprite 数组
 *
 * @return 成功加载的帧数
 */
int load_gif_frames(void);

/**
 * @brief 释放所有 GIF 帧 Sprite
 */
void free_gif_frames(void);

/**
 * @brief GIF 帧是否已加载
 */
bool gif_frames_loaded(void);

#ifdef __cplusplus
}
#endif
