# box-demo

> **ESP32-S3 (HW-678) + 可爱橙 2.0" TFT (ST7789) + ESP-IDF v6.0.1 + LovyanGFX**

[![Platform](https://img.shields.io/badge/Platform-ESP32--S3-blue)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Framework](https://img.shields.io/badge/Framework-ESP--IDF%20v6.0.1-green)](https://docs.espressif.com/projects/esp-idf/en/v6.0.1)
[![Display](https://img.shields.io/badge/Display-ST7789%20240%C3%97320-orange)]()

一个基于 ESP32-S3 开发板与 2.0 英寸 ST7789 TFT 彩色屏的**交互式图形应用**，包含主菜单、图片浏览器、走马灯和 GIF 播放器，支持按键交互与背景音乐播放。

---

## 📦 硬件清单

| 组件       | 型号 / 规格                               | 数量 |
| ---------- | ----------------------------------------- | ---- |
| 开发板     | HW-678ABCD (ESP32-S3-WROOM-1-N16R8)       | 1    |
| TFT 彩色屏 | 可爱橙 KAC-N200-2432KHWIG20-A8 (ST7789V2) | 1    |
| 音频 DAC   | MAX98357A I2S 功放模块 + 喇叭             | 1    |
| 按键       | 轻触开关 (UP/DOWN/LEFT/RIGHT)             | 4    |
| 连接线     | 杜邦线 (母-母)                            | 若干 |
| USB 数据线 | Type-C                                    | 1    |

---

## 🔌 接线说明

### 视频接线（TFT）

| ESP32-S3 (HW-678) | TFT (8-Pin SPI) | 信号      | 功能              |
| ----------------- | --------------- | --------- | ----------------- |
| **GPIO14**  | Pin 3 (SCL)     | SPI Clock | SPI 总线时钟      |
| **GPIO13**  | Pin 4 (SDA)     | SPI MOSI  | SPI 主机输出      |
| **GPIO12**  | Pin 5 (RES)     | Reset     | 硬件复位 (低有效) |
| **GPIO11**  | Pin 6 (DC)      | Data/Cmd  | 数据/命令选择     |
| **GPIO10**  | Pin 7 (CS)      | Chip Sel  | 片选 (低有效)     |
| **3.3V**    | Pin 2 (VCC)     | Power     | 逻辑电源 3.3V     |
| **3.3V**    | Pin 8 (BLK)     | Backlight | 背光 (高=亮)      |
| **GND**     | Pin 1 (GND)     | Ground    | 电源地            |

> ⚠️ **背光直接接 3.3V 即常亮**，如需调光可将 BLK 接 GPIO 通过 PWM 控制。

```
ESP32-S3 (HW-678)              TFT-LCD (8-Pin)
┌──────────────┐              ┌────────────────┐
│          GND ├──────────────┤ GND (Pin 1)    │
│         3.3V ├──────────────┤ VCC (Pin 2)    │
│       GPIO14 ├──────────────┤ SCL (Pin 3)    │
│       GPIO13 ├──────────────┤ SDA (Pin 4)    │
│       GPIO12 ├──────────────┤ RES (Pin 5)    │
│       GPIO11 ├──────────────┤ DC  (Pin 6)    │
│       GPIO10 ├──────────────┤ CS  (Pin 7)    │
│         3.3V ├──────────────┤ BLK (Pin 8)    │
└──────────────┘              └────────────────┘
```

### 按键接线（按键）

| ESP32-S3 GPIO | 按键 | 模式         | 说明                                   |
| :-----------: | :---: | ------------ | -------------------------------------- |
|    GPIO17    |  UP  | INPUT_PULLUP | 按下 = GND                             |
|     GPIO3     | DOWN | INPUT_PULLUP | ⚠️ Strapping 引脚 (JTAG)，运行时可用 |
|     GPIO8     | LEFT | INPUT_PULLUP | 按下 = GND                             |
|    GPIO18    | RIGHT | INPUT_PULLUP | 按下 = GND                             |

### 音频接线（MAX98357A）

| ESP32-S3 GPIO | MAX98357A 引脚 | 信号         | 说明 |
| :-----------: | -------------- | ------------ | ---- |
|     GPIO5     | BCLK           | I2S 位时钟   |      |
|     GPIO4     | LRCLK          | I2S 声道时钟 |      |
|     GPIO6     | DIN            | I2S 音频数据 |      |


---

## 🏗️ 软件架构

```
box-demo/
├── main/
│   ├── CMakeLists.txt      # 组件注册 (LovyanGFX + spiffs)
│   ├── main.cpp            # 主程序: 状态机 + 菜单 + 3 个子功能 + 音频任务
│   ├── lgfx_conf.h         # LovyanGFX 配置 (ST7789 + SPI3_HOST + DMA)
│   └── lcd_image.h         # 嵌入式测试图片 (40×40 RGB565)
├── components/
│   └── LovyanGFX/          # 图形库 (本地克隆)
├── resource/               # 运行时资源 (SPIFFS 镜像源)
│   ├── 0001~0003.png       # 图片浏览器测试图
│   ├── gif_0001~0028.png   # GIF 动画帧 (28 帧 200×200)
│   ├── 500x150.raw         # 走马灯图片 (RGB565)
│   └── music.wav           # 背景音乐 (22050Hz 16-bit Mono)
├── docs/                   # 📚 详细文档 (13 篇)
│   ├── 文档索引.md          # 文档索引入口
│   ├── DIJI-NES参考分析.md  # DIJI-NES 参考分析
│   ├── hardware/
│   │   ├── 开发板规格.md        # 开发板完整规格
│   │   └── 显示屏规格.md        # TFT 规格 + 接线
│   ├── architecture/
│   │   ├── 应用架构.md          # 应用架构 (状态机 + 生命周期)
│   │   ├── 按键系统.md          # 按键交互系统
│   │   └── 构建配置.md          # 构建与配置指南
│   ├── guides/
│   │   ├── 抗闪烁方案.md        # 抗闪烁方案
│   │   ├── 音频子系统.md        # 音频子系统
│   │   └── 图片加载指南.md      # 图片加载指南
│   ├── future/
│   │   ├── 云端语音识别方案.md  # WiFi 云端语音识别
│   │   └── 麦克风接入方案.md    # I2S 麦克风方案
│   └── reference/              # 厂商资料 + 参考源码
└── CMakeLists.txt          # 顶层项目文件 (SPIFFS 镜像生成)
```

### 技术栈

| 层级     | 技术选择                                                       |
| -------- | -------------------------------------------------------------- |
| 芯片     | ESP32-S3-N16R8 (Xtensa LX7 双核 240MHz, 16MB Flash, 8MB PSRAM) |
| RTOS     | FreeRTOS (ESP-IDF 内置)                                        |
| 构建系统 | ESP-IDF v6.0.1 + CMake                                         |
| 图形库   | **LovyanGFX** (本地组件克隆)                             |
| 显示屏   | ST7789V2, 240×320, SPI3_HOST, DMA, RGB565                     |
| 音频     | I2S0 标准模式 TX → MAX98357A (22050Hz 16-bit Mono)            |
| 存储     | SPIFFS (4MB 分区，存放图片/音频资源)                           |
| 语言     | C++17 (`.cpp`)                                               |

---

## 🚀 快速开始

### 环境要求

- ESP-IDF v6.0.1 (安装于 `D:\EE\ESP-IDF\.espressif\v6.0.1\esp-idf`)
- ESP32-S3 目标芯片
- COM7 (或对应串口号)

### 编译 & 烧录

```powershell
# 1. 激活 ESP-IDF 环境
. "D:\EE\ESP-IDF\.espressif\v6.0.1\esp-idf\export.ps1"

# 2. 设置目标芯片 (首次)
idf.py set-target esp32s3

# 3. 编译项目
idf.py build

# 4. 烧录到开发板
idf.py -p COM7 flash

# 5. (可选) 查看串口日志
idf.py -p COM7 monitor
```

> ⚠️ 烧录完成后，**可能需要按一下板子上的 RST (EN) 按钮**，显示屏才会开始工作。

### 一键编译烧录

```powershell
idf.py build flash -p COM7
```

---

## � 应用功能

启动后进入**主菜单**，通过 4 个方向键交互，选择并进入子功能。

### 按键操作

|      按键      | 菜单     | 子功能内            |
| :-------------: | -------- | ------------------- |
|  **UP**  | 上移选择 | 取消退出 (弹窗中)   |
| **DOWN** | 下移选择 | 弹出退出确认窗      |
| **LEFT** | —       | 上一张 / 减速 (GIF) |
| **RIGHT** | 确认进入 | 下一张 / 加速 (GIF) |

### 功能一：图片浏览器 (IMG Browser)

- 自动扫描 `/spiffs/` 下 `0001.png` ~ `0099.png`
- PNG 解码后居中显示，LEFT/RIGHT 翻页
- 尺寸缓存避免重复解析 PNG 头部

### 功能二：图片走马灯 (IMG Marquee)

- 加载 `500x150.raw` (RGB565) 到 PSRAM
- 双缓冲 `pushImage` 实现无缝横向滚动
- 自动推进，约 33fps

### 功能三：GIF 播放器 (GIF Player)

- 28 帧 200×200 PNG 序列帧，预加载到 PSRAM Sprite 数组 (~2.24 MB)
- LEFT/RIGHT 调速 (1~20 级，对应 25~500ms 帧间隔)
- 帧序号 + 速度条实时显示

### 背景音乐

- 进入任意子功能自动播放 `/spiffs/music.wav`
- 22050Hz 16-bit Mono PCM，I2S0 TX → MAX98357A
- 退出子功能自动停止，循环播放

---

## 📚 文档索引

### 硬件

| 文档                                            | 内容                                                 |
| ----------------------------------------------- | ---------------------------------------------------- |
| [docs/esp32-s3-board.md](docs/esp32-s3-board.md)   | HW-678 开发板完整规格、41 Pin 引脚表、Strapping 警告 |
| [docs/hardware-wiring.md](docs/hardware-wiring.md) | TFT 接线图、GPIO 矩阵分析、SPI3_HOST 方案详解        |
| [docs/tft-lcd-specs.md](docs/tft-lcd-specs.md)     | ST7789V2 完整参数、电学/光学特性、初始化序列         |

### 软件

| 文档                                              | 内容                                                |
| ------------------------------------------------- | --------------------------------------------------- |
| [docs/app-architecture.md](docs/app-architecture.md) | 三级状态机、退出弹窗、音频生命周期、内存策略        |
| [docs/audio-subsystem.md](docs/audio-subsystem.md)   | I2S 配置、MAX98357A、WAV 播放任务                   |
| [docs/button-system.md](docs/button-system.md)       | 4 键接线、边缘检测状态机、盲区补偿                  |
| [docs/build-config.md](docs/build-config.md)         | sdkconfig 关键项、分区表、SPIFFS 资源管理、编译烧录 |

### 参考

| 文档                                                                        | 内容                                            |
| --------------------------------------------------------------------------- | ----------------------------------------------- |
| [docs/diji-nes-reference.md](docs/diji-nes-reference.md)                       | DIJI-NES 原型完整分析、硬件对比、可复用方案     |
| [docs/lovyangfx-sprite-image-guide.md](docs/lovyangfx-sprite-image-guide.md)   | 开发踩坑记录：SPIFFS 兼容、PSRAM、按键防抖      |
| [docs/anti-flicker-backbuffer-guide.md](docs/anti-flicker-backbuffer-guide.md) | 抗闪烁演进：从 fillRect→Sprite→0x28→全帧缓冲 |
| [docs/cloud-asr-plan.md](docs/cloud-asr-plan.md)                               | 🚧 未来规划：WiFi 云端语音识别方案              |

---

## 🔬 关键技术决策

### 为什么用 LovyanGFX？

- 原生支持 **ESP-IDF**（不仅是 Arduino）
- 内置 **DMA 传输**，SPI 利用率高
- 支持 ST7789 Panel 类，初始化序列自动化
- 被 DIJI-NES 验证可靠（60FPS 游戏渲染）

### 为什么 SPI3_HOST 而不是 FSPI？

GPIO10~14 在 ESP32-S3 上对应 FSPI (SPI2) 默认引脚，但 GPIO11/12 被 TFT 的 DC/RES 占用。使用 **SPI3_HOST + GPIO 矩阵重映射** 避免了引脚冲突。

详见 [docs/hardware-wiring.md](docs/hardware-wiring.md#esp32-s3-硬件-spi-分析-gpio-矩阵)。

### 为什么 invert=true、rgb_order=false？

此 ST7789 屏的默认配置需要：

- `invert = true` → 否则画面全白
- `rgb_order = false` (BGR) → 否则颜色偏绿/蓝

这两个参数在 DIJI-NES 和 STM32 例程中均有验证。

---

## 🙏 参考与致谢

| 项目                                                               | 说明                              |
| ------------------------------------------------------------------ | --------------------------------- |
| [DIJI-NES](https://github.com/k7212519/DIJI-NES)                      | ESP32-S3 NES 模拟器，验证硬件配置 |
| [LovyanGFX](https://github.com/lovyan03/LovyanGFX)                    | 高性能图形库 (1.7K⭐)             |
| [ESP32_S3_N8R2_RGB](https://github.com/shantanuk47/ESP32_S3_N8R2_RGB) | HW-678 WS2812 参考实现            |
| 可爱橙科技                                                         | TFT 显示屏供应商 (规格书 + 例程)  |

---

## 📝 许可证

本项目代码基于 ESP-IDF 官方模板，LovyanGFX 遵循 FreeBSD 许可证。
