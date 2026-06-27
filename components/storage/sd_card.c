/*
 * SPDX-FileCopyrightText: 2025 box-demo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sd_card.h"
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_timer.h"
#include "ff.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"

static const char *TAG = "sd_card";

// SD 卡 SPI 引脚 (FSPI 独立总线)
#define SD_CS_PIN   GPIO_NUM_42
#define SD_SCLK_PIN GPIO_NUM_40
#define SD_MISO_PIN GPIO_NUM_39
#define SD_MOSI_PIN GPIO_NUM_41

// 速度测试文件
#define SPEED_TEST_PATH  "/sdcard/_speed_test.tmp"
#define SPEED_TEST_SIZE  (512 * 1024)  // 512KB

static sdmmc_card_t *s_card = NULL;
static bool s_mounted = false;

// ==================== 公开 API ====================

void sd_card_init(void)
{
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_MOSI_PIN,
        .miso_io_num = SD_MISO_PIN,
        .sclk_io_num = SD_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "FSPI bus init failed (%s), SD card unavailable", esp_err_to_name(ret));
        return;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    host.max_freq_khz = 10000;  // 10 MHz

    sdspi_device_config_t slot_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_cfg.gpio_cs   = SD_CS_PIN;
    slot_cfg.host_id   = host.slot;

    esp_vfs_fat_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_cfg, &mount_cfg, &s_card);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card mount failed (%s) — no card or unsupported format",
                 esp_err_to_name(ret));
        spi_bus_free(SPI2_HOST);
        return;
    }

    s_mounted = true;

    sd_card_info_t info;
    sd_card_get_info(&info);
    ESP_LOGI(TAG, "SD mounted: '%s' %s, %llu/%llu MB, sector=%d",
             info.name, info.fs_type,
             (unsigned long long)(info.total_bytes / (1024 * 1024)),
             (unsigned long long)(info.free_bytes / (1024 * 1024)),
             info.sector_size);
}

bool sd_card_mounted(void)
{
    return s_mounted;
}

void sd_card_get_info(sd_card_info_t *info)
{
    if (!info) return;
    memset(info, 0, sizeof(sd_card_info_t));

    if (!s_mounted || !s_card) return;

    info->mounted = true;

    // 卡名称 (CID)
    memcpy(info->name, s_card->cid.name, sizeof(s_card->cid.name));
    info->name[sizeof(s_card->cid.name)] = '\0';
    // 去除尾部空格
    for (int i = (int)strlen(info->name) - 1; i >= 0; i--) {
        if (info->name[i] == ' ' || info->name[i] == 0xFF) info->name[i] = '\0';
        else break;
    }

    // 容量
    info->sector_size = s_card->csd.sector_size;
    info->total_bytes = (uint64_t)s_card->csd.capacity * s_card->csd.sector_size;

    // 文件系统类型（通过容量判断 SDHC/SDSC）
    if (info->total_bytes >= 2ULL * 1024 * 1024 * 1024) {  // >= 2GB = SDHC
        strncpy(info->fs_type, "SDHC", sizeof(info->fs_type) - 1);
    } else {
        strncpy(info->fs_type, "SDSC", sizeof(info->fs_type) - 1);
    }

    // 可用空间 (使用 FATFS 原生 API，比 statvfs 可靠)
    FATFS *fs = NULL;
    DWORD free_clusters;
    if (f_getfree("/sdcard", &free_clusters, &fs) == FR_OK && fs) {
        info->free_bytes = (uint64_t)free_clusters * fs->csize * fs->ssize;
    }
}

bool sd_card_speed_test(int *write_ms, int *read_ms)
{
    if (write_ms) *write_ms = -1;
    if (read_ms) *read_ms = -1;

    if (!s_mounted) return false;

    // 准备测试数据 (64KB 块)
    const int BLOCK = 64 * 1024;
    uint8_t *buf = malloc(BLOCK);
    if (!buf) return false;
    for (int i = 0; i < BLOCK; i++) buf[i] = (uint8_t)(i & 0xFF);

    // === 写入测试 ===
    int64_t t0 = esp_timer_get_time();
    FILE *f = fopen(SPEED_TEST_PATH, "wb");
    if (!f) { free(buf); return false; }

    for (int written = 0; written < SPEED_TEST_SIZE; written += BLOCK) {
        int chunk = (SPEED_TEST_SIZE - written < BLOCK) ? (SPEED_TEST_SIZE - written) : BLOCK;
        fwrite(buf, 1, chunk, f);
    }
    fclose(f);

    int64_t t1 = esp_timer_get_time();
    if (write_ms) *write_ms = (int)((t1 - t0) / 1000);

    // === 读取测试 ===
    t0 = esp_timer_get_time();
    f = fopen(SPEED_TEST_PATH, "rb");
    if (!f) { remove(SPEED_TEST_PATH); free(buf); return false; }

    while (fread(buf, 1, BLOCK, f) > 0) { }
    fclose(f);

    t1 = esp_timer_get_time();
    if (read_ms) *read_ms = (int)((t1 - t0) / 1000);

    // 清理
    remove(SPEED_TEST_PATH);
    free(buf);
    return true;
}

int sd_card_list_dir(const char *path, sd_card_entry_t *entries, int max)
{
    if (!s_mounted || !entries || max <= 0) return -1;
    DIR *d = opendir(path);
    if (!d) return -1;
    int count = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL && count < max) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        strncpy(entries[count].name, de->d_name, 63);
        entries[count].name[63] = '\0';
        entries[count].is_dir = (de->d_type == DT_DIR);
        entries[count].size = 0;
        if (!entries[count].is_dir) {
            char fp[280];
            snprintf(fp, sizeof(fp), "%s/%s", path, de->d_name);
            struct stat st;
            if (stat(fp, &st) == 0) entries[count].size = (uint32_t)st.st_size;
        }
        count++;
    }
    closedir(d);
    return count;
}
