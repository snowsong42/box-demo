# 硬件连线说明

> 来源：项目 README.md + 可爱橙 STM32 例程 `lcd_init.h`

## ESP32 → TFT-LCD (2.0英寸 8-PIN SPI) 引脚对应表

| ESP32 引脚 | TFT 引脚 | 信号名称 | 功能说明 |
|-----------|----------|---------|---------|
| GPIO14 | SCL (SCLK) | SPI Clock | SPI总线时钟信号 |
| GPIO13 | SDA (MOSI) | SPI MOSI | SPI主机输出 / 从机输入（数据线） |
| GPIO12 | RES | Reset | 显示屏硬件复位，低电平有效 |
| GPIO11 | DC | Data/Command | 数据/命令选择：低=命令，高=数据 |
| GPIO10 | CS | Chip Select | 片选信号，低电平有效 |
| VCC (3.3V) | BLK | Backlight | 背光控制，接高电平点亮 |
| GND | GND | Ground | 电源地 |
| 3.3V | VCC | Power | 3.3V 电源供电 |

## 连接示意图

```
ESP32                    TFT-LCD (8-PIN SPI)
+-----+                  +------------------+
|     | GPIO14(SCL) ---> | SCL (SPI Clock)  |
|     | GPIO13(SDA) ---> | SDA (SPI MOSI)   |
|     | GPIO12      ---> | RES (Reset)      |
|     | GPIO11      ---> | DC (Data/Cmd)    |
|     | GPIO10      ---> | CS (Chip Select) |
|     | 3.3V        ---> | VCC + BLK        |
|     | GND         ---> | GND              |
+-----+                  +------------------+
```

## STM32 例程中的引脚对照（参考）

| STM32F103RC 引脚 | TFT 引脚 | 说明 |
|-----------------|----------|------|
| PA5 | SCL | SPI Clock |
| PA7 | SDA (MOSI) | SPI Data |
| PD2 | RES | Reset |
| PB5 | DC | Data/Command |
| PA4 | CS | Chip Select |
| PB6 | BLK | Backlight |

## 注意事项

1. **供电电压**：TFT 屏使用 3.3V 供电，不可接 5V
2. **信号电平**：所有控制信号均为 3.3V 电平
3. **背光**：BLK 直接接 3.3V 即可常亮，也可接 GPIO 通过 PWM 调节亮度
4. **SPI 模式**：使用 **SPI3_HOST 硬件 SPI + DMA**（通过 GPIO 交换矩阵重映射），时钟极性 CPOL=0, 时钟相位 CPHA=0（模式0），写时钟 80MHz
5. **数据格式**：MSB 优先传输（先传高位）

---

## ESP32-S3 硬件 SPI 分析 (GPIO 矩阵)

### GPIO10~14 的 FSPI 默认功能

ESP32-S3 的 **GPIO10~14** 同时也是 **FSPI (SPI2)** 的默认引脚：

| GPIO   | FSPI 默认功能 | TFT 当前连接 | 硬件 SPI 兼容性          |
| ------ | ------------- | ------------ | ------------------------ |
| GPIO10 | FSPICS0 (CS)  | TFT CS       | ✅ 完美匹配              |
| GPIO11 | FSPID (MOSI)  | TFT DC       | ⚠️ 被 DC 占用            |
| GPIO12 | FSPICLK (CLK) | TFT RES      | ⚠️ 被 RES 占用           |
| GPIO13 | FSPIQ (MISO)  | TFT SDA      | ✅ 可通过 GPIO 矩阵重映射 |
| GPIO14 | FSPIWP (WP)   | TFT SCL      | ✅ 可通过 GPIO 矩阵重映射 |

### 结论：可通过 GPIO 矩阵使用 SPI3 硬件 SPI

当前连线方式不直接兼容 FSPI（因为 GPIO11 被 DC 占用、GPIO12 被 RES 占用），但可以使用 **SPI3_HOST** + **GPIO 交换矩阵**：

```
SPI3_HOST 配置:
  CS   = GPIO10  (直连, 天然匹配)
  MOSI = GPIO13  (经 GPIO 矩阵)
  SCLK = GPIO14  (经 GPIO 矩阵)
  MISO = -1      (TFT 只写不读)

DC   = GPIO11  (GPIO 单独控制)
RES  = GPIO12  (GPIO 单独控制)
```

> 💡 ESP32-S3 的 GPIO 交换矩阵 (GPIO Matrix) 支持将 SPI 信号路由到任意 GPIO，这正是 **LovyanGFX 默认使用 SPI3_HOST 且能正常工作的原因**。

### LovyanGFX 配置中的关键参数

```cpp
cfg.spi_host = SPI3_HOST;       // 使用 SPI3，避开 FSPI 的默认引脚冲突
cfg.pin_sclk = 14;              // 通过矩阵映射 SCLK → GPIO14
cfg.pin_mosi = 13;              // 通过矩阵映射 MOSI → GPIO13
cfg.pin_miso = -1;              // 不使用 MISO
cfg.pin_dc   = 11;              // DC 作为普通 GPIO 单独控制
cfg.freq_write = 80000000;      // 80MHz SPI 时钟（最大值）
cfg.dma_channel = SPI_DMA_CH_AUTO; // 自动 DMA 通道
```

### DIJI-NES 的 SPI 总线隔离参考

DIJI-NES 项目同时使用 TFT 和 SD 卡，采用**双 SPI 总线隔离**策略：

| 外设  | SPI 总线      | 引脚                          |
| ----- | ------------- | ----------------------------- |
| TFT   | **SPI3_HOST** | SCLK=14, MOSI=13, DC=11, CS=10 |
| SD 卡 | **FSPI**      | SCLK=40, MISO=39, MOSI=41, CS=42 |

这种隔离避免了 SD 卡 SPI 操作时重新配置总线导致 TFT 显示异常。本项目仅使用 TFT，可只用 SPI3_HOST。
