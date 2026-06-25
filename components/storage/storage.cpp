/*
 * SPDX-FileCopyrightText: 2025 box-demo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "storage.h"
#include <stdio.h>
#include "esp_spiffs.h"
#include "esp_log.h"

static const char *TAG = "storage";

void storage_init(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = false
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed (%s)", esp_err_to_name(ret));
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info("storage", &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS: total=%d used=%d", total, used);
    }
}

int detect_img_count(void)
{
    int count = 0;
    for (int i = 1; i <= 99; i++) {
        char path[32];
        snprintf(path, sizeof(path), "/spiffs/%04d.png", i);
        FILE *f = fopen(path, "r");
        if (f) {
            fclose(f);
            count = i;
        } else {
            break;
        }
    }
    ESP_LOGI(TAG, "Detected %d images", count);
    return count;
}

bool get_png_size(const char *path, int *w, int *h)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "get_png_size: fopen failed %s", path);
        return false;
    }

    uint8_t buf[24];
    if (fread(buf, 1, 24, f) != 24) {
        ESP_LOGE(TAG, "get_png_size: fread failed %s", path);
        fclose(f);
        return false;
    }
    fclose(f);

    if (buf[0] != 0x89 || buf[1] != 'P' || buf[2] != 'N' || buf[3] != 'G') {
        ESP_LOGE(TAG, "get_png_size: not PNG %s", path);
        return false;
    }
    if (buf[12] != 'I' || buf[13] != 'H' || buf[14] != 'D' || buf[15] != 'R') {
        ESP_LOGE(TAG, "get_png_size: no IHDR %s", path);
        return false;
    }

    *w = (buf[16] << 24) | (buf[17] << 16) | (buf[18] << 8) | buf[19];
    *h = (buf[20] << 24) | (buf[21] << 16) | (buf[22] << 8) | buf[23];
    return true;
}
