/*
 * SPDX-FileCopyrightText: 2025 box-demo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file network.cpp
 * @brief WiFi 配网 + Ping/HTTP/TCP 网络测试
 */

#include "network.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

static const char *TAG = "network";

// ==================== 常量 ====================
#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define NVS_NAMESPACE       "wifi_creds"
#define NVS_KEY_SSID        "ssid"
#define NVS_KEY_PASS        "pass"
#define PROV_AP_SSID        CONFIG_NETWORK_PROV_AP_SSID
#define PROV_AP_PASS        "12345678"
#define MAX_RETRY           5
#define RESULT_BUF_SIZE     2048

// ==================== WiFi 状态 ====================
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static bool s_connected = false;
static wifi_status_t s_status = {};

// ==================== 测试状态 ====================
static bool s_test_running = false;
static TaskHandle_t s_test_task = NULL;
static net_result_cb_t s_result_cb = NULL;

// ==================== 结果缓冲 ====================
static char s_result_buf[RESULT_BUF_SIZE];
static int s_result_len = 0;

// ==================== WiFi 事件处理 ====================

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "WiFi retry %d/%d", s_retry_num, MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            s_connected = false;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(s_status.ip, sizeof(s_status.ip), IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "WiFi connected. IP: %s", s_status.ip);
        s_retry_num = 0;
        s_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// ==================== 结果缓冲管理 ====================

static void result_clear(void)
{
    s_result_len = 0;
    s_result_buf[0] = '\0';
}

static void result_append(const char *line)
{
    int line_len = strlen(line);
    int remain = RESULT_BUF_SIZE - s_result_len - 2;
    if (remain <= 0) return;

    int copy_len = (line_len < remain) ? line_len : remain;
    memcpy(s_result_buf + s_result_len, line, copy_len);
    s_result_len += copy_len;
    s_result_buf[s_result_len] = '\n';
    s_result_len++;
    s_result_buf[s_result_len] = '\0';
}

static void result_callback(const char *line)
{
    result_append(line);
    if (s_result_cb) {
        s_result_cb(line);
    }
}

// ==================== WiFi 初始化 ====================

void wifi_init(void)
{
    ESP_LOGI(TAG, "WiFi init...");

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                        &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                        &wifi_event_handler, NULL, NULL));

    // 尝试从 NVS 读取凭据并连接
    nvs_handle_t nvs;
    char ssid[33] = {0};
    char pass[65] = {0};
    size_t len = sizeof(ssid);

    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK) {
        nvs_get_str(nvs, NVS_KEY_SSID, ssid, &len);
        len = sizeof(pass);
        nvs_get_str(nvs, NVS_KEY_PASS, pass, &len);
        nvs_close(nvs);
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    wifi_config_t wifi_cfg = {};

    if (strlen(ssid) > 0) {
        strncpy((char *)wifi_cfg.sta.ssid, ssid, 32);
        strncpy((char *)wifi_cfg.sta.password, pass, 64);
        ESP_LOGI(TAG, "Found saved WiFi: %s", ssid);
    } else {
        ESP_LOGW(TAG, "No saved WiFi credentials in NVS");
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 非阻塞 — 事件驱动连接，不在此等待
    ESP_LOGI(TAG, "WiFi connecting in background...");
}

void wifi_prov_start(void)
{
    ESP_LOGI(TAG, "Starting provisioning AP: %s", PROV_AP_SSID);

    // 停止 STA，切换到 AP+STA 模式
    esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(500));

    esp_wifi_set_mode(WIFI_MODE_APSTA);

    wifi_config_t ap_cfg;
    memset(&ap_cfg, 0, sizeof(ap_cfg));
    strncpy((char *)ap_cfg.ap.ssid, PROV_AP_SSID, 32);
    strncpy((char *)ap_cfg.ap.password, PROV_AP_PASS, 64);
    ap_cfg.ap.ssid_len = strlen(PROV_AP_SSID);
    ap_cfg.ap.channel = 1;
    ap_cfg.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    ap_cfg.ap.max_connection = 2;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Provisioning AP started: %s / %s", PROV_AP_SSID, PROV_AP_PASS);
}

bool wifi_is_connected(void)
{
    return s_connected;
}

void wifi_clear_credentials(void)
{
    s_connected = false;
    s_status.ip[0] = '\0';
    s_retry_num = MAX_RETRY;  // 阻止事件处理器自动重连
    esp_wifi_disconnect();
    esp_wifi_restore();
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG, "WiFi credentials cleared");
}

void wifi_get_status(wifi_status_t *status)
{
    if (!status) return;

    memset(status, 0, sizeof(wifi_status_t));

    if (s_connected) {
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            strncpy(status->ssid, (const char *)ap.ssid, 32);
            status->rssi = ap.rssi;
        }
        strncpy(status->ip, s_status.ip, 15);
    }

    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(status->mac, sizeof(status->mac),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    status->mac[17] = '\0';
}

// ==================== Ping / 连通性测试 ====================

static void ping_test_task(void *arg)
{
    char *host = (char *)arg;
    char line[128];

    snprintf(line, sizeof(line), "Conn test: %s ...", host);
    result_callback(line);

    for (int i = 0; i < 4 && s_test_running; i++) {
        int64_t start = esp_timer_get_time();

        struct addrinfo hints = {};
        struct addrinfo *res;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(host, "80", &hints, &res) != 0) {
            snprintf(line, sizeof(line), "  #%d: DNS fail", i + 1);
            result_callback(line);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sock < 0) {
            snprintf(line, sizeof(line), "  #%d: sock fail", i + 1);
            result_callback(line);
            freeaddrinfo(res);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        struct timeval tv = {.tv_sec = 2, .tv_usec = 0};
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        int ret = connect(sock, res->ai_addr, res->ai_addrlen);
        int64_t elapsed = esp_timer_get_time() - start;

        if (ret == 0) {
            snprintf(line, sizeof(line), "  #%d: OK  %lldms", i + 1, elapsed / 1000);
        } else {
            snprintf(line, sizeof(line), "  #%d: FAIL %lldms", i + 1, elapsed / 1000);
        }
        result_callback(line);

        close(sock);
        freeaddrinfo(res);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    snprintf(line, sizeof(line), "Connectivity test complete.");
    result_callback(line);
    free(host);              // 释放 strdup 的 host 副本
    s_test_task = NULL;
    s_test_running = false;
    vTaskDelete(NULL);
}

void ping_start(const char *host, int count, net_result_cb_t callback)
{
    if (s_test_running) return;

    result_clear();
    s_result_cb = callback;
    s_test_running = true;

    char *host_copy = strdup(host ? host : CONFIG_NETWORK_DEFAULT_HOST);
    xTaskCreate(ping_test_task, "ping_test", 4096, host_copy, 5, &s_test_task);
}

// ==================== HTTP GET 测试 ====================

static void http_test_task(void *arg)
{
    char *url = (char *)arg;
    char line[256];

    snprintf(line, sizeof(line), "HTTP GET %s", url);
    result_callback(line);

    esp_http_client_config_t cfg = {};
    cfg.url = url;
    cfg.timeout_ms = 5000;
    cfg.disable_auto_redirect = true;
    esp_http_client_handle_t client = esp_http_client_init(&cfg);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        int len = esp_http_client_get_content_length(client);
        snprintf(line, sizeof(line), "Status: %d  Content-Length: %d", status, len);
        result_callback(line);

        // 读取响应体前 512 字节
        char buf[513] = {0};
        int read = esp_http_client_read(client, buf, 512);
        if (read > 0) {
            buf[read] = '\0';
            // 截断每行显示
            char *saveptr;
            char *tok = strtok_r(buf, "\r\n", &saveptr);
            int lines = 0;
            while (tok && lines < 8) {
                if (strlen(tok) > 64) tok[64] = '\0';
                snprintf(line, sizeof(line), "  %s", tok);
                result_callback(line);
                tok = strtok_r(NULL, "\r\n", &saveptr);
                lines++;
            }
            if (tok) result_callback("  ... (truncated)");
        }
    } else {
        snprintf(line, sizeof(line), "HTTP request failed: %s", esp_err_to_name(err));
        result_callback(line);
    }

    esp_http_client_cleanup(client);
    result_callback("HTTP test complete.");
    free(url);               // 释放 strdup 的 url 副本
    s_test_task = NULL;
    s_test_running = false;
    vTaskDelete(NULL);
}

void http_get_start(const char *url, net_result_cb_t callback)
{
    if (s_test_running) return;

    result_clear();
    s_result_cb = callback;
    s_test_running = true;

    char *url_copy = strdup(url ? url : "http://httpbin.org/get");
    xTaskCreate(http_test_task, "http_test", 8192, url_copy, 5, &s_test_task);
}

// ==================== TCP 客户端测试 ====================

static void tcp_test_task(void *arg)
{
    char *params = (char *)arg;
    char host[64];
    int port;
    sscanf(params, "%63[^:]:%d", host, &port);
    free(params);

    char line[256];
    snprintf(line, sizeof(line), "TCP connect %s:%d ...", host, port);
    result_callback(line);

    struct addrinfo hints = {};
    struct addrinfo *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0) {
        snprintf(line, sizeof(line), "DNS resolve failed: %s", host);
        result_callback(line);
        s_test_task = NULL;
        s_test_running = false;
        vTaskDelete(NULL);
        return;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        result_callback("Socket create failed");
        freeaddrinfo(res);
        s_test_task = NULL;
        s_test_running = false;
        vTaskDelete(NULL);
        return;
    }

    // 设置超时
    struct timeval tv = {.tv_sec = 5, .tv_usec = 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        snprintf(line, sizeof(line), "Connect failed: %s:%d", host, port);
        result_callback(line);
        close(sock);
        freeaddrinfo(res);
        s_test_task = NULL;
        s_test_running = false;
        vTaskDelete(NULL);
        return;
    }
    freeaddrinfo(res);
    result_callback("Connected!");

    // 发送 HTTP 请求
    char send_data[128];
    snprintf(send_data, sizeof(send_data),
             "GET / HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", host);
    int sent = send(sock, send_data, strlen(send_data), 0);
    snprintf(line, sizeof(line), "Sent: %d bytes", sent);
    result_callback(line);

    // 接收响应
    char recv_buf[256];
    int recvd = recv(sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (recvd > 0) {
        recv_buf[recvd] = '\0';
        if (recvd > 80) recv_buf[80] = '\0';
        snprintf(line, sizeof(line), "Recv: %d bytes <- \"%.80s\"", recvd, recv_buf);
    } else {
        snprintf(line, sizeof(line), "Recv: timeout or closed");
    }
    result_callback(line);

    close(sock);
    result_callback("TCP test complete.");
    s_test_task = NULL;
    s_test_running = false;
    vTaskDelete(NULL);
}

void tcp_client_start(const char *host, int port, const char *data, net_result_cb_t callback)
{
    if (s_test_running) return;

    result_clear();
    s_result_cb = callback;
    s_test_running = true;

    char params[96];
    snprintf(params, sizeof(params), "%s:%d", host ? host : CONFIG_NETWORK_DEFAULT_HOST,
             port > 0 ? port : 80);
    xTaskCreate(tcp_test_task, "tcp_test", 4096, strdup(params), 5, &s_test_task);
}

// ==================== 测试控制 ====================

const char *network_get_results(void)
{
    return s_result_buf[0] ? s_result_buf : "";
}

void network_test_stop(void)
{
    s_test_running = false;             // 先设标志让任务自行退出
    int timeout = 50;
    while (s_test_task && timeout-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(10));  // 等待最多 500ms
    }
    if (s_test_task) {
        vTaskDelete(s_test_task);       // 最后手段才强杀
        s_test_task = NULL;
    }
}

bool network_test_running(void)
{
    return s_test_running;
}

// ==================== 网页配网服务 ====================

#include "esp_http_server.h"

static httpd_handle_t s_prov_server = NULL;
static volatile bool s_prov_done = false;

bool wifi_prov_is_done(void) { return s_prov_done; }

// 配网页 HTML（内嵌）
static const char PROV_HTML[] = R"raw(
<!DOCTYPE html><html><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>box-demo WiFi Setup</title>
<style>
body{font-family:Arial;background:#1a1a2e;color:#eee;margin:0;padding:20px}
h2{color:#00d4ff;text-align:center}
.card{background:#16213e;border-radius:10px;padding:16px;margin:12px 0}
label{display:block;margin:8px 0 4px;color:#aaa}
select,input{width:100%;padding:10px;border-radius:6px;border:none;background:#0f3460;color:#fff;font-size:16px;box-sizing:border-box}
button{width:100%;padding:12px;border:none;border-radius:8px;background:#00d4ff;color:#000;font-size:18px;font-weight:bold;margin-top:12px;cursor:pointer}
.btn2{background:#0f3460;color:#00d4ff}
#status{margin-top:12px;text-align:center;font-size:14px;color:#aaa}
</style></head><body>
<h2>box-demo WiFi Setup</h2>
<div class="card">
<div style="display:flex;align-items:center;justify-content:space-between;margin-bottom:6px">
<label style="margin:0;color:#aaa">WiFi Networks</label>
<button class="btn2" onclick="scan()" style="width:auto;padding:5px 14px;margin:0;font-size:13px">Scan</button>
</div>
<select id="ssid_list" onchange="document.getElementById('ssid').value=this.value">
<option value="">-- Press Scan --</option></select>
<label>or enter SSID</label>
<input id="ssid" placeholder="WiFi name">
<label>Password (AP: 12345678)</label>
<input id="pass" type="password" placeholder="WiFi password">
<button onclick="connect()">Connect</button>
<div id="status"></div>
</div>
<script>
async function scan(){
 document.getElementById('ssid_list').innerHTML='<option>Scanning...</option>';
 try{
  let r=await fetch('/scan');let d=await r.json();
  let s=document.getElementById('ssid_list');
  s.innerHTML=d.map(w=>`<option value="${w.ssid}">${w.ssid} (${w.rssi}dBm)</option>`).join('');
 }catch(e){document.getElementById('ssid_list').innerHTML='<option>Scan failed</option>';}
}
function connect(){
 let ssid=document.getElementById('ssid').value;
 let pass=document.getElementById('pass').value;
 if(!ssid){alert('Enter SSID');return;}
 document.getElementById('status').innerText='Connecting...';
 fetch('/connect',{method:'POST',headers:{'Content-Type':'application/json'},
  body:JSON.stringify({ssid:ssid,password:pass})})
 .then(r=>r.text()).then(t=>{document.getElementById('status').innerText=t;});
}
</script></body></html>
)raw";

/* GET / */
static esp_err_t prov_root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, PROV_HTML, strlen(PROV_HTML));
    return ESP_OK;
}

/* GET /scan — 返回 JSON 列表 [{ssid, rssi, auth}, ...] */
static esp_err_t prov_scan_handler(httpd_req_t *req)
{
    wifi_scan_config_t scan_cfg;
    memset(&scan_cfg, 0, sizeof(scan_cfg));
    scan_cfg.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_cfg, true));

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    wifi_ap_record_t *ap_records = (wifi_ap_record_t *)calloc(ap_count, sizeof(wifi_ap_record_t));
    esp_wifi_scan_get_ap_records(&ap_count, ap_records);

    // 按 SSID 去重，保留信号最强的
    int unique_count = 0;
    for (int i = 0; i < ap_count; i++) {
        bool dup = false;
        for (int j = 0; j < unique_count; j++) {
            if (strcmp((const char *)ap_records[i].ssid,
                       (const char *)ap_records[j].ssid) == 0) {
                // 同名：保留 RSSI 更高的
                if (ap_records[i].rssi > ap_records[j].rssi) {
                    ap_records[j] = ap_records[i];
                }
                dup = true;
                break;
            }
        }
        if (!dup && ap_records[i].ssid[0] != '\0') {
            if (unique_count != i) ap_records[unique_count] = ap_records[i];
            unique_count++;
        }
    }

    char *json = (char *)calloc(1, unique_count * 80 + 4);
    strcat(json, "[");
    for (int i = 0; i < unique_count && i < 16; i++) {
        char entry[96];
        snprintf(entry, sizeof(entry), "%s{\"ssid\":\"%s\",\"rssi\":%d,\"auth\":%d}",
                 i > 0 ? "," : "", ap_records[i].ssid, ap_records[i].rssi, ap_records[i].authmode);
        strcat(json, entry);
    }
    strcat(json, "]");

    free(ap_records);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    free(json);
    return ESP_OK;
}

/* POST /connect — Body: {"ssid":"xxx","password":"xxx"} */
static esp_err_t prov_connect_handler(httpd_req_t *req)
{
    char buf[256] = {0};
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_sendstr(req, "ERR: no data");
        return ESP_OK;
    }
    buf[len] = '\0';

    // 简单 JSON 解析：提取 "ssid":"..." 和 "password":"..."
    char ssid[33] = {0}, pass[65] = {0};
    char *sp = strstr(buf, "\"ssid\"");
    char *pp = strstr(buf, "\"password\"");
    if (sp) {
        sp = strchr(sp + 6, '"'); if (sp) sp++;
        char *end = strchr(sp, '"');
        if (sp && end && (end - sp) < 32) {
            memcpy(ssid, sp, end - sp);
        }
    }
    if (pp) {
        pp = strchr(pp + 10, '"'); if (pp) pp++;
        char *end = strchr(pp, '"');
        if (pp && end && (end - pp) < 64) {
            memcpy(pass, pp, end - pp);
        }
    }

    if (ssid[0] == '\0') {
        httpd_resp_sendstr(req, "ERR: no SSID");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Prov: saving WiFi %s", ssid);

    // 保存到 NVS
    nvs_handle_t nvs;
    nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    nvs_set_str(nvs, NVS_KEY_SSID, ssid);
    nvs_set_str(nvs, NVS_KEY_PASS, pass);
    nvs_commit(nvs);
    nvs_close(nvs);

    // 连接
    wifi_config_t wifi_cfg = {};
    strncpy((char *)wifi_cfg.sta.ssid, ssid, 32);
    strncpy((char *)wifi_cfg.sta.password, pass, 64);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_wifi_connect();

    httpd_resp_sendstr(req, "OK: Connecting...");
    s_prov_done = true;  // 通知主循环清理
    return ESP_OK;
}

void wifi_prov_web_start(void)
{
    ESP_LOGI(TAG, "Starting provisioning AP + web server...");
    s_prov_done = false;

    esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(300));

    esp_wifi_set_mode(WIFI_MODE_APSTA);

    wifi_config_t ap_cfg;
    memset(&ap_cfg, 0, sizeof(ap_cfg));
    strncpy((char *)ap_cfg.ap.ssid, "box-demo", 32);
    strncpy((char *)ap_cfg.ap.password, "12345678", 64);
    ap_cfg.ap.ssid_len = 8;
    ap_cfg.ap.channel = 1;
    ap_cfg.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    ap_cfg.ap.max_connection = 3;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 启动 HTTP Server
    httpd_config_t http_cfg = HTTPD_DEFAULT_CONFIG();
    http_cfg.server_port = 80;
    httpd_start(&s_prov_server, &http_cfg);

    httpd_uri_t root_uri = { .uri = "/",       .method = HTTP_GET,  .handler = prov_root_handler, .user_ctx = NULL };
    httpd_uri_t scan_uri = { .uri = "/scan",   .method = HTTP_GET,  .handler = prov_scan_handler, .user_ctx = NULL };
    httpd_uri_t conn_uri = { .uri = "/connect",.method = HTTP_POST, .handler = prov_connect_handler, .user_ctx = NULL };
    httpd_register_uri_handler(s_prov_server, &root_uri);
    httpd_register_uri_handler(s_prov_server, &scan_uri);
    httpd_register_uri_handler(s_prov_server, &conn_uri);

    ESP_LOGI(TAG, "Provisioning web server started on 192.168.4.1");
}

void wifi_prov_web_stop(void)
{
    if (s_prov_server) {
        httpd_stop(s_prov_server);
        s_prov_server = NULL;
    }
    ESP_LOGI(TAG, "Provisioning stopped");
}
