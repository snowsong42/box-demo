# 🔌 GPIO 引脚全表

> ESP32-S3-WROOM-1 (HW-678ABCD) — 共占用 20 个 GPIO

---

## 引脚总表

| # | GPIO | 外设 | 信号 | 方向 | 说明 |
|---|------|------|------|------|------|
| 1 | **14** | TFT (SPI3) | SCLK | OUT | SPI 时钟 |
| 2 | **13** | TFT (SPI3) | MOSI | OUT | SPI 数据 |
| 3 | **11** | TFT (GPIO) | DC | OUT | 数据/命令选择 |
| 4 | **10** | TFT (SPI3) | CS | OUT | 片选 |
| 5 | **12** | TFT (GPIO) | RST | OUT | 硬件复位 |
| 6 | **42** | SD 卡 (FSPI) | CS | OUT | SD 卡片选 |
| 7 | **40** | SD 卡 (FSPI) | SCLK | OUT | SD 卡时钟 |
| 8 | **39** | SD 卡 (FSPI) | MISO | IN | SD 卡数据输入 |
| 9 | **41** | SD 卡 (FSPI) | MOSI | OUT | SD 卡数据输出 |
| 10 | **48** | 按键 | BACK | IN_PU | BACK 键 |
| 11 | **47** | 按键 | START | IN_PU | START 键 |
| 12 | **8** | 按键 | LEFT | IN_PU | 左键 |
| 13 | **18** | 按键 | RIGHT | IN_PU | 右键 |
| 14 | **17** | 按键 | UP | IN_PU | 上键 |
| 15 | **3** | 按键 | DOWN | IN_PU | 下键 ⚠️ Strapping |
| 16 | **5** | I2S0 TX | BCLK | OUT | 音频位时钟 |
| 17 | **4** | I2S0 TX | LRCLK | OUT | 音频左右声道时钟 |
| 18 | **6** | I2S0 TX | DATA | OUT | 音频数据输出 |
| 19 | **7** | I2S1 RX | BCLK | OUT | 麦克风位时钟 |
| 20 | **15** | I2S1 RX | WS | OUT | 麦克风左右声道时钟 |
| | **1** | I2S1 RX | DIN | IN | 麦克风数据输入 |
| | **2** | GPIO | MUTE | OUT | 功放静音 (MAX98357A SD) |

---

## SPI 总线隔离架构

```
ESP32-S3
├── SPI3_HOST (GPIO 10-14)
│   └── TFT ST7789 (80MHz)
│
├── FSPI/SPI2_HOST (GPIO 39-42)
│   └── MicroSD 卡 (10MHz)
│
└── 两个独立 SPI 控制器，各自独立 DMA
```

⚠️ **不要将 TFT 和 SD 卡放在同一条 SPI 总线上**——SD 卡读写时重新初始化 SPI 会导致 TFT 画面闪烁。

---

## 空闲 GPIO

可用作扩展的 GPIO：0, 9, 16, 19-21, 35-38, 43-46

> GPIO 35/36/37 可能被 Octal PSRAM 占用，确认后再用。

---

## Strapping 引脚警告

| GPIO | 功能 | 上电要求 |
|------|------|----------|
| 3 | DOWN 键 | 内部上拉，外接下拉按键不影响上电时序 |

---

## 按键映射

| 物理键 | GPIO | 代码常量 | 返回值 |
|--------|------|----------|--------|
| UP | 17 | BTN_UP | BTN_U (1) |
| DOWN | 3 | BTN_DOWN | BTN_D (2) |
| LEFT | 8 | BTN_LEFT | BTN_L (3) |
| RIGHT | 18 | BTN_RIGHT | BTN_R (4) |
| START | 47 | BTN_START | BTN_S (5) |
| BACK | 48 | BTN_BACK | BTN_B (6) |

按键按下 = GND (低电平)，松开 = 内部上拉高电平。代码中边沿检测：`curr != BTN_NONE && curr != s_prev_btn`。
