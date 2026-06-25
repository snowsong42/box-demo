/*
 * SPDX-FileCopyrightText: 2025 box-demo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file network.h
 * @brief 网络子系统：WiFi 配网 + Ping/HTTP/TCP 测试
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==================== WiFi 状态结构 ====================

typedef struct {
    char ssid[33];    /**< 已连接的 WiFi SSID */
    char ip[16];      /**< IPv4 地址字符串 */
    int  rssi;        /**< 信号强度 dBm */
    char mac[18];     /**< MAC 地址字符串 */
} wifi_status_t;

// ==================== 回调类型 ====================

/** 测试结果回调：每次输出一行文本 */
typedef void (*net_result_cb_t)(const char *line);

// ==================== WiFi 管理 ====================

/**
 * @brief 初始化 WiFi（非阻塞）：注册事件，尝试连接
 *
 * 调用后立即返回。用 wifi_is_connected() 轮询连接状态。
 */
void wifi_init(void);

/**
 * @brief 启动 SoftAP 配网模式
 */
void wifi_prov_start(void);

/**
 * @brief 启动 SoftAP + HTTP 网页配网服务
 */
void wifi_prov_web_start(void);

/**
 * @brief 停止网页配网服务并关闭 AP
 */
void wifi_prov_web_stop(void);

/**
 * @brief 配网是否已完成（网页端提交了凭据）
 */
bool wifi_prov_is_done(void);

/**
 * @brief 检查 WiFi 是否已连接
 */
bool wifi_is_connected(void);

/**
 * @brief 获取当前 WiFi 连接状态
 */
void wifi_get_status(wifi_status_t *status);

// ==================== 网络测试 ====================

/** ... 同上 ... */

/**
 * @brief 获取测试结果缓冲（用于 UI 显示）
 *
 * @return 以换行符分隔的结果文本，如果没有结果返回 ""。
 */
const char *network_get_results(void);

/**
 * @brief 启动 Ping 测试（异步，结果通过回调输出）
 *
 * @param host     目标主机（如 "baidu.com"）
 * @param count    Ping 次数
 * @param callback 结果回调（可为 NULL）
 */
void ping_start(const char *host, int count, net_result_cb_t callback);

/**
 * @brief 启动 HTTP GET 测试（异步）
 *
 * @param url      目标 URL
 * @param callback 结果回调
 */
void http_get_start(const char *url, net_result_cb_t callback);

/**
 * @brief 启动 TCP 客户端测试（异步）
 *
 * @param host     服务器地址
 * @param port     端口
 * @param data     要发送的数据（NULL 则发默认测试包）
 * @param callback 结果回调
 */
void tcp_client_start(const char *host, int port, const char *data, net_result_cb_t callback);

/**
 * @brief 停止当前正在运行的测试
 */
void network_test_stop(void);

/**
 * @brief 是否有测试正在运行
 */
bool network_test_running(void);

#ifdef __cplusplus
}
#endif
