# 💾 SD 卡开发指南

> MicroSD 卡通过独立 FSPI 总线接入，挂载 FAT 文件系统到 `/sdcard`

---

## 硬件接线

| 信号 | GPIO | SPI 总线 |
|------|------|----------|
| CS | 42 | FSPI (SPI2_HOST) |
| SCLK | 40 | |
| MISO | 39 | |
| MOSI | 41 | |

⚠️ SD 卡使用独立的 **FSPI** 总线，与 TFT 的 **SPI3_HOST** 不共享。这是关键设计——如果共用总线，SD 卡读写会导致 TFT 画面闪烁。

---

## 驱动初始化

```c
// sd_card.c sd_card_init()
spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);

sdmmc_host_t host = SDSPI_HOST_DEFAULT();
host.slot = SPI2_HOST;
host.max_freq_khz = 10000;  // 10 MHz

esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_cfg, &mount_cfg, &card);
```

启动时静默挂载（`storage_init()` 中调用）。未插卡或失败不阻塞系统，仅打印 W 级别日志。

---

## CMake 依赖（ESP-IDF v6.0.1）

```cmake
# components/storage/CMakeLists.txt
REQUIRES
    spiffs
    driver
    sdmmc
    vfs
    fatfs            # ← esp_vfs_fat.h
    esp_timer        # ← esp_timer_get_time()
    esp_driver_sdspi # ← driver/sdspi_host.h (v6.0.1 从 driver 拆分)
```

**踩坑记录**：ESP-IDF v6.0.1 把 SDSPI 从 `driver` 组件拆成了独立组件 `esp_driver_sdspi`。不加这个依赖会报 `driver/sdspi_host.h: No such file or directory`。

同样 `esp_vfs_fat.h` 需要显式声明 `fatfs` 依赖。

---

## API 兼容性

### SD_OCR_SDHC_CAP（v6.0.1 中已移除）

```c
// 错误写法（v5.x）
if (s_card->ocr & SD_OCR_SDHC_CAP) { ... }

// 正确写法（v6.0.1）
if (info->total_bytes >= 2ULL * 1024 * 1024 * 1024) { ... }  // >= 2GB = SDHC
```

### 可用空间查询

`statvfs()` 在 ESP-IDF FATFS 上不稳定，用 FATFS 原生 API 替代：

```c
FATFS *fs = NULL;
DWORD free_clusters;
f_getfree("/sdcard", &free_clusters, &fs);
info->free_bytes = (uint64_t)free_clusters * fs->csize * fs->ssize;
```

---

## 文件浏览器

`draw_sd_card_browse()` 使用 POSIX `opendir`/`readdir` 遍历目录树。

交互：
- 状态页按 `RIGHT` → 进入浏览模式
- `U/D` 选择，`R` 进子目录，`L` 返回上级
- `BACK` 回到状态页

---

## SPIFFS vs SD 卡对比

| | SPIFFS | SD 卡 FAT |
|------|------|------|
| 顺序写入 | GC 阻塞 50-200ms | 几乎无延迟 |
| 删建同名文件 | 碎片化滚雪球 | 无影响 |
| 最大容量 | 4MB（分区限制） | GB 级 |
| 反复录音 | 第 3 次崩溃 | 始终一致 |
| 适用场景 | 配置、小文件 | 流式写入、大文件 |

---

## SD 卡目录结构约定

```
/sdcard/
├── 0001.png ~ 0009.png    ← 图片浏览器
├── gif_0001.png ~ gif_0028.png  ← GIF 帧
├── 500x150.raw            ← 走马灯
├── music.wav              ← 背景音乐
└── rec.wav                ← 录音（程序自动生成）
```

---

## 相关文件

- `components/storage/sd_card.c` — 驱动 + FAT 挂载
- `components/storage/include/sd_card.h` — API
- `components/display/display.cpp` — `draw_sd_card_status()` / `draw_sd_card_browse()`
- `main/main.cpp` — `handle_sd_card()` 状态机
