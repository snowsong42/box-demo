/*
 * SPDX-FileCopyrightText: 2025 box-demo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file storage.h
 * @brief SPIFFS 文件系统挂载和文件工具函数
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 挂载 SPIFFS 分区（/spiffs）
 */
void storage_init(void);

/**
 * @brief 扫描 /spiffs/0001.png ~ 0099.png，统计可用图片数
 *
 * @return 找到的连续编号图片数量
 */
int detect_img_count(void);

/**
 * @brief 解析 PNG IHDR 头获取图像宽高
 *
 * @param path PNG 文件路径
 * @param w   [out] 图像宽度
 * @param h   [out] 图像高度
 * @return true 成功，false 失败（非 PNG 或读取错误）
 */
bool get_png_size(const char *path, int *w, int *h);

#ifdef __cplusplus
}
#endif
