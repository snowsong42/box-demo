# DIJI-NES 参考分析

> 来源：
>
> - GitHub: [UF-Evan/DIJI-NES](https://github.com/UF-Evan/DIJI-NES) (194⭐, 48 forks, GPLv3) — 官方仓库
> - 立创开源硬件平台: [DIJI-NES](https://oshwhub.com/uf-evan/project_gikrsdoj) — PCB 设计 + BOM + 3D 外壳
> - Gitee 镜像: [UF-Evan/DIJI-NES](https://gitee.com/UF-Evan/DIJI-NES) — 国内访问镜像
>
> DIJI-NES 是一个基于 ESP32-S3 的**开源 NES 掌机项目**，采用定制 PCB、锂电池供电和 3D 打印外壳。本项目与其使用**相同的主控芯片** (ESP32-S3-N16R8) 和**相同的 TFT 驱动 IC** (ST7789V2)，但 TFT 模组型号和物理接口形式不同。

本文档提取 DIJI-NES 项目中可复用的硬件配置、软件架构和优化策略，供本项目及后续开发参考。

---

## 1. 项目概况

| 项目属性    | 值                                                                               | 来源                                                                                                 |
| ----------- | -------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------- |
| 名称        | DIJI-NES                                                                         | GitHub README                                                                                        |
| 作者        | [UF-Evan](https://github.com/UF-Evan)                                               | GitHub 仓库                                                                                          |
| 平台        | ESP32-S3-N16R8 (双核 240MHz, 16MB Flash, 8MB Octal PSRAM)                        | [GitHub §硬件需求](https://github.com/UF-Evan/DIJI-NES#%EF%B8%8F-%E7%A1%AC%E4%BB%B6%E9%9C%80%E6%B1%82) |
| PCB 形态    | **定制 PCB** (立创 EDA 设计), 非通用开发板                                 | [立创开源平台](https://oshwhub.com/uf-evan/project_gikrsdoj)                                            |
| 框架        | Arduino (PlatformIO)                                                             | [platformio.ini](https://github.com/UF-Evan/DIJI-NES/blob/main/platformio.ini)                          |
| 图形库      | **LovyanGFX v1.2.7**                                                       | platformio.ini `lib_deps`                                                                          |
| TFT 型号    | **HD20001C12** (深圳华迪创显科技)                                          | [立创 §已验证屏幕](https://oshwhub.com/uf-evan/project_gikrsdoj)                                       |
| TFT 驱动 IC | ST7789V2, 240×320, SPI                                                          | 立创描述 + lgfx_conf.h                                                                               |
| TFT 接口    | 12-pin FPC (0.5mm 间距)                                                          | 立创 BOM: AFC01-S12FCA-00                                                                            |
| 音频 DAC    | MAX98357A I2S                                                                    | [GitHub §硬件需求](https://github.com/UF-Evan/DIJI-NES#%EF%B8%8F-%E7%A1%AC%E4%BB%B6%E9%9C%80%E6%B1%82) |
| 存储        | MicroSD 卡 (FAT32, SDHC)                                                         | GitHub README                                                                                        |
| 供电        | 3.7V 锂电池 + TP4056 充电 + Type-C                                               | 立创原理图                                                                                           |
| 许可证      | GPLv3                                                                            | [LICENSE](https://github.com/UF-Evan/DIJI-NES/blob/main/LICENSE)                                        |
| 版本        | v0.5.0 (2026-06-18)                                                              | [GitHub Releases](https://github.com/UF-Evan/DIJI-NES/releases)                                         |
| 关键参考    | [Anemoia-ESP32](https://github.com/Shim06/Anemoia-ESP32) — APU 时钟同步 + 帧级调度 | [GitHub §致谢](https://github.com/UF-Evan/DIJI-NES#%EF%B8%8F-%E8%87%B4%E8%B0%A2)                       |

---

## 2. 完整硬件接线分析

> 以下所有引脚定义提取自 `DIJI-NES-main/src/main.cpp` 第 66~89 行 PIN 宏定义段。

### 2.1 ESP32-S3 引脚总占用表

DIJI-NES 共占用 ESP32-S3 的 **18 个 GPIO**，分布在 4 组外设上：

| 序号 | GPIO | 外设        | 信号   | 方向  | 说明                       |
| ---- | ---- | ----------- | ------ | ----- | -------------------------- |
| 1    | 14   | TFT (SPI3)  | SCLK   | OUT   | SPI 时钟                   |
| 2    | 13   | TFT (SPI3)  | MOSI   | OUT   | SPI 数据                   |
| 3    | 11   | TFT (GPIO)  | DC     | OUT   | 数据/命令选择              |
| 4    | 10   | TFT (SPI3)  | CS     | OUT   | 片选                       |
| 5    | 12   | TFT (GPIO)  | RST    | OUT   | 硬件复位                   |
| 6    | 42   | SD卡 (FSPI) | CS     | OUT   | SD 卡片选                  |
| 7    | 40   | SD卡 (FSPI) | SCLK   | OUT   | SD 卡时钟                  |
| 8    | 39   | SD卡 (FSPI) | MISO   | IN    | SD 卡数据输入              |
| 9    | 41   | SD卡 (FSPI) | MOSI   | OUT   | SD 卡数据输出              |
| 10   | 48   | 手柄按键    | A      | IN_PU | A 键 (⚠️ 与 WS2812 冲突) |
| 11   | 47   | 手柄按键    | B      | IN_PU | B 键                       |
| 12   | 8    | 手柄按键    | LEFT   | IN_PU | 左键                       |
| 13   | 18   | 手柄按键    | RIGHT  | IN_PU | 右键                       |
| 14   | 17   | 手柄按键    | UP     | IN_PU | 上键                       |
| 15   | 3    | 手柄按键    | DOWN   | IN_PU | 下键 ⚠️ Strapping 引脚   |
| 16   | 15   | 手柄按键    | START  | IN_PU | 开始键                     |
| 17   | 16   | 手柄按键    | SELECT | IN_PU | 选择键                     |
| 18   | 5    | I2S 音频    | BCLK   | OUT   | I2S 位时钟                 |
| 19   | 4    | I2S 音频    | LRCLK  | OUT   | I2S 左右声道时钟           |
| 20   | 6    | I2S 音频    | DATA   | OUT   | I2S 音频数据               |

> IN_PU = 输入模式，启用内部上拉电阻。按键按下 = GND (低电平)，松开 = 上拉高电平。

### 2.2 TFT 显示屏接线 (SPI3_HOST)

```cpp
// 来自 lgfx_conf.h — LovyanGFX 总线配置
cfg.spi_host    = SPI3_HOST;       // ESP32-S3 SPI3
cfg.spi_mode    = 0;               // CPOL=0, CPHA=0
cfg.freq_write  = 80000000;        // 80MHz 写时钟
cfg.freq_read   = 6000000;         // 6MHz 读时钟
cfg.spi_3wire   = true;            // 3 线 SPI (MOSI 兼 MISO)
cfg.dma_channel = SPI_DMA_CH_AUTO; // 自动 DMA
cfg.pin_sclk    = 14;
cfg.pin_mosi    = 13;
cfg.pin_miso    = -1;              // 不使用
cfg.pin_dc      = 11;

// 面板配置
cfg.pin_cs      = 10;
cfg.pin_rst     = 12;
cfg.pin_busy    = -1;
cfg.panel_width  = 240;
cfg.panel_height = 320;
cfg.invert       = true;           // 此屏需要反色
cfg.rgb_order    = false;          // BGR 顺序
```

#### DIJI-NES 使用的 TFT: 12-pin FPC 接口

> 来源：[立创开源 §已验证屏幕](https://oshwhub.com/uf-evan/project_gikrsdoj) 描述中的 FPC 引脚定义表

DIJI-NES 使用的 HD20001C12 屏幕采用 **12-pin FPC 排线 (0.5mm 间距)**，引脚顺序如下：

| FPC Pin | 信号              | 说明                   |
| ------- | ----------------- | ---------------------- |
| 1       | GND               | 地                     |
| 2       | LEDK              | 背光阴极 (LED Cathode) |
| 3       | LEDA              | 背光阳极 (LED Anode)   |
| 4       | VDD               | 逻辑电源 (3.3V)        |
| 5       | GND               | 地                     |
| 6       | GND               | 地                     |
| 7       | **DC (RS)** | 数据/命令选择          |
| 8       | **CS**      | 片选                   |
| 9       | **SCK**     | SPI 时钟               |
| 10      | **SDA**     | SPI 数据               |
| 11      | **RST**     | 硬件复位               |
| 12      | GND               | 地                     |

> ⚠️ **关键差异**：DIJI-NES 使用 **12-pin FPC**，本项目使用 **8-pin 2.54mm 排针**。虽然物理接口和引脚编号完全不同，但 ESP32-S3 端的 **GPIO 分配完全相同**、**驱动 IC 相同 (ST7789V2)**、**LovyanGFX 配置可完全复用**。

#### 本项目 TFT: 8-pin 排针 (对比)

| 本项目 Pin | 信号 | ESP32 GPIO | DIJI-NES FPC Pin |
| ---------- | ---- | ---------- | :--------------: |
| 1 (GND)    | GND  | GND        |     1,5,6,12     |
| 2 (VCC)    | 3.3V | 3.3V       |        4        |
| 3 (SCL)    | SCLK | GPIO14     |        9        |
| 4 (SDA)    | MOSI | GPIO13     |        10        |
| 5 (RES)    | RST  | GPIO12     |        11        |
| 6 (DC)     | DC   | GPIO11     |        7        |
| 7 (CS)     | CS   | GPIO10     |        8        |
| 8 (BLK)    | 背光 | 3.3V       | 2+3 (LEDK+LEDA) |

### 2.3 SD 卡接线 (FSPI 独立总线)

DIJI-NES 将 SD 卡放在独立的 **FSPI (SPI2)** 总线上，避免与 TFT 的 SPI3 相互干扰：

```cpp
// 来自 main.cpp 第 67~71 行
#define SD_CS_PIN     42
#define SD_SCLK_PIN   40
#define SD_MISO_PIN   39
#define SD_MOSI_PIN   41
#define SD_FREQ       10000000  // 10 MHz

// 初始化 — 使用独立 SPI 对象
SPIClass sdSPI(FSPI);
sdSPI.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
SD.begin(SD_CS_PIN, sdSPI, SD_FREQ);
```

```
ESP32-S3 (FSPI)                  MicroSD 模块
┌──────────────┐              ┌──────────────┐
│ GPIO42 (CS)  ├──────────────┤ CS            │
│ GPIO40 (SCLK)├──────────────┤ SCLK          │
│ GPIO39 (MISO)├──────────────┤ MISO (DO)     │
│ GPIO41 (MOSI)├──────────────┤ MOSI (DI)     │
│ 3.3V         ├──────────────┤ VCC           │
│ GND          ├──────────────┤ GND           │
└──────────────┘              └──────────────┘
```

#### SPI 总线隔离原理

```
ESP32-S3 SPI 拓扑:
┌─────────────────────────────────────────────────┐
│  SPI3_HOST (GPIO14,13,11,10) ──→ TFT ST7789    │
│  FSPI      (GPIO40,39,41,42) ──→ SD Card       │
│                                                 │
│  ★ 两个独立 SPI 控制器，各自有独立 DMA 通道      │
│  ★ SD 卡插拔/读写不会导致 TFT 画面闪烁           │
│  ★ 即使 SD 卡 SPI 重新初始化，也不影响 TFT       │
└─────────────────────────────────────────────────┘
```

> ⚠️ 如果 TFT 和 SD 卡共用一根 SPI 总线，每次 SD 卡读写前必须 `endTransaction()` 释放总线，之后 `beginTransaction()` 重新获取——频繁切换会导致 TFT 画面异常。

### 2.4 游戏手柄按键接线 (8 键)

DIJI-NES 支持 NES 经典 8 键布局，全部使用 GPIO 的 `INPUT_PULLUP` 模式：

```cpp
// 来自 main.cpp 第 74~82 行 + 第 409~428 行
#define A_BUTTON      48
#define B_BUTTON      47
#define LEFT_BUTTON   8
#define RIGHT_BUTTON  18
#define UP_BUTTON     17
#define DOWN_BUTTON   3
#define START_BUTTON  15
#define SELECT_BUTTON 16

void initializeButtons() {
    pinMode(A_BUTTON, INPUT_PULLUP);
    pinMode(B_BUTTON, INPUT_PULLUP);
    pinMode(LEFT_BUTTON, INPUT_PULLUP);
    pinMode(RIGHT_BUTTON, INPUT_PULLUP);
    pinMode(UP_BUTTON, INPUT_PULLUP);
    pinMode(DOWN_BUTTON, INPUT_PULLUP);
    pinMode(START_BUTTON, INPUT_PULLUP);
    pinMode(SELECT_BUTTON, INPUT_PULLUP);
}

void updateButtons() {
    buttons.A      = !digitalRead(A_BUTTON);      // 按下=0→反转=1
    buttons.B      = !digitalRead(B_BUTTON);
    buttons.LEFT   = !digitalRead(LEFT_BUTTON);
    buttons.RIGHT  = !digitalRead(RIGHT_BUTTON);
    buttons.UP     = !digitalRead(UP_BUTTON);
    buttons.DOWN   = !digitalRead(DOWN_BUTTON);
    buttons.START  = !digitalRead(START_BUTTON);
    buttons.SELECT = !digitalRead(SELECT_BUTTON);
}
```

#### 按键接线图

```
ESP32-S3                       NES 手柄 (DB9)
┌──────────────┐              ┌──────────────┐
│ GPIO48 (A)   ├──────────────┤ A 键 → GND   │
│ GPIO47 (B)   ├──────────────┤ B 键 → GND   │
│ GPIO8  (←)  ├──────────────┤ ← 键 → GND   │
│ GPIO18 (→)  ├──────────────┤ → 键 → GND   │
│ GPIO17 (↑)  ├──────────────┤ ↑ 键 → GND   │
│ GPIO3  (↓)  ├──────────────┤ ↓ 键 → GND   │
│ GPIO15 (Sta)├──────────────┤ START → GND   │
│ GPIO16 (Sel)├──────────────┤ SELECT → GND  │
│ GND         ├──────────────┤ 公共地        │
└──────────────┘              └──────────────┘

每个按键: ESP32 GPIO ←→ 微动开关 ←→ GND
按下 = GPIO 读到 0 (LOW)，松开 = 上拉读到 1 (HIGH)
代码中取反: buttons.X = !digitalRead(X) → 按下=1, 松开=0
```

#### 按键防抖

```cpp
static unsigned long lastButtonTime = 0;
static const unsigned long BUTTON_DEBOUNCE = 200;  // 200ms 防抖

// 在菜单输入处理中:
if (millis() - lastButtonTime > BUTTON_DEBOUNCE) {
    // 处理按键...
    lastButtonTime = millis();
}
```

### 2.5 I2S 音频输出接线 (MAX98357A)

DIJI-NES 使用 I2S 标准协议输出 NES APU 模拟音频到 MAX98357A I2S DAC 功放模块：

```cpp
// 来自 main.cpp 第 83~89 行
#define I2S_BCLK_PIN 5
#define I2S_LRCLK_PIN 4
#define I2S_DATA_PIN 6
constexpr int AUDIO_SAMPLE_RATE = 44100;
constexpr int I2S_NUM = 0;

// 初始化 (main.cpp 第 1115~1155 行)
void initializeAudio() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 44100,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .dma_buf_count = 4,
        .dma_buf_len = 256,
        .use_apll = false
    };
    i2s_driver_install((i2s_port_t)I2S_NUM, &i2s_config, 0, NULL);

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK_PIN,     // 位时钟
        .ws_io_num = I2S_LRCLK_PIN,     // 左右声道时钟
        .data_out_num = I2S_DATA_PIN,   // 音频数据
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    i2s_set_pin((i2s_port_t)I2S_NUM, &pin_config);
}
```

```
ESP32-S3 (I2S0)                  MAX98357A 模块
┌──────────────┐              ┌──────────────┐
│ GPIO5 (BCLK) ├──────────────┤ BCLK         │
│ GPIO4 (LRCLK)├──────────────┤ LRC          │
│ GPIO6 (DATA) ├──────────────┤ DIN          │
│ 3.3V         ├──────────────┤ VIN          │
│ GND          ├──────────────┤ GND          │
└──────────────┘              ├──────────────┤
                              │ SPK+ ──→ 喇叭│
                              │ SPK- ──→ 喇叭│
                              └──────────────┘
```

### 2.6 引脚冲突分析

| 冲突引脚         | 冲突外设                 | 影响                           | 解决方案                                   |
| ---------------- | ------------------------ | ------------------------------ | ------------------------------------------ |
| **GPIO48** | A_BUTTON ↔ WS2812       | 按键可能干扰 RGB LED 颜色      | 分时复用或优先分配；本项目不装手柄则无影响 |
| **GPIO3**  | DOWN_BUTTON ↔ Strapping | 上电时若 GPIO3 被拉低影响 JTAG | 外接下拉按键不影响上电时序 (已有上拉)      |

#### GPIO48 冲突深度分析

```
GPIO48 在 HW-678 上的双重身份:
┌────────────────────────────────────────────┐
│ 板载功能: WS2812 RGB LED (数据输入)         │
│ DIJI-NES:  A 键输入 (INPUT_PULLUP)          │
│                                            │
│ 冲突: 当读取按键时 digitalRead(GPIO48)     │
│        WS2812 的数据线会被短暂影响           │
│                                            │
│ 实际影响: 由于 WS2812 需要严格的时序信号     │
│         (800KHz)，普通 GPIO 读操作不会       │
│         产生有效 WS2812 数据，LED 可能闪烁    │
│                                            │
│ 建议: 如需同时使用 WS2812 和按键，          │
│       将 A 键移到其他空闲 GPIO (如 GPIO9)   │
└────────────────────────────────────────────┘
```

### 2.7 GPIO 占用完整视图

```
ESP32-S3 (HW-678) GPIO 占用全景:

GPIO#  0         1         2         3
  │      │         │         │         │
  │  BOOT(按钮)  (空闲)   (空闲)   DOWN键*
  │
  4         5         6         7         8
  │         │         │         │         │
LRCLK     BCLK      DATA     (空闲)   LEFT键
  │                                                                              │
  9         10        11        12        13       14       15
  │         │         │         │         │        │        │
(空闲)    TFT_CS   TFT_DC   TFT_RST  TFT_MOSI TFT_SCLK  START键
  │
  16        17        18
  │         │         │
SELECT     UP键    RIGHT键

  39        40        41        42
  │         │         │         │
SD_MISO  SD_SCLK  SD_MOSI   SD_CS

  47        48
  │         │
 B键      A键 (⚠️冲突)

图例:
TFT   = SPI3 显示屏 (5 根)
SD    = FSPI SD卡  (4 根)
按键  = 8 个游戏按键
I2S   = 音频输出  (3 根)
空闲  = 未使用

共占用: 5(TFT) + 4(SD) + 8(按键) + 3(I2S) = 20 个 GPIO
HW-678 可用 GPIO: 36 个 (N16R8 型号中 GPIO35/36/37 被 Octal PSRAM 独占)
剩余可用: ~16 个
```

### 2.8 移植到本项目的硬件建议

如果要从 DIJI-NES 移植外设到当前 Demo 项目，推荐优先级：

| 优先级    | 外设     | 难度 | 说明                                         |
| --------- | -------- | ---- | -------------------------------------------- |
| ✅ 已有   | TFT      | -    | 接线和驱动完全一致                           |
| 🔧 容易   | SD 卡    | 低   | 加 4 根杜邦线 + SPI 代码即可                 |
| 🔧 容易   | 按键     | 低   | 每个按键只需 1 根 GPIO + GND，任意 GPIO 均可 |
| ⚠️ 中等 | I2S 音频 | 中   | 需要 MAX98357A 模块 + 喇叭，代码较复杂       |
| ⚠️ 注意 | WS2812   | 低   | 与 A 键冲突，若不接手柄则无影响              |

### 2.9 DIJI-NES vs 本项目硬件差异总表

> 来源：对比 [GitHub §硬件需求](https://github.com/UF-Evan/DIJI-NES#%EF%B8%8F-%E7%A1%AC%E4%BB%B6%E9%9C%80%E6%B1%82) + [立创 BOM](https://oshwhub.com/uf-evan/project_gikrsdoj) 与 `main/lgfx_conf.h`

| 对比维度              | DIJI-NES                                   | 本项目 (TFT-LCD Demo)            | 兼容性          |
| --------------------- | ------------------------------------------ | -------------------------------- | --------------- |
| **主控**        | ESP32-S3-WROOM-1-N16R8                     | HW-678ABCD (ESP32-S3-WROOM-1)    | ✅ 同系列       |
| **PCB**         | 定制 PCB (立创 EDA)                        | 面包板 + 杜邦线                  | 🟡 接线可参考   |
| **TFT 型号**    | HD20001C12 (华迪创显)                      | KAC-N200-2432KHWIG20-A8 (可爱橙) | ⚠️ 不同厂商   |
| **TFT 接口**    | 12-pin FPC (0.5mm)                         | 8-pin 2.54mm 排针                | ⚠️ 物理不兼容 |
| **TFT 驱动 IC** | ST7789V2                                   | ST7789V2                         | ✅ 完全一致     |
| **TFT 分辨率**  | 240×320                                   | 240×320                         | ✅ 完全一致     |
| **TFT GPIO**    | 14,13,11,10,12                             | 14,13,11,10,12                   | ✅ 完全一致     |
| **SD 卡**       | MicroSD (FSPI: 42,40,39,41)                | 无                               | 🔧 可添加       |
| **按键**        | 8 键直连 GPIO                              | 无                               | 🔧 可添加       |
| **音频**        | MAX98357A (I2S0: 5,4,6)                    | 无                               | 🔧 可添加       |
| **供电**        | 3.7V 锂电池 + TP4056 + Type-C              | USB Type-C 直供                  | 🟡 供电方式不同 |
| **外壳**        | 3D 打印 (STL 已开源)                       | 无                               | -               |
| **固件框架**    | Arduino (PlatformIO)                       | ESP-IDF v6.0.1 (原生)            | 🟡 框架不同     |
| **图形库**      | LovyanGFX v1.2.7                           | LovyanGFX (本地组件)             | ✅ 同一个库     |
| **SPI 配置**    | SPI3_HOST, 80MHz, mode 0, invert=true, BGR | 同上                             | ✅ 完全一致     |

> 💡 **核心结论**：虽然硬件形态差异很大（掌机 PCB vs 面包板 Demo），但 **TFT 驱动层面的配置参数和 LovyanGFX 代码可 100% 复用**。如果后续添加 SD 卡、按键、音频等外设，DIJI-NES 的 SPI 总线隔离和 GPIO 分配方案是直接可参考的最佳实践。

---

## 3. LovyanGFX 配置 (已验证可用)

```cpp
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789  _panel_instance;
    lgfx::Bus_SPI       _bus_instance;

public:
    LGFX(void) {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host    = SPI3_HOST;       // ESP32-S3 SPI3
            cfg.spi_mode    = 0;               // CPOL=0, CPHA=0
            cfg.freq_write  = 80000000;        // 80MHz 写时钟
            cfg.freq_read   = 6000000;         // 6MHz 读时钟
            cfg.spi_3wire   = true;            // 3 线 SPI
            cfg.use_lock    = false;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk    = 14;
            cfg.pin_mosi    = 13;
            cfg.pin_miso    = -1;
            cfg.pin_dc      = 11;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs      = 10;
            cfg.pin_rst     = 12;
            cfg.pin_busy    = -1;
            cfg.panel_width  = 240;
            cfg.panel_height = 320;
            cfg.offset_x     = 0;
            cfg.offset_y     = 0;
            cfg.invert       = true;    // ★ 关键：此屏需要反色
            cfg.rgb_order    = false;   // BGR 顺序
            cfg.dlen_16bit   = false;
            cfg.bus_shared   = false;
            _panel_instance.config(cfg);
        }
        setPanel(&_panel_instance);
    }
};
```

> ✅ 本项目 `main/lgfx_conf.h` 中的配置与此**完全一致**。

---

## 4. FreeRTOS 双核任务架构

### 任务分配

```
┌──────────────────────────────────────────────────────┐
│                    ESP32-S3                          │
│                                                      │
│  Core 0 ─── display_task() ─── DMA 推屏到 TFT        │
│         ─── APU I2S 音频输出                         │
│                                                      │
│  Core 1 ─── loop() ─── CPU 模拟 + PPU 渲染            │
│         ─── 按键扫描                                  │
│                                                      │
│  Core 间通信: FreeRTOS Queue (frame_queue)            │
└──────────────────────────────────────────────────────┘
```

### 关键代码

```cpp
// 初始化时创建任务
xTaskCreatePinnedToCore(display_task, "display", 4096, NULL, 1, NULL, 0);

// Core 1 渲染完一帧后
uint8_t idx = render_buf_idx;
xQueueSend(frame_queue, &idx, 0);
render_buf_idx = 1 - render_buf_idx; // 切换缓冲

// Core 0 等待渲染完成的帧
void display_task(void* arg) {
    uint8_t buf_idx;
    for (;;) {
        xQueueReceive(frame_queue, &buf_idx, portMAX_DELAY);
        // DMA 推屏 ...
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
```

---

## 5. 双缓冲渲染管线

### 内存布局

```
frame_buf[0]  ── 256×240×2 = 122,880 字节 (DMA 内存)
frame_buf[1]  ── 256×240×2 = 122,880 字节 (DMA 内存)
display_crop_buf ── 248×60×2 = 29,760 字节 (DMA 裁剪缓冲)
```

### 分配方式

```cpp
frame_buf[0] = (uint16_t*)heap_caps_malloc(
    256 * 240 * sizeof(uint16_t),
    MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL
);
```

> ⚠️ 必须用 `MALLOC_CAP_DMA` 确保内存在 DMA 可访问区域（内部 SRAM，非 PSRAM）。

### 数据流

```
PPU 渲染 → frame_buf[render_idx]
    │
    ├─ 裁剪 (裁掉 4px 边缘) → display_crop_buf
    │
    └─ DMA 推送 → TFT (60 行/块，共 4 块)
```

---

## 6. 帧率优化策略

### 6.1 奇数周期自适应跳帧

```cpp
if (ENABLE_FRAMESKIP && force_render_frames == 0) {
    frameskip_phase = (frameskip_phase + 1) & 1;
    if (frameskip_phase == 0) {
        ppu.renderedThisFrame = false; // 跳帧
        return;
    }
}
```

> 使用奇数周期跳帧避免与游戏内 2 帧闪烁动画锁相。

### 6.2 IRAM 热路径

```cpp
IRAM_ATTR void renderBackgroundLine() { ... }
IRAM_ATTR void renderSpriteLine() { ... }
IRAM_ATTR void checkSprite0HitFast() { ... }
```

> 将热点函数放入 IRAM 以减少 Flash 缓存缺失。

### 6.3 性能指标

| 场景          | FPS     |
| ------------- | ------- |
| 大部分游戏    | 57~61   |
| 重精灵场景    | 55~58   |
| 目标帧率      | 60      |
| 单帧 DMA 耗时 | ~1-3 ms |

---

## 7. SPI 总线隔离策略

> 来源：[GitHub README §SD 卡](https://github.com/UF-Evan/DIJI-NES#sd-%E5%8D%A1) 引脚表 + `main.cpp` 第 67~71 行。引脚接线详见 [§2.3](#23-sd-卡接线-fspi-独立总线)。

DIJI-NES 同时使用 TFT 和 SD 卡，将它们放在**不同 SPI 总线上**以避免冲突：

SD 卡使用独立 SPI 对象：

```cpp
SPIClass sdSPI(FSPI);
sdSPI.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
SD.begin(SD_CS_PIN, sdSPI, 10000000);
```

> 💡 本 Demo 项目不使用 SD 卡，只需 SPI3_HOST 即可。

---

## 8. I2S 音频输出

> 来源：[GitHub README §I2S 音频](https://github.com/UF-Evan/DIJI-NES#i2s-%E9%9F%B3%E9%A2%91) + [立创硬件参数](https://oshwhub.com/uf-evan/project_gikrsdoj)。接线详见 [§2.5](#25-i2s-音频输出接线-max98357a)。

DIJI-NES 使用 I2S DAC (MAX98357A) 输出 NES APU 音频，关键配置：

```cpp
#define I2S_BCLK_PIN 5
#define I2S_LRCLK_PIN 4
#define I2S_DATA_PIN 6
constexpr int AUDIO_SAMPLE_RATE = 44100;

// 配置
i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    // ...
};
```

> 本项目不涉及音频，此部分仅供扩展参考。

---

## 9. 按键输入

> 来源：[GitHub README §控制器按键](https://github.com/UF-Evan/DIJI-NES#%E6%8E%A7%E5%88%B6%E5%99%A8%E6%8C%89%E9%94%AE) 引脚表。完整接线和冲突分析详见 [§2.4](#24-游戏手柄按键接线-8-键) 和 [§2.6](#26-引脚冲突分析)。

DIJI-NES 使用 8 个 GPIO 作为游戏手柄按键（INPUT_PULLUP），按键防抖 200ms：

```cpp
// 初始化
pinMode(A_BUTTON, INPUT_PULLUP); // ...其余 7 个同上

// 读取 (按下=GND, 因此取反)
buttons.A = !digitalRead(A_BUTTON);
```

> ⚠️ **注意**：`A_BUTTON` (GPIO48) 与 HW-678 板载 WS2812 共用，详见 §2.6。

---

## 10. 编译参数 (PlatformIO)

```ini
[env:esp32s3-n16r8]
platform = espressif32@6.10.0
board = esp32-s3-devkitc-1
framework = arduino
lib_deps = lovyan03/LovyanGFX@^1.2.7
board_build.flash_mode = qio
board_build.arduino.memory_type = qio_opi
board_build.psram_type = opi
board_upload.flash_size = 16MB
board_build.partitions = default_16MB.csv
build_flags = -Ofast -funroll-loops -fstrict-aliasing
```

对应 ESP-IDF sdkconfig：

```ini
CONFIG_ESP32S3_SPIRAM_SUPPORT=y
CONFIG_SPIRAM_MODE_OCT=y       # Octal PSRAM
CONFIG_SPIRAM_SIZE=8388608     # 8MB
CONFIG_ESP32S3_DEFAULT_CPU_FREQ_240=y
```

---

## 11. 关键技术总结

| 技术点             | DIJI-NES 方案                              | 本项目应用             |
| ------------------ | ------------------------------------------ | ---------------------- |
| **图形库**   | LovyanGFX v1.2.7                           | LovyanGFX (本地组件)   |
| **SPI 配置** | SPI3_HOST, 80MHz, mode 0, invert=true, BGR | ✅ 完全一致            |
| **双缓冲**   | 2× frame_buf + crop_buf                   | 暂不需要 (静态画面)    |
| **DMA 推屏** | 60 行/块，共 4 次 DMA/帧                   | LovyanGFX 内部自动 DMA |
| **SPI 隔离** | TFT=SPI3, SD=FSPI                          | 仅 TFT，无冲突         |
| **双核分工** | Core0→显示+音频, Core1→模拟              | 单核足够 (Demo)        |
| **帧率优化** | 奇数周期跳帧 + IRAM 热路径 + 查表          | 不需要 (非实时渲染)    |

---

## 12. 版本历史关键节点

| 版本   | 日期       | 关键变更                                               |
| ------ | ---------- | ------------------------------------------------------ |
| v0.2.0 | 2026-03-24 | 48→60 FPS 性能突破 (CPU 赤字追踪、OAM 预评估、IRAM)   |
| v0.3.0 | 2026-04-26 | 奇数周期自适应跳帧、4px overscan 裁边                  |
| v0.4.0 | 2026-05-28 | UTF-8 中文文件名支持、USB CDC 默认启用                 |
| v0.5.0 | 2026-06-18 | DIJI-NES logo 粒子动画、5 格图形音量控制、APU 音频改善 |

> 完整 Changelog: [CHANGELOG.md](https://github.com/UF-Evan/DIJI-NES/blob/main/CHANGELOG.md)

---

## 13. 立创开源硬件平台项目详情

> 来源：[立创开源硬件平台 - DIJI-NES](https://oshwhub.com/uf-evan/project_gikrsdoj)

DIJI-NES 不仅是一个软件项目，更是一个**完整的开源硬件工程**。在立创开源硬件平台上，作者公开了 PCB 设计、BOM、原理图和装配说明。

### 13.1 项目定位

```
"DIJI-NES 是一款基于 ESP32-S3 的开源 NES 掌机项目。
 从面包板验证开始，逐步演进为完整 PCB 设计，适合作为：
 • ESP32 学习项目
 • NES 模拟器学习项目
 • 开源掌机项目
 • PCB 设计与焊接练手项目"
```

— [立创项目描述](https://oshwhub.com/uf-evan/project_gikrsdoj)

### 13.2 PCB 关键信息

| 项目         | 说明                                                                                       |
| ------------ | ------------------------------------------------------------------------------------------ |
| 设计工具     | 立创 EDA 专业版 ([在线编辑](https://pro.lceda.cn/editor#id=9b26471bf5db4fc1b1c64a73d2c92466)) |
| 原理图       | `SCH_Schematic1_1_1-P1_2026-06-09.png`                                                   |
| PCB Layout   | `PCB_PCB1_2026-06-09.jpg`                                                                |
| FPC 连接器   | **AFC01-S12FCA-00** (12-pin, 0.5mm, 下接)                                            |
| USB-C 连接器 | **MC-107SSY** (HANBO 汉博)                                                           |
| ESD 保护     | **SMBJ6.5CA** (R+O 宏嘉诚)                                                           |
| CC 电阻      | **5.1kΩ 0603** ×2 (USB PD 识别)                                                    |
| 充电指示灯   | **NCD0805R1** 红色 LED (国星光电)                                                    |

### 13.3 3D 打印外壳

| 项目     | 说明                                                                     |
| -------- | ------------------------------------------------------------------------ |
| 材料     | FDM 3D 打印 (推荐 PLA / PETG)                                            |
| 文件     | STL 已开源 ([立创 3D 模型库](https://model.jlc-3dp.cn/modelDetail/265d...)) |
| 紧固件   | M2.5 六角螺母 ×4 + M2.5×8mm 十字盘头螺丝 ×4                           |
| 组装顺序 | 屏幕→前壳 → PCB 入壳 → 螺母嵌入后壳 → 合壳 → 锁螺丝                 |

> ⚠️ 请勿使用过长螺丝，以免顶压 PCB 或损坏屏幕。不同材料 (PETG/ABS) 因收缩率不同，螺母孔可能需要修整。

### 13.4 供电方案 (TP4056 锂电池充电)

DIJI-NES 的电源子系统（从原理图推断）：

```
Type-C (5V) ──→ TP4056 ──→ 3.7V Li-Po ──→ LDO ──→ 3.3V
                  │                          │
                  └─ 充电指示灯 LED          └─→ ESP32-S3 + TFT + SD + MAX98357A
```

| 组件        | 功能              |
| ----------- | ----------------- |
| TP4056      | 锂电池充电管理 IC |
| 3.7V 锂电池 | 主供电            |
| LDO         | 降压至 3.3V       |
| Type-C      | 充电 + 程序烧录   |

### 13.5 社区互动数据

| 指标     | 数值  |
| -------- | ----- |
| 点赞     | 866   |
| 收藏     | 9     |
| 分享     | 17    |
| 评论     | 19 条 |
| 工程成员 | 1 人  |

### 13.6 社区常见问题 (来自立创评论区)

| 问题                          | 作者回复                                                |
| ----------------------------- | ------------------------------------------------------- |
| ROM/游戏文件去哪下载？        | 由于版权原因需自行寻找资源 (GitHub 上搜 nesRoms)        |
| 不装游戏 SD 卡认不到？        | 是的，装进去游戏就可以了                                |
| BOM 文件下载不了 / 位置不对？ | 重新导出 BOM 确认 (部分因版本更新导致位置变化)          |
| 原理图有飞线警告？            | 已修复 (发布时改电阻导致 VCC 和 A 引脚意外连接)         |
| 固件和源码 GitHub 打不开？    | 已同步到 Gitee 镜像: https://gitee.com/UF-Evan/DIJI-NES |

### 13.7 演示视频

- [B站视频 — DIJI-NES 功能演示及介绍](https://www.bilibili.com/video/BV1BQV26cEu8/)

---

## 14. 信息来源汇总

本文档所有分析均基于以下一手来源：

| # | 来源             | 类型                   | URL                                              |
| - | ---------------- | ---------------------- | ------------------------------------------------ |
| 1 | GitHub 仓库      | 源码 + README          | https://github.com/UF-Evan/DIJI-NES              |
| 2 | 立创开源硬件平台 | PCB + BOM + 原理图     | https://oshwhub.com/uf-evan/project_gikrsdoj     |
| 3 | Gitee 镜像       | 源码 + 固件            | https://gitee.com/UF-Evan/DIJI-NES               |
| 4 | 本地副本         | `DIJI-NES-main/src/` | `d:\EE\project\ESP-IDF\TFT-LCD\DIJI-NES-main\` |
| 5 | B站视频          | 功能演示               | https://www.bilibili.com/video/BV1BQV26cEu8/     |
