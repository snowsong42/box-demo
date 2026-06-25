/*
 * SPDX-FileCopyrightText: 2025 box-demo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "buttons.h"
#include "esp_log.h"

static const char *TAG = "buttons";

void buttons_init(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pin_bit_mask = (1ULL << BTN_UP) | (1ULL << BTN_DOWN)
                         | (1ULL << BTN_LEFT) | (1ULL << BTN_RIGHT)
                         | (1ULL << BTN_START) | (1ULL << BTN_BACK);
    gpio_config(&io_conf);
    ESP_LOGI(TAG, "Initialized: UP=17 DOWN=3 LEFT=8 RIGHT=18 START=47 BACK=48");
}

int read_buttons(void)
{
    static int s_prev_btn = BTN_NONE;

    int curr = BTN_NONE;
    if (gpio_get_level(BTN_UP) == 0)    curr = BTN_U;
    if (gpio_get_level(BTN_DOWN) == 0)  curr = BTN_D;
    if (gpio_get_level(BTN_LEFT) == 0)  curr = BTN_L;
    if (gpio_get_level(BTN_RIGHT) == 0) curr = BTN_R;
    if (gpio_get_level(BTN_START) == 0) curr = BTN_S;
    if (gpio_get_level(BTN_BACK) == 0)  curr = BTN_B;

    int event = (curr != BTN_NONE && curr != s_prev_btn) ? curr : BTN_NONE;
    s_prev_btn = curr;
    return event;
}
