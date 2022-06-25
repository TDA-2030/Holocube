/***************************************************
  HoloCubic多功能固件源码

  聚合多种APP，内置天气、时钟、相册、特效动画、视频播放、视频投影、
  浏览器文件修改。（各APP具体使用参考说明书）

  Github repositories：https://github.com/ClimbSnail/HoloCubic_AIO

  Last review/edit by ClimbSnail: 2021/08/21
 ****************************************************/

#include "driver/display.h"
#include "driver/lv_port_fatfs.h"
#include "driver/imu.h"
#include "common.h"
#include "sys/app_controller.h"

#include "app/weather/weather.h"
#include "app/bilibili_fans/bilibili.h"
#include "app/server/server.h"
#include "app/settings/settings.h"
#include "app/picture/picture.h"
#include "app/media_player/media_player.h"
#include "app/screen_share/screen_share.h"
#include "app/file_manager/file_manager.h"
#include "app/anniversary/anniversary.h"
#include "app/audio_spectrum/audio_spectrum.h"

#include <esp32-hal.h>
#include <esp32-hal-timer.h>

static const char *TAG = "Holo Main";

AppController *app_controller; // APP控制器

static void memdisk_init(void)
{
    if (!SPIFFS.begin(true)) {
        ESP_LOGI(TAG, "SPIFFS Mount Failed");
        return;
    }

    SPIClass *sd_spi = new SPIClass(HSPI); // another SPI
    sd_spi->begin(2, 3, 4, 5);         // Replace default HSPI pins
    if (!SD.begin(5, *sd_spi, 80000000)) { // SD-Card SS pin is 15
        ESP_LOGI(TAG, "Card Mount Failed");
        return;
    }
    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE) {
        ESP_LOGI(TAG, "No SD card attached");
        return;
    }

    ESP_LOGI(TAG, "SD Card Type: ");
    if (cardType == CARD_MMC) {
        ESP_LOGI(TAG, "MMC");
    } else if (cardType == CARD_SD) {
        ESP_LOGI(TAG, "SDSC");
    } else if (cardType == CARD_SDHC) {
        ESP_LOGI(TAG, "SDHC");
    } else {
        ESP_LOGI(TAG, "UNKNOWN");
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    ESP_LOGI(TAG, "SD Card Size: %lluMB", cardSize);
}

void setup()
{
    Serial.begin(115200);

    ESP_LOGI(TAG, "AIO (All in one) version " AIO_VERSION );

    // MAC ID可用作芯片唯一标识
    uint64_t _chipmacid = 0LL;
    esp_efuse_mac_get_default((uint8_t *) (&_chipmacid));
    ESP_LOGI(TAG, "ChipID(EfuseMac): %0X", _chipmacid);

    app_controller = new AppController(); // APP控制器

    memdisk_init();

    app_controller->read_config(&app_controller->sys_cfg);

    /*** Init screen ***/
    screen.init(0,//app_controller->sys_cfg.rotation,
                app_controller->sys_cfg.backLight);

    /*** Init micro SD-Card ***/
    lv_fs_if_init();

    g_network.config();

    app_controller->init();
    // 将APP"安装"到controller里
    app_controller->app_install(&weather_app);
    // app_controller->app_install(&picture_app);
    // app_controller->app_install(&media_app);
    app_controller->app_install(&screen_share_app);
    // app_controller->app_install(&file_manager_app);
    // app_controller->app_install(&server_app);
    // app_controller->app_install(&bilibili_app);
    // app_controller->app_install(&settings_app);
    app_controller->app_install(&audio_spectrum_app);
    // app_controller->app_install(&anniversary_app);

    // app_controller->main_process(&mpu.action_info);
    /*** Init IMU as input device ***/
    lv_port_indev_init();
    mpu.init(app_controller->sys_cfg.mpu_order, 1); // 初始化比较耗时

}

void loop()
{
    screen.routine();
    ImuAction *act_info = mpu.getAction();
    printf("[Operate] active: %s\n", mpu.getActionname(act_info->active));
    app_controller->main_process((APP_ACTIVE_TYPE)act_info->active); // 运行当前进程
    act_info->isValid = 0;
}
