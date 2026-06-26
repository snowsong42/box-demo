/*
 * SPDX-FileCopyrightText: 2025 box-demo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "record.h"
#include "audio.h"
#include <stdio.h>
#include <string.h>
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "record";

#define AMP_MUTE_GPIO   GPIO_NUM_2

// I2S1 RX
static i2s_chan_handle_t s_rx_chan = nullptr;

// Recording
static TaskHandle_t s_record_task = nullptr;
static bool s_recording = false;
static int  s_record_elapsed_sec = 0;
static int  s_record_max_sec = 0;

// File playback
static TaskHandle_t s_play_task = nullptr;
static bool s_playing = false;

// ==================== WAV header ====================

static void write_wav_header(FILE *f, uint32_t data_size)
{
    uint32_t riff_size  = 36 + data_size;
    uint32_t fmt_size   = 16;
    uint16_t fmt_tag    = 1;
    uint16_t channels   = 1;
    uint32_t sample_rate = 22050;
    uint32_t byte_rate  = 44100;
    uint16_t block_align = 2;
    uint16_t bits_per_sample = 16;
    uint32_t data_chunk = 0x61746164;

    fwrite("RIFF", 1, 4, f);
    fwrite(&riff_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    fwrite(&fmt_size, 4, 1, f);
    fwrite(&fmt_tag, 2, 1, f);
    fwrite(&channels, 2, 1, f);
    fwrite(&sample_rate, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    fwrite(&block_align, 2, 1, f);
    fwrite(&bits_per_sample, 2, 1, f);
    fwrite(&data_chunk, 4, 1, f);
    fwrite(&data_size, 4, 1, f);
}

// ==================== 录音任务 ====================

static void record_task(void *arg)
{
    const char *path = (const char *)arg;
    if (!path) path = "/spiffs/recording.wav";

    uint8_t placeholder[44] = {0};
    size_t total_data = 0;
    int    prev_sec = -1;
    int32_t raw[256];
    int16_t pcm[256];

    FILE *f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s", path);
        goto record_cleanup;
    }
    fwrite(placeholder, 1, 44, f);

    // 关功放，开 I2S1 RX，等待麦克风稳定
    gpio_set_level(AMP_MUTE_GPIO, 0);
    i2s_channel_enable(s_rx_chan);
    vTaskDelay(pdMS_TO_TICKS(80));

    while (s_recording) {
        size_t bytes_read = 0;
        esp_err_t err = i2s_channel_read(s_rx_chan, raw, sizeof(raw),
                                         &bytes_read, pdMS_TO_TICKS(20));
        if (err == ESP_OK && bytes_read > 0) {
            int samples = bytes_read / 8;
            for (int i = 0; i < samples; i++) {
                pcm[i] = (int16_t)(raw[i * 2] >> 16);
            }
            fwrite(pcm, 2, samples, f);
            total_data += samples * 2;

            int sec = (int)(total_data / 44100);
            if (sec != prev_sec) {
                s_record_elapsed_sec = sec;
                prev_sec = sec;
            }
            if (sec >= s_record_max_sec) {
                s_recording = false;
            }
        }
    }

    // 回写 WAV header
    fseek(f, 0, SEEK_SET);
    write_wav_header(f, (uint32_t)total_data);
    fclose(f);
    ESP_LOGI(TAG, "Record done: %u bytes", (uint32_t)total_data);

record_cleanup:
    i2s_channel_disable(s_rx_chan);
    gpio_set_level(AMP_MUTE_GPIO, 1);
    s_record_task = nullptr;
    vTaskDelete(NULL);
}

// ==================== 文件播放任务 ====================

static void play_task(void *arg)
{
    const char *path = (const char *)arg;
    int16_t buf[512];
    i2s_chan_handle_t tx = nullptr;

    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Play: failed to open %s", path);
        goto play_cleanup;
    }

    fseek(f, 44, SEEK_SET);
    size_t rd;
    tx = audio_get_tx_handle();

    i2s_channel_enable(tx);

    while (s_playing && (rd = fread(buf, 1, sizeof(buf), f)) > 0) {
        size_t wr;
        i2s_channel_write(tx, buf, rd, &wr, portMAX_DELAY);
    }

    fclose(f);

play_cleanup:
    if (tx) i2s_channel_disable(tx);
    s_playing = false;
    s_play_task = nullptr;
    vTaskDelete(NULL);
}

// ==================== 公开 API ====================

bool record_init(void)
{
    // I2S1 RX
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, NULL, &s_rx_chan);

    i2s_std_config_t rx_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(22050),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = GPIO_NUM_7,
            .ws   = GPIO_NUM_15,
            .dout = I2S_GPIO_UNUSED,
            .din  = GPIO_NUM_1,
            .invert_flags = { .mclk_inv = 0, .bclk_inv = 0, .ws_inv = 0 }
        },
    };
    i2s_channel_init_std_mode(s_rx_chan, &rx_cfg);

    // 功放静音控制
    gpio_config_t mute_cfg = {
        .pin_bit_mask = BIT64(AMP_MUTE_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&mute_cfg);
    gpio_set_level(AMP_MUTE_GPIO, 1);

    ESP_LOGI(TAG, "I2S1 RX init OK, amp mute=GPIO%d", AMP_MUTE_GPIO);
    return true;
}

bool record_start(const char *filepath, int max_sec)
{
    if (s_recording || s_playing) return false;
    s_recording = true;
    s_record_max_sec = (max_sec > 0) ? max_sec : 15;
    s_record_elapsed_sec = 0;

    if (xTaskCreate(record_task, "record", 4096, (void *)filepath, 1, &s_record_task) != pdPASS) {
        s_recording = false;
        return false;
    }
    return true;
}

void record_stop(void)
{
    if (!s_recording) return;
    s_recording = false;
    if (s_record_task) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

bool record_is_recording(void) { return s_recording; }

int  record_time_elapsed(void) { return s_record_elapsed_sec; }

bool record_play_start(const char *filepath)
{
    if (s_playing || s_recording) return false;
    s_playing = true;

    if (xTaskCreate(play_task, "playfile", 4096, (void *)filepath, 1, &s_play_task) != pdPASS) {
        s_playing = false;
        return false;
    }
    return true;
}

void record_play_stop(void)
{
    if (!s_playing) return;
    s_playing = false;
    if (s_play_task) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

bool record_is_playing(void) { return s_playing; }
