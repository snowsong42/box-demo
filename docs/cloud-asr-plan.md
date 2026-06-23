# 方案 B2：WiFi 云端语音识别方案

> 方案 B2 属于前面分析中的**云端识别路线**：I2S MEMS 麦克风 → ESP32-S3 录制 → WiFi 上传音频 → 云端 ASR 返回文字。

---

## 0. 与方案 A 的对比回顾

| 维度 | 方案 A (SU-03T 离线模块) | 方案 B2 (WiFi 云端 ASR) |
|------|:---:|:---:|
| 识别方式 | 固定词条 (100~300 条) | **自由语音**，不限词条 |
| 识别精度 | 中等 (~90%) | **高 (≥97%)** |
| 延迟 | <100ms | 1~3 秒 |
| WiFi 依赖 | 不需要 | **必须** |
| CPU 占用 | ≈0% | **5~10%** (录音 + 网络) |
| 成本 | ¥10~20 一次性 | ¥0 + API 按量计费 |
| 适用场景 | 命令控制、唤醒词 | **自由对话、实时转写、会议记录** |

> 如果需求是**固定命令**（"打开灯"、"下一首"），选方案 A。如果需要**自由说话转文字**（语音输入、聊天、搜索），选方案 B2。

---

## 1. 硬件方案

### 1.1 推荐麦克风：INMP441

| 参数 | 值 |
|------|-----|
| 型号 | INMP441 (MEMS 全向) |
| 接口 | I2S PDM 数字输出 |
| 信噪比 | 61 dBA |
| 灵敏度 | -26 dBFS |
| 频率响应 | 60 Hz ~ 15 kHz |
| 工作电压 | 3.3V |
| 成本 | ¥8~15 |
| 可用 GPIO | 仅需 **2 根** (CLK + DIN) |

### 1.2 接线方案

```
ESP32-S3 (I2S1)               INMP441 模块
┌──────────────┐             ┌──────────────┐
│ GPIO7 (CLK)  ├─────────────┤ SCK          │
│ GPIO9 (DIN)  ├─────────────┤ SD (Data)    │
│ 3.3V         ├─────────────┤ VDD          │
│ GND          ├─────────────┤ GND + L/R    │
└──────────────┘             └──────────────┘

ESP32-S3 I2S 资源分配:
┌─────────────────────────────────────────────┐
│ I2S0 (TX) → MAX98357A (游戏音频输出)  ★已占 │
│ I2S1 (RX) → INMP441   (语音输入)     ★新增 │
│                                             │
│ ESP32-S3 支持 I2S1 PDM RX + 硬件            │
│ PDM-to-PCM 滤波器，无需软件转换              │
└─────────────────────────────────────────────┘
```

> ⚠️ GPIO7 和 GPIO9 在 DIJI-NES 接线中空闲，与现有外设无冲突。

### 1.3 INMP441 立体声配置

INMP441 的 L/R 引脚决定声道：
- **L/R = GND** → 左声道（数据在 WS=0 时有效）
- **L/R = VDD** → 右声道（数据在 WS=1 时有效）

推荐 L/R 接 GND，使用左声道模式。

---

## 2. 音频参数标准

主流云 ASR API 共同要求的 PCM 格式：

| 参数 | 标准值 | 说明 |
|------|--------|------|
| 采样率 | **16000 Hz** | 百度/讯飞/Google 均支持 |
| 位深 | **16-bit** | PCM signed 16-bit LE |
| 声道 | **1 (Mono)** | 单声道即可 |
| 编码 | **PCM raw** | 不压缩，或可选 opus |
| 帧大小 | 3200 字节 | 100ms = 16000×2×0.1 |
| 网络格式 | WebSocket 或 HTTP POST | 流式传输 |

---

## 3. 云端 API 选型

### 3.1 三家主流平台对比

| 维度 | 百度语音识别 | 火山引擎（豆包语音） | 讯飞语音 |
|------|:----------:|:-------------:|:-------:|
| 免费额度 | 短语音 5 万次/天 | 首月免费/试用额度 | 500 次/天 |
| 付费价格 | ¥0.0034/次 (~¥3/千次) | ¥0.003/次 (~¥3/千次) | ¥0.0033/次 |
| 协议 | REST + **WebSocket 流式** | WebSocket 二进制协议 | WebSocket |
| 实时流式 | ✅ 实时返回中间结果 | ✅ 实时分句 + 逐词时间戳 | ✅ 实时返回 |
| 方言支持 | ✅ 粤语/四川话等 | ✅ | ✅ |
| 热词 | ✅ | ✅ 自学习平台 | ✅ |
| 标点预测 | ✅ | ✅ (workflow 可配置) | ✅ |
| ITN(反文本正则) | ✅ | ✅ | ✅ |
| 接入难度 | 中等 | 中等 | 中等 |
| 文档质量 | ★★★★☆ | ★★★★☆ | ★★★☆☆ |

### 3.2 推荐：百度语音识别（REST API 简化版）

对于 ESP32 最友好的是**百度短语音识别 REST API**，原因：

1. **最简单**：HTTP POST 上传音频 → 返回 JSON，不需要 WebSocket
2. **60 秒限制**：对对话场景足够
3. **免费额度大**：5 万次/天，个人项目基本不花钱
4. **ESP-IDF 只需 HTTP Client**：不需要 WebSocket 库

```
POST https://vop.baidu.com/server_api
Content-Type: application/json

{
    "format": "pcm",
    "rate": 16000,
    "channel": 1,
    "cuid": "ESP32-S3-XXXX",
    "token": "24.xxxxxx",  // 通过 API Key + Secret Key 获取
    "speech": "<base64 encoded PCM data>",
    "len": 32000            // PCM 字节数
}

响应:
{
    "err_no": 0,
    "err_msg": "success.",
    "corpus_no": "123456",
    "sn": "xxx",
    "result": ["这是语音识别结果文本"]
}
```

### 3.3 进阶：百度实时语音识别 WebSocket

如果需要**边说边识别**（实时转写），使用百度 WebSocket 流式 API：

```
wss://vop.baidu.com/realtime_asr

第1帧(JSON): {"type": "START", "data": {"format":"pcm","rate":16000,...}}
第2~N帧(二进制): PCM 音频数据 (每帧建议 100~200ms)
最后一帧(JSON): {"type": "FINISH"}
服务端每帧回复: {"type":"MID_TEXT","result":"中间识别结果..."}
最终回复: {"type":"FIN_TEXT","result":"最终识别结果"}
```

### 3.4 火山引擎（豆包/字节跳动）WebSocket 协议

> 来源：[火山引擎官方文档](https://www.volcengine.com/docs/6561/80818)

```
WebSocket URL: wss://openspeech.bytedance.com/api/v2/asr

消息流程:
┌──────────┐                         ┌──────────┐
│ ESP32-S3 │ ── Full Client Request──→│  火山ASR  │
│          │ ←── Full Server Response │          │
│          │ ── Audio Only (pkt 1)──→│          │
│          │ ←── Partial Result     │          │
│          │ ── Audio Only (pkt 2)──→│          │
│          │ ←── Partial Result     │          │
│          │ ── Audio Only (END) ──→│          │
│          │ ←── Final Result       │          │
└──────────┘                         └──────────┘
```

火山引擎支持**逐词时间戳**、**分句结果**、**中间实时结果**、**Gzip 压缩**，适合高性能场景。

---

## 4. ESP32-S3 固件架构

### 4.1 整体数据流

```
┌─────────────────────────────────────────────────────────┐
│                    ESP32-S3                              │
│                                                          │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐           │
│  │ I2S1 PDM │───→│ PDM→PCM  │───→│ Ring     │           │
│  │ (INMP441)│    │ Hardware │    │ Buffer   │           │
│  └──────────┘    └──────────┘    └────┬─────┘           │
│                                       │                  │
│  ┌──────────────────────────────────┐ │                  │
│  │ DIJI-NES 游戏任务                 │ │ FreeRTOS Task    │
│  │ Core0: display_task (DMA 推屏)    │ │ 不影响           │
│  │ Core1: loop (模拟器+PPU)          │ │                  │
│  └──────────────────────────────────┘ │                  │
│                                       ↓                  │
│  ┌──────────────────────────────────────┐               │
│  │ voice_recognition_task (Core 0 空闲) │               │
│  │                                      │               │
│  │ 1. 从 Ring Buffer 读取 1~2 秒 PCM   │               │
│  │ 2. Base64 编码 (约 1.33×膨胀)       │               │
│  │ 3. HTTP POST → 百度 ASR API         │               │
│  │ 4. 解析 JSON → 得到文字             │               │
│  └──────────────────────────────────────┘               │
│                                       │                  │
│  ┌──────────────────────────────────────┐               │
│  │ WiFi 栈 (内置)                       │               │
│  └──────────────────────────────────────┘               │
└─────────────────────────────────────────────────────────┘
```

### 4.2 任务调度策略

```
CPU0 (协议核):  display_task (DMA) + voice_recognition_task
CPU1 (应用核):  NES 模拟主循环

关键：voice_recognition_task 在 Core 0 运行，优先级低于 display_task
display_task 每帧 ~3ms DMA 后主动 vTaskDelay(2)，给 voice 留窗口
```

**对游戏帧率的影响**：
- I2S DMA 录音：**零 CPU 开销**（硬件 PDM→PCM + DMA）
- HTTP 上传：占用约 2~5% CPU（主要在等待网络 I/O）
- Base64 编码：约 1~3ms CPU 时间/秒音频
- **预计帧率下降 <1 FPS**（57~61 → 56~60），基本不影响

### 4.3 内存估算

| 组件 | 大小 | 说明 |
|------|------|------|
| I2S DMA 缓冲 | ~8 KB | 4 个 buffer × 256 samples × 2 bytes |
| Ring Buffer (PCM) | ~64 KB | 缓存 2 秒音频 (16000×2×2) |
| HTTP 请求缓冲 | ~128 KB | Base64 编码后 + JSON header |
| WiFi 栈 | ~60 KB | 已计入系统 |
| SSL/TLS | ~40 KB | HTTPS 需要 |
| **总计增加** | **~300 KB** | PSRAM 充足 (8MB 还剩 ~7.3MB) |

---

## 5. 核心代码框架 (ESP-IDF)

### 5.1 I2S PDM 麦克风初始化

```c
// i2s_mic_init.c — 使用 I2S1 PDM RX 模式
#include "driver/i2s_pdm.h"

#define I2S_MIC_BCLK   GPIO_NUM_7
#define I2S_MIC_DIN    GPIO_NUM_9

i2s_chan_handle_t rx_handle;

void mic_init(void) {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, NULL, &rx_handle);

    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg  = I2S_PDM_RX_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_PDM_RX_SLOT_PCM_FMT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = I2S_MIC_BCLK,
            .din = I2S_MIC_DIN,
            .invert_flags = { .clk_inv = false },
        },
    };
    i2s_channel_init_pdm_rx_mode(rx_handle, &pdm_rx_cfg);
    i2s_channel_enable(rx_handle);
}
```

> ESP32-S3 I2S1 支持硬件 PDM→PCM 转换，`I2S_PDM_RX_SLOT_PCM_FMT_DEFAULT_CONFIG` 自动启用。来源：[ESP-IDF I2S PDM RX 文档](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/i2s.html#pdm-rx-mode)

### 5.2 音频录制到 Ring Buffer

```c
#define RING_BUF_SIZE   (16000 * 2 * 3)  // 3 秒音频

static int16_t ring_buf[RING_BUF_SIZE];
static volatile size_t ring_write_pos = 0;

void mic_record_task(void *arg) {
    int16_t buf[512];
    size_t bytes_read;
    while (1) {
        i2s_channel_read(rx_handle, buf, sizeof(buf), &bytes_read, portMAX_DELAY);
        size_t samples = bytes_read / sizeof(int16_t);
        // 写入 Ring Buffer
        for (size_t i = 0; i < samples; i++) {
            ring_buf[ring_write_pos] = buf[i];
            ring_write_pos = (ring_write_pos + 1) % RING_BUF_SIZE;
        }
    }
}
```

### 5.3 百度 REST API 调用

```c
#include "esp_http_client.h"
#include "mbedtls/base64.h"

// 获取 access_token (30 天有效，建议启动时获取并缓存)
// POST https://aip.baidubce.com/oauth/2.0/token
// grant_type=client_credentials&client_id=YOUR_API_KEY&client_secret=YOUR_SECRET

esp_err_t baidu_asr_recognize(const int16_t *pcm_data, size_t pcm_bytes,
                               char *result_text, size_t result_max) {
    // 1. Base64 编码 PCM 数据
    size_t b64_len;
    mbedtls_base64_encode(NULL, 0, &b64_len,
                          (const unsigned char *)pcm_data, pcm_bytes);
    char *b64_buf = malloc(b64_len);
    mbedtls_base64_encode((unsigned char *)b64_buf, b64_len, &b64_len,
                          (const unsigned char *)pcm_data, pcm_bytes);

    // 2. 构造 JSON body
    char json_body[2048];
    snprintf(json_body, sizeof(json_body),
        "{"
        "\"format\":\"pcm\","
        "\"rate\":16000,"
        "\"channel\":1,"
        "\"cuid\":\"ESP32-S3-DIJI-NES\","
        "\"token\":\"%s\","
        "\"speech\":\"%s\","
        "\"len\":%d"
        "}", access_token, b64_buf, (int)pcm_bytes);

    // 3. HTTP POST
    esp_http_client_config_t config = {
        .url = "https://vop.baidu.com/server_api",
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_body, strlen(json_body));
    esp_err_t err = esp_http_client_perform(client);

    // 4. 解析 JSON 响应
    if (err == ESP_OK) {
        char response[1024];
        int len = esp_http_client_read_response(client, response, sizeof(response)-1);
        response[len] = '\0';
        // 用 cJSON 解析: result_text = cJSON_GetObjectItem(root,"result")->child->valuestring
    }

    esp_http_client_cleanup(client);
    free(b64_buf);
    return err;
}
```

### 5.4 按键触发录音流程

```
用户按下一个 "录音" 按键 (新增 GPIO，如 GPIO21)
    │
    ▼
voice_recognition_task 被唤醒
    │
    ├─ 1. 读取 Ring Buffer 中最近 N 秒音频 (如 2 秒)
    │
    ├─ 2. 调用 baidu_asr_recognize(pcm, len, &result)
    │
    ├─ 3. 成功 → 将 result 显示到 TFT / 存入变量
    │    失败 → 显示 "识别失败，请重试"
    │
    └─ 4. 等待下次触发
```

---

## 6. WiFi 初始化 (与 DIJI-NES 共存)

```c
// 在 setup() 中，FreeRTOS 调度器启动前
void wifi_init(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid = "YOUR_SSID",
            .password = "YOUR_PASSWORD",
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_start();
    esp_wifi_connect();
}
```

> WiFi 栈运行在独立的 FreeRTOS 任务中，使用 WiFi 专用 CPU 资源（非 Core 0/1），不影响游戏。

---

## 7. 百度 API 开通流程

### 7.1 注册 & 创建应用

```
1. 访问 https://console.bce.baidu.com/ai/#/ai/speech/overview/index
2. 登录百度智能云 (需实名)
3. 点击"创建应用"
4. 选择"语音识别" → 勾选"短语音识别"
5. 获取: AppID, API Key, Secret Key
```

### 7.2 获取 Access Token

```bash
# 在 PC 上测试:
curl "https://aip.baidubce.com/oauth/2.0/token?grant_type=client_credentials\
&client_id=YOUR_API_KEY&client_secret=YOUR_SECRET_KEY"
# 返回: {"access_token":"24.xxxxxx","expires_in":2592000}  # 30天有效
```

### 7.3 费用

| 套餐 | 价格 | 说明 |
|------|------|------|
| 免费 | **5 万次/天** | 个人项目够用 |
| 付费 | ¥3/千次 | 超出免费额度后 |

---

## 8. 与 DIJI-NES 共存的注意事项

### 8.1 电源

INMP441 功耗极低 (<2mA @ 3.3V)，不影响现有供电。

### 8.2 音频干扰

INMP441 的 MEMS 麦克风和 DIJI-NES 的 MAX98357A 喇叭在同一空间会产生**回声**问题。解决方案：
- 使用**按键触发录音**（按住说，松开识别），而非持续监听
- 录音期间短暂静音游戏音频（调用 `muteAudio()`）

### 8.3 Task Watchdog

Core 0 上已有 `display_task` (高优先级 DMA 推屏) + WiFi 栈。新增 `voice_recognition_task` 如果长时间阻塞（如网络超时），可能触发 task watchdog。

解决方案：将 HTTP 超时设短（5 秒），且 `voice_recognition_task` 优先级设为最低。

### 8.4 推荐触发方式

| 方式 | 硬件 | 说明 |
|------|------|------|
| **按键触发** | 新增 1 个按键 (GPIO21→GND) | 按住说话，松开识别 |
| 触屏触发 | 需触屏模块 (另加 I2C) | 点击屏幕按钮触发 |
| 语音唤醒 | 需方案 A 模块 (SU-03T) | WAKE 词触发后切换云端 |

> 推荐**按键触发**：最简单、无干扰、不增加 CPU 负担。

---

## 9. 完整物料清单

| 物料 | 型号 | 数量 | 单价 | 来源 |
|------|------|:---:|------|------|
| MEMS 麦克风 | INMP441 模块 | 1 | ¥8~15 | 淘宝 |
| 微动按键 | 6×6mm 四脚 | 1 | ¥0.5 | 淘宝/立创 |
| 杜邦线 | 母-母 20cm | 4 | ¥2 | 淘宝 |
| — | — | **总计** | **¥10~18** | — |

> 无需额外 PCB，用面包板/杜邦线直接连接。

---

## 10. 实施步骤

| 步骤 | 任务 | 预计耗时 | 依赖 |
|:---:|------|:------:|------|
| 1 | 焊接 INMP441 排针，杜邦线连接到 ESP32 | 10 min | — |
| 2 | 注册百度语音识别，获取 API Key/Secret | 10 min | 百度账号实名 |
| 3 | 在 PC 上用 curl 验证 API 可用 | 5 min | 步骤 2 |
| 4 | 在 ESP-IDF 中集成 I2S PDM 录音驱动 | 1 h | 步骤 1 |
| 5 | 添加 WiFi 初始化代码 (与 DIJI-NES 并行) | 30 min | — |
| 6 | 添加 HTTP Client 调用百度 ASR API | 1 h | 步骤 3,5 |
| 7 | 添加按键触发 + 结果显示 | 30 min | 步骤 4,6 |
| 8 | 联调测试 + 优化延迟 | 1 h | 全部 |
| — | **总计** | **~4.5 h** | — |

---

## 11. 故障排查

| 现象 | 可能原因 | 排查方法 |
|------|---------|---------|
| I2S 初始化失败 | GPIO 冲突 | 确认 GPIO7/9 未被其他外设占用 |
| 录音全是静音 | L/R 脚悬空 | L/R 接 GND |
| 录音噪声大 | 电源干扰 | INMP441 VDD 加 100nF 电容到 GND |
| HTTP 返回 401 | Token 过期 | 重新获取 access_token |
| HTTP 返回 3300/3301 | 音频格式不对 | 确认 PCM 16kHz 16bit mono |
| 识别结果差 | 音量太小 | 调高 INMP441 增益或靠近麦克风说话 |
| 游戏帧率骤降 | voice task 优先级过高 | 降低 voice_recognition_task 优先级 |
