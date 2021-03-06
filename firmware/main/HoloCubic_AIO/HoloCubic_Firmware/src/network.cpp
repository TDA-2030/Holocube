#include "network.h"
#include "common.h"
#include "nvs_flash.h"
#include "captive_portal.h"
#include "esp_log.h"
#include "esp_check.h"
#include "app_sntp.h"

static const char *TAG = "network";

static TaskHandle_t g_net_task_handle;
static bool is_connected = 0;

static void net_task(void *args)
{
    esp_err_t ret = ESP_OK;
    bool is_configured;
    captive_portal_start("Holocube", NULL, &is_configured);
    if (is_configured) {
        wifi_config_t wifi_config;
        esp_wifi_get_config((wifi_interface_t)ESP_IF_WIFI_STA, &wifi_config);
        // char str[84];
        // sprintf(str, "正在连接%s", wifi_config.sta.ssid);
        // paint_roll_text_create(0, 16, 64);
        // paint_roll_text_set_string(str, &Font16_gbk);
        ESP_LOGI(TAG, "SSID:%s, PASSWORD:%s", wifi_config.sta.ssid, wifi_config.sta.password);
        ret = captive_portal_wait(30000 / portTICK_PERIOD_MS);
        if (ESP_OK != ret) {
            ESP_LOGE(TAG, "Connect to AP timeout, restart to entry configuration");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            nvs_flash_erase();
            esp_restart();
        }
    } else {
        // paint_roll_text_create(0, 16, 64);
        // paint_roll_text_set_string("未联网，请进入192.168.4.1进行配网", &Font16_gbk);
        ret = captive_portal_wait(portMAX_DELAY);
    }
    is_connected = 1;
    app_sntp_init();
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

Network::Network()
{
    ESP_LOGI(TAG, "Network construct");
    m_preDisWifiConnInfoMillis = 0;
    is_connected = 0;
}

void Network::config(void)
{
    if (NULL != g_net_task_handle) {
        ESP_LOGE(TAG, "net task already created");
        return;
    }

    BaseType_t ret_val = xTaskCreatePinnedToCore(net_task, "net_task", 3 * 1024, NULL, 2, &g_net_task_handle, 0);
    if (pdPASS != ret_val) {
        ESP_LOGE(TAG, "net task creat failed");
    }
}

bool Network::isconnected(void)
{
    return is_connected;
}
