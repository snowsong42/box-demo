# ESP32-S3 开发板规格 (HW-678)

> 来源：HW-678ABCD 共用产品说明书 v0.0.0 + **ESP32-S3-WROOM-1 官方数据手册 v1.8** + ESP32-S3 Series Datasheet

---

## 1. 产品标识

| 项目       | 参数                       |
| ---------- | -------------------------- |
| 产品型号   | HW-678ABCD                 |
| 核心模组   | **ESP32-S3-WROOM-1** |
| SoC        | Espressif ESP32-S3 系列    |
| 说明书版本 | v0.0.0 (2024-10-06)        |

---

## 2. 模组硬件参数

| 项目      | HW-678A (推测)                   | HW-678B (推测)                      |
| --------- | -------------------------------- | ----------------------------------- |
| 模组型号  | ESP32-S3-WROOM-1-**N16R8** | ESP32-S3-WROOM-1-**N8R2**     |
| 架构      | Xtensa LX7 双核 32位 (带 FPU)    | 同左                                |
| 主频      | 最高 240 MHz                     | 同左                                |
| SRAM      | 512 KB (片上)                    | 同左                                |
| ROM       | 384 KB                           | 同左                                |
| RTC SRAM  | 16 KB                            | 同左                                |
| Flash     | **16 MB** Quad SPI         | **8 MB** Quad SPI             |
| PSRAM     | **8 MB** Octal SPI         | **2 MB** Quad SPI             |
| 晶振      | 40 MHz                           | 同左                                |
| Wi-Fi     | 2.4 GHz 802.11 b/g/n (150Mbps)   | 同左                                |
| 蓝牙      | BLE 5.0 + Bluetooth Mesh         | 同左                                |
| 可用 GPIO | **36 个** (模组共 41 pin)  | **36 个** (有 3 个差异，见下) |

> ⚠️ **注意**：HW-678A 使用 **Octal SPI PSRAM (8MB)**，会占用 **GPIO35、GPIO36、GPIO37** 三个 IO——这三个 IO 不可用于其他用途。HW-678B 使用 Quad SPI PSRAM (2MB)，这三个 IO 可用。

---

## 3. 板载资源

| 资源                     | 规格                                                  |
| ------------------------ | ----------------------------------------------------- |
| **WS2812 RGB LED** | 可编程全彩 LED，连接至**GPIO48**                |
| **USB**            | **Type-C**，支持供电 + 串口 + **USB-OTG** |
| **BOOT 按钮**      | 进入下载模式                                          |
| **RGB 扩展接口**   | IN-OUT 排针，可外接 WS2812 灯带                       |
| **I2C**            | 任意 GPIO 可配置                                      |
| **SPI**            | 硬件 SPI2(FSPI) + SPI3 可用                           |
| **天线**           | PCB 板载天线                                          |
| **供电**           | 3.3V ~ 5V（USB 或 5V pin 输入）                       |
| **尺寸**           | 57mm × 28mm                                          |
| **IDE 支持**       | Arduino IDE 2.2.1 / ESP-IDF / PlatformIO              |

---

## 4. 完整引脚定义表 (ESP32-S3-WROOM-1, 41 Pins)

> 格式：`Pin# | Name | 类型 | 复用功能（粗体为默认功能）`

### 4.1 本项目使用的引脚 (TFT-LCD)

| Pin# | Name           | 类型  | 复用功能 (默认粗体)                                                 | 本项目用途                   |
| ---- | -------------- | ----- | ------------------------------------------------------------------- | ---------------------------- |
| 18   | **IO10** | I/O/T | RTC_GPIO10, TOUCH10, ADC1_CH9,**FSPICS0**, FSPIIO4, SUBSPICS0 | **TFT CS** (片选)      |
| 19   | **IO11** | I/O/T | RTC_GPIO11, TOUCH11, ADC2_CH0,**FSPID**, FSPIIO5, SUBSPID     | **TFT DC** (数据/命令) |
| 20   | **IO12** | I/O/T | RTC_GPIO12, TOUCH12, ADC2_CH1,**FSPICLK**, FSPIIO6, SUBSPICLK | **TFT RES** (复位)     |
| 21   | **IO13** | I/O/T | RTC_GPIO13, TOUCH13, ADC2_CH2,**FSPIQ**, FSPIIO7, SUBSPIQ     | **TFT SDA** (SPI MOSI) |
| 22   | **IO14** | I/O/T | RTC_GPIO14, TOUCH14, ADC2_CH3,**FSPIWP**, FSPIDQS, SUBSPIWP   | **TFT SCL** (SPI 时钟) |

### 4.2 模组全部 41 Pin 定义

| Pin#         | Name           | 类型  | 复用功能                                                       |
| ------------ | -------------- | ----- | -------------------------------------------------------------- |
| 1            | GND            | P     | 地                                                             |
| 2            | **3V3**  | P     | 3.3V 电源输入                                                  |
| 3            | **EN**   | I     | 高电平=芯片使能；低电平=关闭。**不可悬空！**                   |
| 4            | IO4            | I/O/T | RTC_GPIO4, TOUCH4, ADC1_CH3                                    |
| 5            | IO5            | I/O/T | RTC_GPIO5, TOUCH5, ADC1_CH4                                    |
| 6            | IO6            | I/O/T | RTC_GPIO6, TOUCH6, ADC1_CH5                                    |
| 7            | IO7            | I/O/T | RTC_GPIO7, TOUCH7, ADC1_CH6                                    |
| 8            | IO15           | I/O/T | RTC_GPIO15, U0RTS, ADC2_CH4, XTAL_32K_P                        |
| 9            | IO16           | I/O/T | RTC_GPIO16, U0CTS, ADC2_CH5, XTAL_32K_N                        |
| 10           | IO17           | I/O/T | RTC_GPIO17, U1TXD, ADC2_CH6                                    |
| 11           | IO18           | I/O/T | RTC_GPIO18, U1RXD, ADC2_CH7, CLK_OUT3                          |
| 12           | IO8            | I/O/T | RTC_GPIO8, TOUCH8, ADC1_CH7, SUBSPICS1                         |
| 13           | IO19           | I/O/T | RTC_GPIO19, U1RTS, ADC2_CH8, CLK_OUT2,**USB_D-**         |
| 14           | IO20           | I/O/T | RTC_GPIO20, U1CTS, ADC2_CH9, CLK_OUT1,**USB_D+**         |
| **15** | **IO3**  | I/O/T | RTC_GPIO3, TOUCH3, ADC1_CH2 ⚠️**Strapping: JTAG 控制** |
| 16           | IO46           | I/O/T | GPIO46 ⚠️**Strapping: 启动模式**                       |
| 17           | IO9            | I/O/T | RTC_GPIO9, TOUCH9, ADC1_CH8, FSPIHD, SUBSPIHD                  |
| **18** | **IO10** | I/O/T | RTC_GPIO10, TOUCH10, ADC1_CH9,**FSPICS0**, FSPIIO4       |
| **19** | **IO11** | I/O/T | RTC_GPIO11, TOUCH11, ADC2_CH0,**FSPID**, FSPIIO5         |
| **20** | **IO12** | I/O/T | RTC_GPIO12, TOUCH12, ADC2_CH1,**FSPICLK**, FSPIIO6       |
| **21** | **IO13** | I/O/T | RTC_GPIO13, TOUCH13, ADC2_CH2,**FSPIQ**, FSPIIO7         |
| **22** | **IO14** | I/O/T | RTC_GPIO14, TOUCH14, ADC2_CH3,**FSPIWP**, FSPIDQS        |
| 23           | IO21           | I/O/T | RTC_GPIO21                                                     |
| 24           | IO47           | I/O/T | SPICLK_P_DIFF, SUBSPICLK_P_DIFF 🔸 R16V 模块=1.8V              |
| **25** | **IO48** | I/O/T | SPICLK_N_DIFF, SUBSPICLK_N_DIFF 🔸 R16V 模块=1.8V              |
| 26           | IO45           | I/O/T | GPIO45 ⚠️**Strapping: VDD_SPI 电压控制**               |
| **27** | **IO0**  | I/O/T | RTC_GPIO0 ⚠️**Strapping: 启动模式 (最重要)**           |
| 28           | IO35           | I/O/T | SPIIO6, FSPID, SUBSPID 🔸**Octal PSRAM 模块不可用**      |

> 🔸 = 仅 Quad PSRAM (HW-678B N8R2) 可用，Octal PSRAM (HW-678A N16R8) 被 PSRAM 占用。
> ⚠️ = Strapping 引脚，上电时电平决定芯片配置，外接电路需注意。

### 4.3 模组引脚表（续）

| Pin# | Name           | 类型  | 复用功能                                                      |
| ---- | -------------- | ----- | ------------------------------------------------------------- |
| 29   | IO36           | I/O/T | SPIIO7, FSPICLK, SUBSPICLK 🔸**Octal PSRAM 模块不可用** |
| 30   | IO37           | I/O/T | SPIDQS, FSPIQ, SUBSPIQ 🔸**Octal PSRAM 模块不可用**     |
| 31   | IO38           | I/O/T | FSPIWP, SUBSPIWP                                              |
| 32   | IO39           | I/O/T | MTCK, CLK_OUT3, SUBSPICS1                                     |
| 33   | IO40           | I/O/T | MTDO, CLK_OUT2                                                |
| 34   | IO41           | I/O/T | MTDI, CLK_OUT1                                                |
| 35   | IO42           | I/O/T | MTMS                                                          |
| 36   | **RXD0** | I/O/T | **U0RXD**, GPIO44, CLK_OUT2 (USB 串口 RX)               |
| 37   | **TXD0** | I/O/T | **U0TXD**, GPIO43, CLK_OUT1 (USB 串口 TX)               |
| 38   | IO2            | I/O/T | RTC_GPIO2, TOUCH2, ADC1_CH1                                   |
| 39   | IO1            | I/O/T | RTC_GPIO1, TOUCH1, ADC1_CH0                                   |
| 40   | GND            | P     | 地                                                            |
| 41   | EPAD           | P     | 底部散热焊盘 (必须接地)                                       |

---

## 5. 本项目 TFT 连线深度分析

### 5.1 GPIO10~14 = FSPI (SPI2) 硬件 SPI 接口

| GPIO   | FSPI 默认功能                    | TFT 连接 | 可做硬件 SPI 角色        |
| ------ | -------------------------------- | -------- | ------------------------ |
| GPIO10 | **FSPICS0** (SPI2 片选 0)  | TFT CS   | ✓ FSPI CS               |
| GPIO11 | **FSPID** (SPI2 MOSI 数据) | TFT DC   | ⚠️ 被 DC 占用          |
| GPIO12 | **FSPICLK** (SPI2 时钟)    | TFT RES  | ⚠️ 被 RES 占用         |
| GPIO13 | **FSPIQ** (SPI2 MISO)      | TFT SDA  | 可用作 MOSI (经GPIO矩阵) |
| GPIO14 | **FSPIWP** (SPI2 写保护)   | TFT SCL  | 可用作 CLK (经GPIO矩阵)  |

### 5.2 关键结论

**当前接线方式无法直接使用硬件 FSPI**，因为 FSPI 的标准信号分配被 TFT 的控制信号（RES、DC）干扰：

- GPIO11 → 本该是 FSPI MOSI，实际接 TFT DC → **被占用**
- GPIO12 → 本该是 FSPI CLK，实际接 TFT RES → **被占用**

**两种驱动方案**：

| 方案                                     | 实现                                    | 优势               | 劣势                                    |
| ---------------------------------------- | --------------------------------------- | ------------------ | --------------------------------------- |
| **A: 软件 SPI** (当前 README 方式) | GPIO bit-banging                        | 简单，不改硬件     | 速度较慢，CPU 占用高                    |
| **B: 硬件 SPI2 + GPIO 矩阵**       | 将 SPI CLK/MOSI 通过矩阵映射到其他 GPIO | 硬件加速，DMA 支持 | 需用 esp-idf driver，当前接线可能需调整 |

**方案 B 的可行映射（不改硬件的前提）**：

```
spi_device_interface_config_t devcfg = {
    .spics_io_num   = GPIO10,  // CS → FSPICS0 ✓ 复用正确
    .clock_speed_hz = SPI_MASTER_FREQ_40M,
};
spi_bus_config_t buscfg = {
    .mosi_io_num = GPIO13,  // SDA → 通过矩阵做 MOSI
    .sclk_io_num = GPIO14,  // SCL → 通过矩阵做 CLK
    .miso_io_num = -1,      // TFT 只写不读，不需要 MISO
};
// RES 和 DC 仍用 GPIO 单独控制
```

> 💡 ESP32-S3 的 **GPIO 交换矩阵 (GPIO Matrix)** 允许将 SPI 信号路由到任意 GPIO，因此硬件 SPI 可以使用非默认引脚。

---

## 6. Strapping 引脚警告

| GPIO             | Strapping 功能               | 默认值   | 本项目状态              |
| ---------------- | ---------------------------- | -------- | ----------------------- |
| **GPIO0**  | 启动模式：低=下载，高=运行   | 上拉 (1) | BOOT 按钮接地→下载模式 |
| **GPIO3**  | JTAG 信号源控制              | —       | 未使用，安全            |
| **GPIO45** | VDD_SPI 电压：0=3.3V, 1=1.8V | 下拉 (0) | 未使用，Flash=3.3V      |
| **GPIO46** | ROM 日志打印控制             | 下拉 (0) | 未使用，安全            |

---

## 7. ESP32-S3 外设资源总览

| 外设            | 数量          | 说明                                |
| --------------- | ------------- | ----------------------------------- |
| SPI             | 4 个          | SPI0/1 (内部Flash), SPI2=FSPI, SPI3 |
| I2C             | 2 个          | I2C0, I2C1，可映射到任意 GPIO       |
| I2S             | 2 个          | 支持标准/PDM/TDM 模式               |
| UART            | 3 个          | UART0 (USB调试), UART1, UART2       |
| USB OTG         | 1 个          | Full-Speed 2.0, IO19/IO20           |
| USB Serial/JTAG | 1 个          | 内置，无需外部芯片                  |
| ADC             | 2 个 (20通道) | ADC1 (GPIO1-10), ADC2 (GPIO11-20)   |
| 触摸传感器      | 14 个         | TOUCH0~14                           |
| LED PWM         | 8 路          | 独立通道                            |
| MCPWM           | 2 个          | 电机控制 PWM                        |
| 温度传感器      | 1 个          | 片内                                |
| TWAI            | 1 个          | CAN 2.0 兼容                        |
| LCD 接口        | 1 个          | 8/16-bit 并口 RGB 屏                |
| 摄像头接口      | 1 个          | 8/16-bit DVP                        |
| SD/MMC          | 1 个          | 2 个 SD 卡槽                        |
| RMT             | 8 通道        | 红外遥控 / WS2812 驱动              |
| 脉冲计数器      | 4 个          | 正交编码器                          |

---

## 8. HW-678 板级注意事项

| # | 注意点                                                                          |
| - | ------------------------------------------------------------------------------- |
| 1 | **GPIO43/44** = USB 串口 (TXD0/RXD0)，调试时不要用作其他用途              |
| 2 | **GPIO19/20** = USB-OTG D-/D+，如需使用原生 USB 功能则不可占用            |
| 3 | **GPIO48** = WS2812 (HW-678A 和 HW-678B 都用)，这是 RGB LED               |
| 4 | 如果板子是**HW-678A** (N16R8)，**GPIO35/36/37 被 Octal PSRAM 独占** |
| 5 | 如果板子是**HW-678B** (N8R2)，GPIO35/36/37 可用                           |
| 6 | GPIO47/48 在 R16V 模块是**1.8V 电平**（本板非 R16V，安全）                |
| 7 | TFT 背光直接接 3.3V → 上电即常亮，无法通过软件调光（除非外加 PWM 电路）        |

---

## 9. Arduino IDE / PlatformIO 配置

```ini
; ESP-IDF (sdkconfig)
CONFIG_ESP32S3_SPIRAM_SUPPORT=y
; HW-678A:
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SIZE=8388608
; HW-678B:
CONFIG_SPIRAM_MODE_QUAD=y
CONFIG_SPIRAM_SIZE=2097152
```

```
; PlatformIO
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino  ; 或 espidf
board_build.flash_mode = qio
board_build.f_flash = 80000000L
```

---

## 10. DIJI-NES 项目经验 (ESP32-S3 实战模式)

DIJI-NES 是一个在 HW-678 (ESP32-S3 N16R8) 上运行的 NES 模拟器，项目经过多次迭代优化，积累了以下可复用的实战经验。

### 10.1 双核 FreeRTOS 任务分配

| 核心    | 职责                                       | 说明                   |
| ------- | ------------------------------------------ | ---------------------- |
| **Core 0** | 显示 DMA 推送 (`display_task`) + 音频 I2S | 协议栈核心，低延迟 I/O |
| **Core 1** | CPU 模拟 + PPU 渲染                        | 计算密集型             |

```cpp
// 在 setup() 中将 display_task 绑定到 Core 0
xTaskCreatePinnedToCore(display_task, "display", 4096, NULL, 1, NULL, 0);
```

> 💡 **本项目启示**：纯图像显示 Demo 计算量小，不需要双核分离。但如果后续加入动画、视频解码等计算密集任务，应参照此模式将渲染放 Core 1、DMA 推送放 Core 0。

### 10.2 双缓冲 + DMA 推屏管线

```
Core 1 (渲染)                    Core 0 (DMA 推送)
  │                                │
  ├─ 渲染到 frame_buf[0] ──→ Queue ──→ display_task 读取
  ├─ 渲染到 frame_buf[1] ──→ Queue ──→ display_task 读取
  │       ...                       ...
```

- 分配两块 frame buffer (`heap_caps_malloc` with `MALLOC_CAP_DMA`)
- 渲染完成后通过 FreeRTOS Queue 传递 buffer 索引
- display_task 使用 `pushPixelsDMA` 分块传输，60 行/块 = 4 次 DMA/帧
- 每次 DMA 后 `vTaskDelay(2)` 让出时间片，防止 task watchdog 超时

### 10.3 SPI 总线隔离策略

DIJI-NES 将 TFT 和 SD 卡放在不同的 SPI 总线上，避免相互干扰：

| 外设   | SPI 总线      | 引脚                          |
| ------ | ------------- | ----------------------------- |
| TFT    | **SPI3_HOST** | SCLK=14, MOSI=13, DC=11, CS=10 |
| SD 卡  | **FSPI (SPI2)** | SCLK=40, MISO=39, MOSI=41, CS=42 |

> 💡 本项目当前只使用 TFT，可直接用 SPI3_HOST（LovyanGFX 推荐方式）。

### 10.4 实际构建参数 (已验证)

```ini
# PlatformIO (DIJI-NES 实际使用)
board = esp32-s3-devkitc-1
board_build.flash_mode = qio          # Quad I/O Flash 模式
board_build.arduino.memory_type = qio_opi  # Octal PSRAM
board_build.psram_type = opi
board_upload.flash_size = 16MB
build_flags = -Ofast -funroll-loops   # 激进优化
```

对应 ESP-IDF `sdkconfig`：
```ini
CONFIG_ESP32S3_SPIRAM_SUPPORT=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SIZE=8388608
CONFIG_ESP32S3_DEFAULT_CPU_FREQ_240=y
```

---

## 11. WS2812 RGB LED (GPIO48)

HW-678 板载一颗 WS2812 可编程 RGB LED，连接至 **GPIO48**。

### 11.1 ESP-IDF 驱动方法

使用 RMT (Remote Control Transceiver) 外设驱动：

```cpp
#include "driver/rmt_tx.h"
#include "led_strip.h"

// 初始化
led_strip_config_t strip_config = {
    .strip_gpio_num = 48,          // WS2812 数据引脚
    .max_leds = 1,                 // 板载只有 1 颗
    .led_pixel_format = LED_PIXEL_FORMAT_GRB,
    .led_model = LED_MODEL_WS2812,
};
led_strip_rmt_config_t rmt_config = {
    .resolution_hz = 10 * 1000 * 1000, // 10MHz
};
led_strip_handle_t led_strip;
ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

// 设置颜色 (R, G, B 各 0~255)
led_strip_set_pixel(led_strip, 0, 255, 0, 0); // 红色
led_strip_refresh(led_strip);
```

### 11.2 参考项目

GitHub: [shantanuk47/ESP32_S3_N8R2_RGB](https://github.com/shantanuk47/ESP32_S3_N8R2_RGB) — 专门针对 HW-678 (N8R2) 的 WS2812 驱动实现。

---

## 12. HW-678 vs 官方 DevKitC-1 差异

| 特性               | HW-678ABCD                     | ESP32-S3-DevKitC-1 (官方)    |
| ------------------ | ------------------------------ | ---------------------------- |
| 板载 LED           | WS2812 RGB (GPIO48)            | 普通单色 LED (GPIO48)        |
| RGB 扩展接口       | 有 (IN-OUT 排针，可串 WS2812)  | 无                           |
| USB 口             | Type-C (供电+串口+OTG)         | Micro-USB + 独立 UART 芯片   |
| 天线               | PCB 板载天线                   | PCB 板载 / IPEX 可选         |
| BOOT 按钮          | 有                             | 有                           |
| RST 按钮           | EN 引脚                        | 独立 RST 按键                |
| 适用场景           | 入门/教育/原型验证             | 官方参考设计                 |

---

## 13. 本项目实际应用配置

| 参数             | 值                          |
| ---------------- | --------------------------- |
| 开发板           | HW-678ABCD                  |
| 推测型号         | 待确认 (N16R8 或 N8R2)     |
| ESP-IDF 版本     | v6.0.1                      |
| 目标芯片         | esp32s3                     |
| TFT 驱动库       | LovyanGFX (本地组件)        |
| SPI 总线         | SPI3_HOST                   |
| GPIO 占用        | 10(CS), 11(DC), 12(RES), 13(SDA), 14(SCL) |
| 编译优化         | 默认 (-Og 或 -Os)           |
| 烧录端口         | COM7 (USB-Serial/JTAG)      |
| PSRAM            | 已启用 (y)                  |
