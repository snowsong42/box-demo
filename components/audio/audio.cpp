/*
 * SPDX-FileCopyrightText: 2025 box-demo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "audio.h"
#include <stdio.h>
#include "esp_log.h"

static const char *TAG = "audio";

static i2s_chan_handle_t s_tx_chan = nullptr;
static i2s_chan_handle_t s_rx_chan = nullptr;
static TaskHandle_t s_audio_task = nullptr;
static TaskHandle_t s_record_task = nullptr;
static TaskHandle_t s_play_file_task = nullptr;
static bool s_audio_running = false;
static bool s_recording = false;
static bool s_playing_file = false;
static int  s_record_elapsed_sec = 0;
static int  s_record_max_sec = 0;

static void audio_playback_task(void *arg)
{
    const char *path = "/spiffs/music.wav";
    while (s_audio_running) {
        FILE *f = fopen(path, "rb");
        if (!f) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        fseek(f, 44, SEEK_SET);  // skip WAV header
        int16_t buf[512];
        size_t rd;
        while (s_audio_running && (rd = fread(buf, 1, sizeof(buf), f)) > 0) {
            size_t wr;
            i2s_channel_write(s_tx_chan, buf, rd, &wr, portMAX_DELAY);
        }
        fclose(f);
    }
    s_audio_task = nullptr;
    vTaskDelete(NULL);
}

void audio_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, &s_tx_chan, &s_rx_chan);

    // === TX 通道（播放） ===
    i2s_std_config_t tx_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(22050),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = GPIO_NUM_5,
            .ws   = GPIO_NUM_4,
            .dout = GPIO_NUM_6,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { .mclk_inv = 0, .bclk_inv = 0, .ws_inv = 0 }
        },
    };
    i2s_channel_init_std_mode(s_tx_chan, &tx_cfg);

    // === RX 通道（录音） ===
    // TX/RX 共享 BCLK/WS，RX 侧不重复指定
    i2s_std_config_t rx_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(22050),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_GPIO_UNUSED,
            .ws   = I2S_GPIO_UNUSED,
            .dout = I2S_GPIO_UNUSED,
            .din  = GPIO_NUM_1,
            .invert_flags = { .mclk_inv = 0, .bclk_inv = 0, .ws_inv = 0 }
        },
    };
    i2s_channel_init_std_mode(s_rx_chan, &rx_cfg);

    ESP_LOGI(TAG, "I2S initialized: 22050Hz 16bit mono (TX+RX full-duplex)");
}

void audio_start(void)
{
    if (s_audio_running) return;
    s_audio_running = true;
    i2s_channel_enable(s_tx_chan);
    xTaskCreate(audio_playback_task, "audio", 4096, NULL, 1, &s_audio_task);
}

void audio_stop(void)
{
    s_audio_running = false;
    if (s_audio_task) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    i2s_channel_disable(s_tx_chan);
}

void audio_deinit(void)
{
    if (s_audio_running) audio_stop();
    if (s_recording) audio_record_stop();
    if (s_playing_file) audio_play_file_stop();
    if (s_tx_chan) {
        i2s_channel_disable(s_tx_chan);
        s_tx_chan = nullptr;
    }
    if (s_rx_chan) {
        i2s_channel_disable(s_rx_chan);
        s_rx_chan = nullptr;
    }
}

bool audio_is_running(void)
{
    return s_audio_running;
}

// ==================== 录音任务 ====================

/** @brief 写入 44 字节 WAV 文件头（小端序，PCM 16bit Mono 22050Hz） */
static void write_wav_header(FILE *f, uint32_t data_size)
{
    uint32_t riff_size  = 36 + data_size;
    uint32_t fmt_size   = 16;
    uint16_t fmt_tag    = 1;       // PCM
    uint16_t channels   = 1;       // Mono
    uint32_t sample_rate = 22050;
    uint32_t byte_rate  = 22050 * 1 * 2;
    uint16_t block_align = 1 * 2;
    uint16_t bits_per_sample = 16;
    uint32_t data_chunk = 0x61746164; // "data"

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

static void audio_record_task(void *arg)
{
    const char *path = (const char *)arg;
    if (!path) path = "/spiffs/recording.wav";

    // 所有变量在 goto 之前声明（C++ 不允许 goto 跨越初始化）
    uint8_t placeholder[44] = {0};
    size_t total_data = 0;
    int    prev_sec = -1;
    int16_t buf[512];

    // 打开文件（覆盖写入）
    FILE *f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Record: failed to open %s", path);
        goto record_cleanup;
    }

    // 先写占位 header（44 字节）
    fwrite(placeholder, 1, 44, f);

    // 全双工模式：TX 必须先使能以提供 BCLK/WS 时钟，RX 才能采样
    i2s_channel_enable(s_tx_chan);
    i2s_channel_enable(s_rx_chan);

    while (s_recording) {
        size_t bytes_read = 0;
        esp_err_t err = i2s_channel_read(s_rx_chan, buf, sizeof(buf), &bytes_read, pdMS_TO_TICKS(100));
        if (err == ESP_OK && bytes_read > 0) {
            fwrite(buf, 1, bytes_read, f);
            total_data += bytes_read;

            // 每秒更新 elapsed 秒数
            int sec = (int)(total_data / 44100);  // 44100 bytes/sec
            if (sec != prev_sec) {
                s_record_elapsed_sec = sec;
                prev_sec = sec;
            }

            // 超时自动停止
            if (sec >= s_record_max_sec) {
                s_recording = false;
            }
        }
    }

    // 回写正确 WAV header
    fseek(f, 0, SEEK_SET);
    write_wav_header(f, (uint32_t)total_data);

    fclose(f);
    ESP_LOGI(TAG, "Record finished: %u bytes of audio data", (uint32_t)total_data);

record_cleanup:
    i2s_channel_disable(s_rx_chan);
    i2s_channel_disable(s_tx_chan);
    s_record_task = nullptr;
    vTaskDelete(NULL);
}

// ==================== 文件播放任务（单次）====================

static void audio_play_file_task(void *arg)
{
    const char *path = (const char *)arg;
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "PlayFile: failed to open %s", path);
        goto play_cleanup;
    }

    fseek(f, 44, SEEK_SET);  // 跳过 WAV header
    int16_t buf[512];
    size_t rd;

    i2s_channel_enable(s_tx_chan);

    while (s_playing_file && (rd = fread(buf, 1, sizeof(buf), f)) > 0) {
        size_t wr;
        i2s_channel_write(s_tx_chan, buf, rd, &wr, portMAX_DELAY);
    }

    fclose(f);

play_cleanup:
    i2s_channel_disable(s_tx_chan);
    s_playing_file = false;
    s_play_file_task = nullptr;
    vTaskDelete(NULL);
}

// ==================== 录音 API ====================

bool audio_record_start(const char *filepath, int max_sec)
{
    if (s_recording || s_playing_file || s_audio_running) {
        return false;
    }
    s_recording = true;
    s_record_max_sec = (max_sec > 0) ? max_sec : 15;
    s_record_elapsed_sec = 0;

    if (xTaskCreate(audio_record_task, "record", 4096, (void *)filepath, 1, &s_record_task) != pdPASS) {
        s_recording = false;
        return false;
    }
    return true;
}

void audio_record_stop(void)
{
    if (!s_recording) return;
    s_recording = false;
    if (s_record_task) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

bool audio_is_recording(void)
{
    return s_recording;
}

int  audio_record_time_elapsed(void)
{
    return s_record_elapsed_sec;
}

// ==================== 文件播放 API ====================

bool audio_play_file_start(const char *filepath)
{
    if (s_playing_file || s_recording || s_audio_running) {
        return false;
    }
    s_playing_file = true;

    if (xTaskCreate(audio_play_file_task, "playfile", 4096, (void *)filepath, 1, &s_play_file_task) != pdPASS) {
        s_playing_file = false;
        return false;
    }
    return true;
}

void audio_play_file_stop(void)
{
    if (!s_playing_file) return;
    s_playing_file = false;
    if (s_play_file_task) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

bool audio_is_playing_file(void)
{
    return s_playing_file;
}
