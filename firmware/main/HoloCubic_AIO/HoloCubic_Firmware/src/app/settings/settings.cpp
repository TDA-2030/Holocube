#include "settings.h"
#include "settings_gui.h"
#include "sys/app_controller.h"
#include "common.h"

#define SETTINGS_APP_NAME "Settings"
#define RECV_BUF_LEN 128

struct SettingsAppRunData {
    uint8_t *recv_buf;
    uint16_t recv_len;
};

static SettingsAppRunData *run_data = NULL;
LV_IMG_DECLARE(app_settings);

static int settings_init(AppController *sys)
{
    // 初始化运行时的参数
    settings_gui_init();

    display_settings(AIO_VERSION, "v 2.0.0", LV_SCR_LOAD_ANIM_NONE);

    // 初始化运行时参数
    run_data = (SettingsAppRunData *)calloc(1, sizeof(SettingsAppRunData));
    run_data->recv_buf = (uint8_t *)malloc(RECV_BUF_LEN);
    run_data->recv_len = 0;
    return 0;
}

static void settings_process(AppController *sys, APP_ACTIVE_TYPE act_info)
{
    if (APP_RETURN == act_info) {
        sys->app_exit(); // 退出APP
        return;
    }

    if (Serial.available()) {
        uint16_t len = Serial.read(run_data->recv_buf + run_data->recv_len,
                                   RECV_BUF_LEN - run_data->recv_len);

        run_data->recv_len += len;
        if (run_data->recv_len > 0) {
            Serial.print("rev = ");

            Serial.write(run_data->recv_buf, len);
        }
        delay(50);
    } else {
        delay(200);
    }

    // 发送请求，当请求完成后自动会调用 settings_event_notification 函数
    // sys->req_event(&settings_app, APP_MESSAGE_WIFI_CONN, run_data->val1);
    // 程序需要时可以适当加延时
    // delay(200);
}

static void settings_background_task(AppController *sys, APP_ACTIVE_TYPE act_info)
{
    // 本函数为后台任务，主控制器会间隔一分钟调用此函数
    // 本函数尽量只调用"常驻数据",其他变量可能会因为生命周期的缘故已经释放
}

static int settings_exit_callback(void *param)
{
    settings_gui_del();

    // 释放运行数据
    if (NULL != run_data) {
        free(run_data);
        run_data = NULL;
    }
    return 0;
}

static void settings_message_handle(const char *from, const char *to,
                                    APP_MESSAGE_TYPE type, void *message,
                                    void *ext_info)
{
    // 目前事件主要是wifi开关类事件（用于功耗控制）
    switch (type) {
    case APP_MESSAGE_WIFI_CONN: {
        // todo
    }
    break;
    case APP_MESSAGE_WIFI_AP: {
        // todo
    }
    break;
    case APP_MESSAGE_WIFI_ALIVE: {
        // wifi心跳维持的响应 可以不做任何处理
    }
    break;
    case APP_MESSAGE_GET_PARAM: {
        char *param_key = (char *)message;
    }
    break;
    case APP_MESSAGE_SET_PARAM: {
        char *param_key = (char *)message;
        char *param_val = (char *)ext_info;
    }
    break;
    default:
        break;
    }
}

APP_OBJ settings_app = {SETTINGS_APP_NAME, &app_settings, "",
                        settings_init, settings_process, settings_background_task,
                        settings_exit_callback, settings_message_handle
                       };
