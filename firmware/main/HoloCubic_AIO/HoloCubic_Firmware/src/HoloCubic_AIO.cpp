/***************************************************
  HoloCubic多功能固件源码

  聚合多种APP，内置天气、时钟、相册、特效动画、视频播放、视频投影、
  浏览器文件修改。（各APP具体使用参考说明书）

  Github repositories：https://github.com/ClimbSnail/HoloCubic_AIO

  Last review/edit by ClimbSnail: 2021/08/21
 ****************************************************/

#include "driver/display.h"
#include "driver/lv_port_fatfs.h"
#include "common.h"

#include "sys/app_controller.h"

#include "app/weather/weather.h"
#include "app/bilibili_fans/bilibili.h"
#include "app/server/server.h"
#include "app/idea_anim/idea.h"
#include "app/settings/settings.h"
#include "app/picture/picture.h"
#include "app/media_player/media_player.h"
#include "app/screen_share/screen_share.h"
#include "app/file_manager/file_manager.h"
#include "app/weather_old/weather_old.h"
#include "app/anniversary/anniversary.h"
#include "app/heartbeat/heartbeat.h"
#include "app/audio_spectrum/audio_spectrum.h"

#include <SPIFFS.h>
#include <esp32-hal.h>
#include <esp32-hal-timer.h>

static bool isCheckAction = false;
/*** Component objects **7*/
ImuAction *act_info; // 存放mpu6050返回的数据

AppController *app_controller; // APP控制器

TimerHandle_t xTimerAction = NULL;
void actionCheckHandle(TimerHandle_t xTimer)
{
    // 标志需要检测动作
    isCheckAction = true;
}

void setup()
{
    Serial.begin(115200);

    Serial.println(F("\nAIO (All in one) version " AIO_VERSION "\n"));
    // MAC ID可用作芯片唯一标识
    Serial.print(F("ChipID(EfuseMac): "));
    Serial.println(ESP.getEfuseMac());

    app_controller = new AppController(); // APP控制器

    // 需要放在Setup里初始化
    if (!SPIFFS.begin(true))
    {
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    // config_read(NULL, &g_cfg);   // 旧的配置文件读取方式
    app_controller->read_config(&app_controller->sys_cfg);
    app_controller->read_config(&app_controller->mpu_cfg);
    // app_controller->read_config(&app_controller->rgb_cfg);

    /*** Init screen ***/
    screen.init(3,//app_controller->sys_cfg.rotation,
                app_controller->sys_cfg.backLight);

    /*** Init micro SD-Card ***/
    tf.init();
    lv_fs_if_init();

    app_controller->init();
    // 将APP"安装"到controller里
    app_controller->app_install(&weather_app);
    // app_controller->app_install(&weather_old_app);
    app_controller->app_install(&picture_app);
    // app_controller->app_install(&media_app);
    app_controller->app_install(&screen_share_app);
    app_controller->app_install(&file_manager_app);
    app_controller->app_install(&server_app);
    // app_controller->app_install(&idea_app);
    app_controller->app_install(&bilibili_app);
    app_controller->app_install(&settings_app);
    app_controller->app_install(&audio_spectrum_app);
    app_controller->app_install(&anniversary_app);
    // app_controller->app_install(&heartbeat_app, APP_TYPE_BACKGROUND);
    
    app_controller->main_process(&mpu.action_info);
    /*** Init IMU as input device ***/
    lv_port_indev_init();
    mpu.init(app_controller->sys_cfg.mpu_order,
             app_controller->sys_cfg.auto_calibration_mpu,
             &app_controller->mpu_cfg); // 初始化比较耗时

    // 定义一个mpu6050的动作检测定时器
    xTimerAction = xTimerCreate("Action Check",
                                200 / portTICK_PERIOD_MS,
                                pdTRUE, (void *)0, actionCheckHandle);
    xTimerStart(xTimerAction, 0);
    act_info = mpu.getAction();
}

void loop()
{
    screen.routine();
    if (isCheckAction)
    {
        isCheckAction = false;
        act_info = mpu.getAction();
    }
    app_controller->main_process(act_info); // 运行当前进程
}
