// Copyright 2018-2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"

#include "wm8978.h"
static const char *TAG = "main";

#include "driver/gpio.h"
#include "driver/i2s.h"
#include "bsp_i2c.h"
#include "esp_err.h"
#include "esp_log.h"

/* Required for I2S driver workaround */
#include "esp_rom_gpio.h"
#include "hal/gpio_hal.h"
#include "hal/i2s_ll.h"

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

esp_err_t bsp_i2s_init(i2s_port_t i2s_num, uint32_t sample_rate)
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
// ESP_LOGW(TAG, "set pdm");
    // ret_val |= i2s_set_pdm_rx_down_sample(i2s_num, I2S_PDM_DSR_16S);

    return ret_val;
}

esp_err_t bsp_i2s_deinit(i2s_port_t i2s_num)
{
    esp_err_t ret_val = ESP_OK;

    ret_val |= i2s_stop(I2S_NUM_0);
    ret_val |= i2s_driver_uninstall(i2s_num);

    return ret_val;
}

#define EXAMPLE_LCD_PIXEL_CLOCK_HZ     (40 * 1000 * 1000)
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL  0
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL
#define EXAMPLE_PIN_NUM_DATA0          21
#define EXAMPLE_PIN_NUM_PCLK           18
#define EXAMPLE_PIN_NUM_CS             47
#define EXAMPLE_PIN_NUM_DC             48
#define EXAMPLE_PIN_NUM_RST            -1
#define EXAMPLE_PIN_NUM_BK_LIGHT       -1

// The pixel number in horizontal and vertical
#define EXAMPLE_LCD_H_RES              240
#define EXAMPLE_LCD_V_RES              320
// Bit number used to represent command and parameter
#define EXAMPLE_LCD_CMD_BITS           8
#define EXAMPLE_LCD_PARAM_BITS         8

esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_panel_io_handle_t io_handle = NULL;

static esp_err_t _spi_1line_lcd_init(void)
{
    esp_err_t ret_val = ESP_OK;

    // gpio_config_t bk_gpio_config = {
    //         .mode = GPIO_MODE_OUTPUT,
    //         .pin_bit_mask = BIT64(47)
    //     };
    //     ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    //     gpio_set_level(47, 1);vTaskDelay(pdMS_TO_TICKS(200));
    //     gpio_set_level(47, 0);vTaskDelay(pdMS_TO_TICKS(200));
    //     gpio_set_level(47, 1);vTaskDelay(pdMS_TO_TICKS(200));

    spi_bus_config_t buscfg = {
        .sclk_io_num = EXAMPLE_PIN_NUM_PCLK,
        .mosi_io_num = EXAMPLE_PIN_NUM_DATA0,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * sizeof(uint16_t)
    };
    ret_val |= spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = EXAMPLE_PIN_NUM_DC,
        .cs_gpio_num = EXAMPLE_PIN_NUM_CS,
        .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,
        .lcd_param_bits = EXAMPLE_LCD_PARAM_BITS,
        .user_ctx = NULL,
        // .flags.dc_as_cmd_phase = 1,
    };
    ret_val |= esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t) SPI2_HOST, &io_config, &io_handle);

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_RST,
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    esp_lcd_panel_disp_off(panel_handle, 0);
    esp_lcd_panel_invert_color(panel_handle, 1);

    if (EXAMPLE_PIN_NUM_BK_LIGHT >= 0) {
        gpio_config_t bk_gpio_config = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_BK_LIGHT
        };
        ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
        ESP_LOGI(TAG, "Turn on LCD backlight");
        gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);
    }

    return ESP_OK;
}

static void screen_clear(int color)
{
    int w = 320;
    int h = 240;
    uint16_t *buffer = malloc(w * sizeof(uint16_t));
    if (NULL == buffer) {
        return;
    } else {
        for (size_t i = 0; i < w; i++) {
            buffer[i] = color;
        }

        for (int y = 0; y < h; y++) {
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y, w, y + 1, buffer);
        }

        free(buffer);
    }
}


void app_main()
{
    esp_err_t ret;
    ESP_LOGI(TAG, "Compile time: %s %s", __DATE__, __TIME__);
    /* Initialize NVS. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(settings_read_parameter_from_nvs());

    _spi_1line_lcd_init();
    screen_clear(0xdd);

    bsp_i2s_init(I2S_NUM_0, 16000);
    ESP_ERROR_CHECK(bsp_i2c_init(I2C_NUM_0, 100000, 16, 17));

    bsp_i2c_probe();

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



