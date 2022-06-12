/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2s.h"
#include "esp_log.h"
#include "wm8978.h"

static const char *TAG = "codec";

#define I2S_CONFIG_DEFAULT() { \
    .mode                   = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX, \
    .sample_rate            = sample_rate, \
    .bits_per_sample        = I2S_BITS_PER_SAMPLE_16BIT, \
    .channel_format         = I2S_CHANNEL_FMT_RIGHT_LEFT, \
    .communication_format   = I2S_COMM_FORMAT_STAND_I2S, \
    .intr_alloc_flags       = ESP_INTR_FLAG_LEVEL1, \
    .dma_buf_count          = 6, \
    .dma_buf_len            = 160, \
    .use_apll               = false, \
    .tx_desc_auto_clear     = true, \
    .fixed_mclk             = 0, \
    .mclk_multiple          = I2S_MCLK_MULTIPLE_DEFAULT, \
    .bits_per_chan          = I2S_BITS_PER_CHAN_16BIT, \
}

static esp_err_t bsp_i2s_init(i2s_port_t i2s_num, uint32_t sample_rate)
{
    esp_err_t ret_val = ESP_OK;

    i2s_config_t i2s_config = I2S_CONFIG_DEFAULT();

    i2s_pin_config_t pin_config = {
        .bck_io_num = 13,
        .ws_io_num = 14,
        .data_out_num = 11,
        .data_in_num = 12,
        .mck_io_num = 10,
    };

    ret_val |= i2s_driver_install(i2s_num, &i2s_config, 0, NULL);
    ret_val |= i2s_set_pin(i2s_num, &pin_config);
    ret_val |= i2s_zero_dma_buffer(i2s_num);

    return ret_val;
}


void codec_init(void)
{
    bsp_i2s_init(I2S_NUM_0, 16000);
    vTaskDelay(pdMS_TO_TICKS(500));

    WM8978_Init();
// WM8978_PlayMode();
    WM8978_RecoMode();

// WM8978_HPvol_Set(32, 32);
// WM8978_SPKvol_Set(40);

    ESP_ERROR_CHECK(bsp_spiffs_init("storage", "/spiffs", 2));
// ESP_ERROR_CHECK(bsp_spiffs_init("model", "/srmodel", 4));

// while (1)
// {WM8978_SPKvol_Set(60);
//     esp_err_t sr_echo_play(char audio_file[]);
//     sr_echo_play("/spiffs/echo_cn_wake.wav");
//     vTaskDelay(pdMS_TO_TICKS(500));
// }
    ESP_LOGI(TAG, "speech recognition start");
// app_sr_start(false);

    void ap_start(void);
    ap_start();
    ESP_LOGI(TAG, "exit");
}








