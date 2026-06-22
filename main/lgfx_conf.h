#pragma once
#include <LovyanGFX.hpp>

/// ST7789 TFT 配置 (基于 DIJI-NES 项目已验证的硬件配置)
/// 引脚: SCLK=14, MOSI=13, DC=11, CS=10, RST=12, BLK=VCC
class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7789  _panel_instance;
    lgfx::Bus_SPI       _bus_instance;

public:
    LGFX(void)
    {
        // ---- SPI 总线配置 ----
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host    = SPI3_HOST;       // ESP32-S3 使用 SPI3_HOST
            cfg.spi_mode    = 0;               // CPOL=0, CPHA=0
            cfg.freq_write  = 80000000;        // 写时钟 80MHz (最大值)
            cfg.freq_read   = 6000000;         // 读时钟
            cfg.spi_3wire   = true;            // 3 线 SPI (MOSI 兼 MISO)
            cfg.use_lock    = false;           // 不使用事务锁
            cfg.dma_channel = SPI_DMA_CH_AUTO; // 自动 DMA 通道
            cfg.pin_sclk    = 14;              // SCLK → GPIO14
            cfg.pin_mosi    = 13;              // MOSI → GPIO13
            cfg.pin_miso    = -1;              // 不使用 MISO
            cfg.pin_dc      = 11;              // DC   → GPIO11

            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        // ---- 面板配置 ----
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs           = 10;    // CS  → GPIO10
            cfg.pin_rst          = 12;    // RST → GPIO12
            cfg.pin_busy         = -1;    // 不使用 BUSY

            cfg.panel_width      = 240;   // 面板宽
            cfg.panel_height     = 320;   // 面板高
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 0;

            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = true;
            cfg.invert           = true;   // ★ 此屏需要反色
            cfg.rgb_order        = false;  // BGR 顺序
            cfg.dlen_16bit       = false;  // 8bit 数据长度传输
            cfg.bus_shared       = false;  // 不共享总线

            _panel_instance.config(cfg);
        }

        setPanel(&_panel_instance);
    }
};
