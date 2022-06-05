/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <dirent.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2s.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"

static const char *TAG = "player";


size_t fm_get_file_size(const char *filepath)
{
    struct stat siz =  { 0 };
    stat(filepath, &siz);
    return siz.st_size;
}

esp_err_t sr_echo_play(char audio_file[])
{
    esp_err_t ret;
    typedef struct {
        // The "RIFF" chunk descriptor
        uint8_t ChunkID[4];
        int32_t ChunkSize;
        uint8_t Format[4];
        // The "fmt" sub-chunk
        uint8_t Subchunk1ID[4];
        int32_t Subchunk1Size;
        int16_t AudioFormat;
        int16_t NumChannels;
        int32_t SampleRate;
        int32_t ByteRate;
        int16_t BlockAlign;
        int16_t BitsPerSample;
        // The "data" sub-chunk
        uint8_t Subchunk2ID[4];
        int32_t Subchunk2Size;
    } wav_header_t;

    /**
     * read head of WAV file
     */
    FILE *fp;
    fp = fopen(audio_file, "rb");
    ESP_GOTO_ON_FALSE(NULL != fp, ESP_ERR_NOT_FOUND, err, TAG, "Open file %s failed", audio_file);
    size_t file_size = fm_get_file_size(audio_file);

    uint8_t *audio_buffer = heap_caps_calloc(1, file_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    ESP_GOTO_ON_FALSE(NULL != audio_buffer, ESP_ERR_NO_MEM, err, TAG,  "No mem for sr echo buffer");
    fread(audio_buffer, 1, file_size, fp);

    uint8_t *p = audio_buffer;
    wav_header_t *wav_head = (wav_header_t *)p;
    if (NULL == strstr((char *)wav_head->Subchunk1ID, "fmt") &&
            NULL == strstr((char *)wav_head->Subchunk2ID, "data")) {
        ESP_LOGE(TAG, "Header of wav format error");
        return ESP_FAIL;
    }
    p += sizeof(wav_header_t);
    size_t len = file_size - sizeof(wav_header_t);
    len = len & 0xfffffffc;
    ESP_LOGI(TAG, "frame_rate=%d, ch=%d, width=%d", wav_head->SampleRate, wav_head->NumChannels, wav_head->BitsPerSample);

    size_t bytes_written = 0;
    i2s_write(I2S_NUM_0, p, len, &bytes_written, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(20));
    fclose(fp);
    free(audio_buffer);
    return ESP_OK;
err:
    if (fp) {
        fclose(fp);
    }

    return ret;
}

