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
#include "nvs_flash.h"


static const char *TAG = "main";
#include "Arduino.h"
#include "esp_log.h"


void setup(void);
void loop(void);

void loopTask(void *pvParameters)
{
    setup();
    for(;;) {
        loop();
    }
}

extern "C" void app_main()
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
    // ESP_ERROR_CHECK(settings_read_parameter_from_nvs());

    initArduino();
    xTaskCreate(loopTask, "loopTask", 8192, NULL, 8, NULL);

    ESP_LOGI(TAG, "exit");
}



