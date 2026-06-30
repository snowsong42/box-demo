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
 * @brief 绘制主菜单（7 项滚动列表）
 *
 * @param selection     当前选中项索引 (0..6)
 * @param scroll_offset 顶部可见项索引（滚动偏移）
 */
void draw_menu(int selection, int scroll_offset);

/**
 * @brief 绘制图片浏览器帧
 *
 * @param img_index 当前图片索引
 * @param img_count 图片总数
 * @param w_cache   宽缓存数组
 * @param h_cache   高缓存数组
 */
void draw_img_browser(int img_index, int img_count,
                      int *w_cache, int *h_cache);

/**
 * @brief 绘制走马灯帧
 *
 * @param raw_data     RGB565 图像数据（已加载到 PSRAM）
 * @param scroll_offset 当前滚动偏移
 */
void draw_marquee_frame(uint16_t *raw_data, int scroll_offset);

/**
 * @brief 绘制 GIF 帧
 *
 * @param frame_idx 当前帧索引（0~27）
 * @param gif_speed 播放速度（1~20）
 */
void draw_gif_frame(int frame_idx, int gif_speed);

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

// ==================== 网络测试页 UI ====================

/**
 * @brief 绘制顶部 Tab 导航栏
 *
 * @param active_tab 当前激活的 Tab 索引（0=图片, 1=走马灯, 2=GIF, 3=网络）
 */
void draw_tab_bar(int active_tab);

/**
 * @brief 绘制网络测试子菜单
 *
 * @param sub_selection 当前选中的子项（0=Ping, 1=HTTP, 2=TCP, 3=WiFi状态）
 * @param wifi_connected WiFi 是否已连接
 * @param wifi_ssid      已连接的 SSID（可为 NULL）
 * @param prov_mode      是否处于配网模式
 */
void draw_network_menu(int sub_selection, bool wifi_connected,
                       const char *wifi_ssid, bool prov_mode);

/**
 * @brief 绘制网络测试结果页（滚动文本视图）
 *
 * @param title        页面标题
 * @param body         结果文本（多行，换行符分隔）
 * @param scroll_offset 滚动偏移（行数）
 * @param max_lines    最大可见行数
 * @param running      测试是否仍在运行
 */
void draw_network_result(const char *title, const char *body,
                         int scroll_offset, int max_lines, bool running);

/**
 * @brief 绘制 WiFi 未连接提示页
 */
void draw_wifi_not_connected(void);

/**
 * @brief 绘制启动画面
 */
void draw_boot_screen(void);

/**
 * @brief 绘制 WiFi 状态大字体面板
 */
void draw_wifi_status_big(const char *ssid, const char *ip, int rssi, const char *mac, bool confirm);

/**
 * @brief 绘制 WiFi 配置引导页
 *
 * @param ap_active   AP 是否已启动
 * @param connected   WiFi 是否已连接
 * @param ip          连接后的 IP 地址（可为 NULL）
 */
void draw_wifi_config_page(int phase, bool connected, const char *ip,
                           bool check_done, bool baidu_ok,
                           bool asr_ok, bool chat_ok);

// ==================== 录音播放 UI ====================

/** @brief 绘制录音播放主界面（LEFT 录音 / RIGHT 播放 / BACK 返回） */
void draw_record_main(void);

/** @brief 绘制录音界面
 *  @param recording 是否正在录音
 *  @param time_left 剩余秒数（0=完毕）
 *  @param status    可选状态文本（如 "Stopping..."），NULL=隐藏 */
void draw_record_capture(bool recording, int time_left, const char *status);

/** @brief 绘制播放界面
 *  @param playing 是否正在播放
 *  @param finished 是否播放完毕 */
void draw_record_playback(bool playing, bool finished);

// ==================== ASR 语音识别 UI ====================

/** @brief 绘制 ASR 文本编辑器（自动滚动使光标可见）
 *  @param text        全部文本（UTF-8）
 *  @param cursor_byte 光标位置（字节偏移）
 *  @param scroll_line [in/out] 滚动行数，绘制后自动 clamp 到光标可见范围
 *  @param status      状态文本（如 "Recording...", "Uploading..."），NULL = 隐藏 */
void draw_asr_text(const char *text, int cursor_byte, int *scroll_line, const char *status);

/** @brief 绘制 ASR 录音准备中画面（麦克风初始化等待） */
void draw_asr_preparing(void);

// ==================== SD 卡 UI ====================

/** @brief 绘制 SD 卡状态页面
 *  @param mounted    是否已挂载
 *  @param name       卡名称
 *  @param fs_type    文件系统类型
 *  @param total_mb   总容量 (MB)
 *  @param free_mb    可用空间 (MB)
 *  @param write_kbps 写速度 (KB/s)，-1 表示未测
 *  @param read_kbps  读速度 (KB/s)，-1 表示未测
 *  @param testing    是否正在测速 */
void draw_sd_card_status(bool mounted, const char *name, const char *fs_type,
                         int total_mb, int free_mb,
                         int write_kbps, int read_kbps, bool testing);

/** @brief 绘制 SD 卡文件浏览器 */
void draw_sd_card_browse(const char *path, void *entries,
                         int count, int selection, int scroll);

// ==================== AI Chat UI ====================

/** @brief 绘制 AI Chat 对话页面
 *  @param text        对话历史文本
 *  @param cursor_byte 光标位置（字节偏移）
 *  @param scroll_line [in/out] 滚动行数
 *  @param status      状态文本（NULL=隐藏） */
void draw_chat(const char *text, int cursor_byte, int *scroll_line, const char *status);

#ifdef __cplusplus
}
#endif
