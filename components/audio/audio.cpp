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
static TaskHandle_t s_audio_task = nullptr;
static bool s_audio_running = false;

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
    i2s_new_channel(&chan_cfg, &s_tx_chan, NULL);

    i2s_std_config_t std_cfg = {
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
    i2s_channel_init_std_mode(s_tx_chan, &std_cfg);
    ESP_LOGI(TAG, "I2S initialized: 22050Hz 16bit mono");
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
    if (s_tx_chan) {
        i2s_channel_disable(s_tx_chan);
        s_tx_chan = nullptr;
    }
}

bool audio_is_running(void)
{
    return s_audio_running;
}
