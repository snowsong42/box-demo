/*
 * SPDX-FileCopyrightText: 2025 box-demo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file sd_card.h
 * @brief MicroSD 卡驱动 (FSPI: CS=42, SCLK=40, MISO=39, MOSI=41)
 *
 * 挂载路径: /sdcard
 * 使用独立的 FSPI 总线，不与 TFT (SPI3) 共享
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief SD 卡状态信息 */
typedef struct {
    bool mounted;           // 是否成功挂载
    char name[16];          // 卡名称 (CID)
    char fs_type[8];        // 文件系统类型 (FAT32/FAT16)
    uint64_t total_bytes;   // 总容量 (字节)
    uint64_t free_bytes;    // 可用空间 (字节)
    int sector_size;        // 扇区大小
} sd_card_info_t;

/**
 * @brief 初始化 SD 卡并挂载 FAT 文件系统到 /sdcard
 *
 * 启动时调用，静默挂载（仅打日志）。未插卡或失败不阻塞系统。
 */
void sd_card_init(void);

/**
 * @brief 查询 SD 卡是否已挂载
 */
bool sd_card_mounted(void);

/**
 * @brief 获取 SD 卡详细信息
 */
void sd_card_get_info(sd_card_info_t *info);

/**
 * @brief SD 卡读写速度测试
 *
 * 创建临时文件 → 写入 512KB → 读取 → 删除，返回耗时(ms)。
 *
 * @param write_ms [out] 写入耗时 (ms)，-1 表示失败
 * @param read_ms  [out] 读取耗时 (ms)，-1 表示失败
 * @return true 测试成功
 */
bool sd_card_speed_test(int *write_ms, int *read_ms);

/** @brief 文件/目录条目 */
typedef struct {
    char name[64];
    bool is_dir;
    uint32_t size;
} sd_card_entry_t;

/** @brief 列出指定目录，返回条目数，-1=失败 */
int sd_card_list_dir(const char *path, sd_card_entry_t *entries, int max);

#ifdef __cplusplus
}
#endif
