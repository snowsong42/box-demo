# TFT-LCD 显示屏完整规格书

> 来源：可爱橙 KAC-N200-2432KHWIG20-A8 产品规格书 v1.0 (2025-09-06) + ST7789V2 芯片手册 + STM32 例程代码

---

## 1. 产品标识

| 项目 | 内容 |
| ---- | ---- |
| 制造商型号 | **KAC-N200-2432KHWIG20-A8** |
| 品牌 | 可爱橙科技 (Cute Orange Technology) |
| 版本 | v1.0 |
| 规格书日期 | 2025-09-06 |
| 淘宝店铺 | shop202389220.taobao.com |

---

## 2. 核心参数

| 参数 | 规格 |
| ---- | ---- |
| 屏幕尺寸 | **2.0 英寸** |
| 显示模式 | **IPS** (全视角) / Normally Black |
| 分辨率 | **240 (H) RGB × 320 (V)** |
| 像素间距 | 0.1275 mm × 0.1275 mm |
| 有效显示区域 | 30.6 mm (H) × 40.8 mm (V) |
| 玻璃外形尺寸 | 35.7 mm (H) × 51.2 mm (V) × 2.2 mm (D) |
| FPC/PCB 组装尺寸 | 63.2 mm (H) × 29 mm (V) × 3.0 mm (MAX) |
| 像素排列 | RGB 垂直条纹 (Vertical Stripe) |
| 驱动芯片 | **Sitronix ST7789V2** |
| 接口 | **4 线 SPI** (4-Line SPI) |
| 颜色数 | **65K** (RGB565, 16-bit) |
| 接口引脚间距 | 2.54 mm（标准排针） |
| 连接器 | 8-pin 直插 |

---

## 3. 电学特性

### 3.1 绝对最大额定值 (Absolute Maximum)

| 参数 | 符号 | 最小值 | 最大值 | 单位 |
| ---- | ---- | ------ | ------ | ---- |
| 逻辑供电电压 | VCC | 3.0 | 3.6 | V |
| IO 供电电压 | IOVCC | 3.0 | 3.6 | V |
| 输入电压 | Vi | -0.3 | IOVDD+0.3 | V |
| 工作温度 | T_op | -20 | +70 | °C |
| 存储温度 | T_stg | -30 | +80 | °C |

### 3.2 DC 电气特性

| 参数 | 符号 | 最小值 | 典型值 | 最大值 | 单位 |
| ---- | ---- | ------ | ------ | ------ | ---- |
| 供电电压 | VCC | 3.0 | **3.3** | 3.6 | V |
| 逻辑低输入 | VIL | -0.3·IOVDD | — | 0.3·IOVDD | V |
| 逻辑高输入 | VIH | 0.7·IOVDD | — | IOVDD | V |
| 逻辑低输出 | VOL | — | — | 0.2·IOVDD | V |
| 逻辑高输出 | VOH | 0.8·IOVDD | — | — | V |
| 正常显示电流 | I_vdd | — | **50** | — | mA |
| 待机电流 | I_vdd_stby | — | **20** | — | µA |
| 帧率 | f_FR | — | **60** | — | Hz |

---

## 4. 光学特性

| 参数 | 条件 | 最小值 | 典型值 | 最大值 | 单位 |
| ---- | ---- | ------ | ------ | ------ | ---- |
| 表面亮度 (背光 20mA) | 正视 | 600 | **650** | — | cd/m² |
| 亮度 (产品手册值) | — | — | **400** | — | cd/m² |
| 对比度 | 中心点 | — | **600:1** | — | — |
| 响应时间 (Tr) | 10%→90% | — | 10 | 20 | ms |
| 响应时间 (Tf) | 90%→10% | — | 20 | 30 | ms |
| 可视角度 | 上下左右四方向 | — | **80°** | — | Deg |
| NTSC 色域 | — | — | **60%** | — | % |

### 4.1 色彩坐标 (CIE1931)

| 颜色 | x (typ) | y (typ) |
| ---- | ------- | ------- |
| 红 (Red) | 0.614 | 0.290 |
| 绿 (Green) | 0.270 | 0.540 |
| 蓝 (Blue) | 0.104 | 0.097 |
| 白 (White) | 0.267 | 0.302 |

---

## 5. 引脚定义

| 引脚 | 符号 | I/O | 功能说明 |
| ---- | ---- | --- | -------- |
| 1 | **GND** | P | 逻辑电路地。必须接外部地 |
| 2 | **VCC** | P | 逻辑电源。必须接外部电源 (3.3V) |
| 3 | **SCL** | I | 串行时钟输入 (SPI Clock) |
| 4 | **SDA** | I | 串行数据输入 (SPI MOSI) |
| 5 | **RES** | I | 硬件复位。**低电平有效**，正常工作时保持高电平 |
| 6 | **DC** | I | 数据/命令选择。**高=数据，低=命令** |
| 7 | **CS** | I | 片选。**低电平有效**，拉低后芯片使能 |
| 8 | **BLK** | P | 背光控制。**高电平=亮，低电平=灭** |

> P = Power, I = Input

---

## 6. 背光参数

| 参数 | 规格 |
| ---- | ---- |
| 类型 | 白色 LED × 4 (并联) |
| 驱动方式 | 直连 3.3V 或 GPIO 控制 |
| 亮度 | 可通过 PWM 调节（如需） |

---

## 7. 环境参数

| 参数 | 范围 |
| ---- | ---- |
| 工作温度 | **-20°C ~ +70°C** |
| 存储温度 | **-30°C ~ +80°C** |
| 存储湿度 | 10% ~ 90% RH (≤50°C); ≤60% RH (>60°C) |
| 工作湿度 | 10% ~ 90% RH |
| RoHS | ✅ 合规 |
| 重量 | 约 **7.9 g** |

---

## 8. 可靠性测试

| 测试项目 | 条件 | 时长 |
| -------- | ---- | ---- |
| 高温高湿运行 | +40°C, 90% RH | 96 小时 |
| 高温运行 | +70°C | 96 小时 |
| 低温运行 | -20°C | 96 小时 |
| 热冲击 | -20°C (30min) ↔ +70°C (30min) | 10 循环 |
| ESD (接触) | 150pF, 330Ω, ±2KV | 10 次 |
| ESD (空气) | 150pF, 330Ω, ±6KV | 10 次 |

---

## 9. ST7789V2 初始化序列

> 以下序列经过 STM32 例程验证，移植到 ESP32-S3 可直接复用

```c
// === 阶段 1: 硬件复位 ===
RES = 0;  延时 100ms;
RES = 1;  延时 100ms;
BLK = 1;  // 开背光

// === 阶段 2: 寄存器配置 ===
WriteCMD(0x11);  延时 120ms;          // 退出睡眠

WriteCMD(0x36);  WriteData(0x00);     // MADCTL: 显示方向
// 0x00 = 竖屏正常 | 0xC0 = 180°旋转
// 0x70 = 横屏正常 | 0xA0 = 横屏 180°

WriteCMD(0x3A);  WriteData(0x05);     // 像素格式: RGB565 (16bit)

WriteCMD(0xB2);                        // Porch 控制
WriteData(0x0C); WriteData(0x0C);
WriteData(0x00); WriteData(0x33); WriteData(0x33);

WriteCMD(0xB7);  WriteData(0x35);     // 栅极控制
WriteCMD(0xBB);  WriteData(0x32);     // VCOM = 1.35V
WriteCMD(0xC2);  WriteData(0x01);     // LCM 控制
WriteCMD(0xC3);  WriteData(0x15);     // VRH: GVDD = 4.8V
WriteCMD(0xC4);  WriteData(0x20);     // VDV = 0V
WriteCMD(0xC6);  WriteData(0x0F);     // 帧率 60Hz

WriteCMD(0xD0);                        // 电源控制
WriteData(0xA4); WriteData(0xA1);

// Gamma 正极性 (PVGAM)
WriteCMD(0xE0);
WriteData(0xD0); WriteData(0x08); WriteData(0x0E);
WriteData(0x09); WriteData(0x09); WriteData(0x05);
WriteData(0x31); WriteData(0x33); WriteData(0x48);
WriteData(0x17); WriteData(0x14); WriteData(0x15);
WriteData(0x31); WriteData(0x34);

// Gamma 负极性 (NVGAM)
WriteCMD(0xE1);
WriteData(0xD0); WriteData(0x08); WriteData(0x0E);
WriteData(0x09); WriteData(0x09); WriteData(0x15);
WriteData(0x31); WriteData(0x33); WriteData(0x48);
WriteData(0x17); WriteData(0x14); WriteData(0x15);
WriteData(0x31); WriteData(0x34);

WriteCMD(0x21);  // 反显关闭
WriteCMD(0x29);  // 开启显示
```

---

## 10. 画图命令流程

```
设置列地址:  WriteCMD(0x2A)  WriteData16(x_start)  WriteData16(x_end)
设置行地址:  WriteCMD(0x2B)  WriteData16(y_start)  WriteData16(y_end)
写入像素:    WriteCMD(0x2C)  WriteData16(color1)  WriteData16(color2) ...
```

---

## 11. 颜色宏定义 (RGB565)

| 宏名 | 16进制值 | 颜色 |
| ---- | -------- | ---- |
| `WHITE` | 0xFFFF | 白 |
| `BLACK` | 0x0000 | 黑 |
| `RED` | 0xF800 | 红 |
| `GREEN` | 0x07E0 | 绿 |
| `BLUE` | 0x001F | 蓝 |
| `YELLOW` | 0xFFE0 | 黄 |
| `CYAN` | 0x7FFF | 青 |
| `MAGENTA` | 0xF81F | 品红 |
| `GRAY` | 0x8430 | 灰 |
| `DARKBLUE` | 0x01CF | 深蓝 |
| `LIGHTBLUE` | 0x7D7C | 浅蓝 |

---

## 12. 坐标系统

```
        (0,0) ────→ X+ (240)
          │ ┌──────────┐
          │ │          │
       Y+ │ │  Display │
          │ │  240×320 │
          ↓ │          │
        (320)└──────────┘
```

- 原点 (0, 0) = 屏幕**左上角**
- X 轴：向右增加 (0 ~ 239)
- Y 轴：向下增加 (0 ~ 319)
- 旋转方向通过 MADCTL 寄存器 (`0x36`) 配置

---

## 13. ESP32 ↔ TFT 连线汇总

| ESP32 GPIO | TFT 引脚 | 信号 |
| ---------- | -------- | ---- |
| GPIO14 | Pin 3 (SCL) | SPI 时钟 |
| GPIO13 | Pin 4 (SDA) | SPI 数据 |
| GPIO12 | Pin 5 (RES) | 硬件复位 |
| GPIO11 | Pin 6 (DC) | 数据/命令 |
| GPIO10 | Pin 7 (CS) | 片选 |
| 3.3V | Pin 2 (VCC) | 逻辑电源 |
| 3.3V | Pin 8 (BLK) | 背光 |
| GND | Pin 1 (GND) | 地 |

---

## 14. LovyanGFX 配置 ↔ ST7789 寄存器对照

本项目使用 LovyanGFX 驱动 ST7789，以下对照表帮助理解每个配置参数的硬件含义：

| LovyanGFX 参数                     | 对应 ST7789 行为                    | 本项目值      | 说明                          |
| ---------------------------------- | ----------------------------------- | ------------- | ----------------------------- |
| `cfg.spi_mode = 0`                 | CPOL=0, CPHA=0 (SPI 模式 0)         | 0             | ST7789 标准时序               |
| `cfg.freq_write = 80000000`        | SPI 时钟频率                        | 80 MHz        | 最大允许值，DMA 传输用        |
| `cfg.panel_width = 240`            | 列地址范围 0~239                    | 240           | 硬件分辨率                    |
| `cfg.panel_height = 320`           | 行地址范围 0~319                    | 320           | 硬件分辨率                    |
| `cfg.invert = true`                | INVON (0x21) 反色使能               | **true**  | ⚠️ 此屏不反色会显示异常     |
| `cfg.rgb_order = false`            | MADCTL BGR 位                       | **false** | BGR 顺序，RGB 会颜色偏绿      |
| `cfg.dummy_read_pixel = 8`         | 假读周期数                          | 8             | 读取像素时序补偿              |
| `tft.setRotation(1)`               | MADCTL (0x36) 寄存器                | 1 (横屏)      | 顺时针 90° 旋转              |
| `tft.setBrightness(255)`           | 不影响 ST7789 (无硬背光 PWM)        | -             | 仅逻辑标记，背光已直连 3.3V   |

### 出厂初始化序列 (LovyanGFX 内置)

LovyanGFX 的 `Panel_ST7789` 类在 `init()` 时自动发送以下命令序列：

```
SLPOUT (0x11) → 延时 130ms        # 退出睡眠
COLOR_MODE (0x3A) → 0x55         # 16bit/pixel (RGB565)
GCTRL (0xB7) → 0x35               # 栅极控制
VCOMS (0xBB) → 0x28               # VCOM 设置
LCMCTRL (0xC0) → 0x0C             # LCM 控制
VDVVRHEN (0xC2) → 0x01,0xFF       # VDV/VRH 使能
VRHS (0xC3) → 0x10                # VRH 设置
VDVSET (0xC4) → 0x20              # VDV 设置
PWCTRL1 (0xD0) → 0xA4,0xA1        # 电源控制
RAMCTRL (0xF0) → 0x00,0xC0        # RAM 控制
INVON (0x21)  ← 由 invert=true 决定
DISPON (0x29)                      # 开显示
```

> ⚠️ **注意**：以上是 LovyanGFX 默认初始化序列，与 STM32 例程的初始化序列略有不同（例如 VCOM 值 = 0x28 vs 0x32，Gamma 值也不同）。**已验证 LovyanGFX 默认序列在此屏上显示正常**，无需手动覆盖。

---

## 15. DIJI-NES 渲染流水线 (可复用模式)

DIJI-NES 实现了高效的 60FPS 游戏画面渲染，其流水线可供本项目参考：

### 视口裁剪

```
原始 NES 画面:  256×240
裁剪 (OVERCAN): 248×240  (左右各裁 4px)
TFT 显示区域:   320×240 → 留左右各 32px 黑边居中
```

### DMA 分块策略

```
240 行 → 每 60 行一块 → 共 4 次 DMA/帧
每次 DMA: tft.setAddrWindow() + tft.pushPixelsDMA() + tft.waitDMA()
```

### 关键代码模式

```cpp
// 分块 DMA 推送（DIJI-NES display_task）
tft.startWrite();
for (int baseY = 0; baseY < SCREEN_HEIGHT; baseY += 60) {
    int h = min(60, SCREEN_HEIGHT - baseY);
    tft.setAddrWindow(offsetX, baseY, displayW, h);
    tft.pushPixelsDMA(cropBuf, displayW * h);
    tft.waitDMA();
}
tft.endWrite();
vTaskDelay(pdMS_TO_TICKS(2)); // 让出 CPU0 时间片
```

> 💡 本项目 Demo 为静态画面，不需要此高帧率流水线。若后续开发动画/游戏，可直接复用此模式。

---

## 16. 已知问题与调试经验

| 问题                                 | 原因                     | 解决方法                          |
| ------------------------------------ | ------------------------ | --------------------------------- |
| 屏幕全白 / 颜色异常                  | `invert` 未设为 `true`   | `cfg.invert = true`               |
| 颜色偏绿/偏蓝                        | RGB/BGR 顺序错误         | `cfg.rgb_order = false` (BGR)     |
| 烧录后屏幕无显示                     | 未按 RST 按钮            | 按板子上的 EN/RST 按钮复位        |
| 文本不在预期位置                     | `setTextDatum` 未设      | 使用 `textdatum_t::middle_center` |
| SPI 总线冲突 (与 SD 卡共用时)        | TFT 和 SD 共用 SPI 总线  | 参照 DIJI-NES 用独立 SPI 总线     |
| `%d` 格式化 `int32_t` 报 warning     | Xtensa 上 int32_t = long | 使用 `%ld` 替代 `%d`              |
