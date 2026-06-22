# BUTT-demo

> **ESP32-S3 (HW-678) + 可爱橙 2.0" TFT (ST7789) + ESP-IDF v6.0.1 + LovyanGFX**

[![Platform](https://img.shields.io/badge/Platform-ESP32--S3-blue)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Framework](https://img.shields.io/badge/Framework-ESP--IDF%20v6.0.1-green)](https://docs.espressif.com/projects/esp-idf/en/v6.0.1)
[![Display](https://img.shields.io/badge/Display-ST7789%20240%C3%97320-orange)]()

一个基于 ESP32-S3 开发板与 2.0 英寸 ST7789 TFT 彩色屏的图形显示 Demo，包含纯色填充、几何图形、嵌入式图片、文字居中显示等基本功能。

---

## 📦 硬件清单

| 组件       | 型号 / 规格                               | 数量 |
| ---------- | ----------------------------------------- | ---- |
| 开发板     | HW-678ABCD (ESP32-S3-WROOM-1)             | 1    |
| TFT 彩色屏 | 可爱橙 KAC-N200-2432KHWIG20-A8 (ST7789V2) | 1    |
| 连接线     | 杜邦线 (母-母) × 8                       | 8 根 |
| USB 数据线 | Type-C                                    | 1    |

---

## 🔌 接线说明

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

---

## 🏗️ 软件架构

```
BUTT-demo/
├── main/
│   ├── CMakeLists.txt      # 组件注册 (LovyanGFX)
│   ├── main.cpp            # 主程序: init → 颜色 → 图形 → 图片 → 文字
│   ├── lgfx_conf.h         # LovyanGFX 配置 (ST7789 + SPI3)
│   └── lcd_image.h         # 嵌入式测试图片 (40×40 RGB565)
├── components/
│   └── LovyanGFX/          # 图形库 (本地克隆)
├── docs/                   # 📚 详细文档
│   ├── esp32-s3-board.md   # 开发板完整规格
│   ├── hardware-wiring.md  # 接线详解 + GPIO矩阵分析
│   ├── tft-lcd-specs.md    # TFT 完整规格 + ST7789 寄存器
│   └── diji-nes-reference.md # DIJI-NES 参考分析
├── DIJI-NES-main/          # 参考项目 (NES 模拟器)
├── 可爱橙彩色屏/           # 厂商原始资料
├── HW-678ABCD共用产品说明书v0.0.0.pdf
└── CMakeLists.txt          # 顶层项目文件
```

### 技术栈

| 层级     | 技术选择                           |
| -------- | ---------------------------------- |
| 芯片     | ESP32-S3 (Xtensa LX7 双核)         |
| RTOS     | FreeRTOS (ESP-IDF 内置)            |
| 构建系统 | ESP-IDF v6.0.1 + CMake             |
| 图形库   | **LovyanGFX** (本地组件克隆) |
| 显示屏   | ST7789V2, 240×320, SPI, RGB565    |
| 语言     | C++17 (`.cpp`)                   |

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

## 🎬 Demo 演示流程

运行后屏幕依次展示：

| 步骤 | 内容                 | 时长 | 说明                                  |
| ---- | -------------------- | ---- | ------------------------------------- |
| 1    | **纯色填充**   | 5 秒 | 红 → 绿 → 蓝 → 白 → 黑，每色 1 秒 |
| 2    | **几何图形**   | 2 秒 | 双层边框 + 十字线 + 四角色块          |
| 3    | **嵌入式图片** | 2 秒 | 40×40 RGB565 测试图 (四象限)         |
| 4    | **居中文字**   | 持续 | 标题 + 分隔线 + 信息 + 签名           |

### 测试图片说明

`lcd_image.h` 中的测试图片为 40×40 像素 RGB565 格式：

```
┌─────────────────────┐
│  RED    │  GREEN    │  ← 上半部分
│ (0xF800)│ (0x07E0)  │
├─────────┼───────────┤
│  BLUE   │  YELLOW   │  ← 下半部分
│ (0x001F)│ (0xFFE0)  │
└─────────────────────┘
```

---

## 📚 文档索引

| 文档                                                  | 内容                                |
| ----------------------------------------------------- | ----------------------------------- |
| [docs/esp32-s3-board.md](docs/esp32-s3-board.md)         | HW-678 开发板完整规格、引脚表、外设 |
| [docs/hardware-wiring.md](docs/hardware-wiring.md)       | 接线图、GPIO 矩阵分析、SPI 方案     |
| [docs/tft-lcd-specs.md](docs/tft-lcd-specs.md)           | ST7789V2 完整参数、寄存器对照       |
| [docs/diji-nes-reference.md](docs/diji-nes-reference.md) | DIJI-NES 架构分析与可复用模式       |

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
