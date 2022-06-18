#include "weather.h"
#include "weather_gui.h"
#include "ESP32Time.h"
#include "sys/app_controller.h"
#include "network.h"
#include "common.h"
#include "ArduinoJson.h"
#include <esp32-hal-timer.h>
#include <map>

#define TAG WEATHER_APP_NAME
#define WEATHER_APP_NAME "Weather"
#define WEATHER_NOW_API "https://www.yiketianqi.com/free/day?appid=%s&appsecret=%s&unescape=1&city=%s"
#define WEATHER_NOW_API_UPDATE "https://yiketianqi.com/api?unescape=1&version=v6&appid=%s&appsecret=%s&city=%s"
#define WEATHER_DALIY_API "https://www.yiketianqi.com/free/week?unescape=1&appid=%s&appsecret=%s&city=%s"
#define TIME_API "http://api.m.taobao.com/rest/api3.do?api=mtop.common.gettimestamp"
#define WEATHER_PAGE_SIZE 2
#define UPDATE_WEATHER 0x01       // 更新天气
#define UPDATE_DALIY_WEATHER 0x02 // 更新每天天气
#define UPDATE_TIME 0x04          // 更新时间

LV_IMG_DECLARE(app_weather);

// 天气的持久化配置
#define WEATHER_CONFIG_PATH "/weather.cfg"
struct WT_Config {
    String tianqi_appid;                 // tianqiapid 的 appid
    String tianqi_appsecret;             // tianqiapid 的 appsecret
    String tianqi_addr;                  // tianqiapid 的地址（填中文）
    unsigned long weatherUpdataInterval; // 天气更新的时间间隔(s)
    unsigned long timeUpdataInterval;    // 日期时钟更新的时间间隔(s)
};

static void write_config(WT_Config *cfg)
{
    char tmp[16];
    // 将配置数据保存在文件中（持久化）
    String w_data;
    w_data = w_data + cfg->tianqi_appid + "\n";
    w_data = w_data + cfg->tianqi_appsecret + "\n";
    w_data = w_data + cfg->tianqi_addr + "\n";
    memset(tmp, 0, 16);
    snprintf(tmp, 16, "%u\n", cfg->weatherUpdataInterval);
    w_data += tmp;
    memset(tmp, 0, 16);
    snprintf(tmp, 16, "%u\n", cfg->timeUpdataInterval);
    w_data += tmp;
    g_flashCfg.writeFile(WEATHER_CONFIG_PATH, w_data.c_str());
}

static void read_config(WT_Config *cfg)
{
    // 如果有需要持久化配置文件 可以调用此函数将数据存在flash中
    // 配置文件名最好以APP名为开头 以".cfg"结尾，以免多个APP读取混乱
    char info[128] = {0};
    uint16_t size = g_flashCfg.readFile(WEATHER_CONFIG_PATH, (uint8_t *)info);
    info[size] = 0;
    if (size == 0) {
        // 默认值
        cfg->tianqi_addr = "北京";
        cfg->weatherUpdataInterval = 900000; // 天气更新的时间间隔900000(900s)
        cfg->timeUpdataInterval = 900000;    // 日期时钟更新的时间间隔900000(900s)
        write_config(cfg);
    } else {
        // 解析数据
        char *param[5] = {0};
        analyseParam(info, 5, param);
        cfg->tianqi_appid = param[0];
        cfg->tianqi_appsecret = param[1];
        cfg->tianqi_addr = param[2];
        cfg->weatherUpdataInterval = atol(param[3]);
        cfg->timeUpdataInterval = atol(param[4]);
    }
}

struct WeatherAppRunData {
    unsigned long preWeatherMillis; // 上一回更新天气时的毫秒数
    unsigned long preTimeMillis;    // 更新时间计数器
    long long preNetTimestamp;      // 上一次的网络时间戳
    long long errorNetTimestamp;    // 网络到显示过程中的时间误差
    long long preLocalTimestamp;    // 上一次的本地机器时间戳
    unsigned int coactusUpdateFlag; // 强制更新标志
    int clock_page;
    unsigned int update_type; // 更新类型的标志位

    BaseType_t xReturned_task_task_update; // 更新数据的异步任务
    TaskHandle_t xHandle_task_task_update; // 更新数据的异步任务

    Weather wea;     // 保存天气状况
};

static WT_Config cfg_data;
static WeatherAppRunData *run_data = NULL;

enum wea_event_Id {
    UPDATE_NOW,
    UPDATE_NTP,
    UPDATE_DAILY
};

std::map<String, int> weatherMap = {{"qing", 0}, {"yin", 1}, {"yu", 2}, {"yun", 3}, {"bingbao", 4}, {"wu", 5}, {"shachen", 6}, {"lei", 7}, {"xue", 8}};

static void task_update(void *parameter); // 异步更新任务

static int windLevelAnalyse(String str)
{
    int ret = 0;
    for (char ch : str) {
        if (ch >= '0' && ch <= '9') {
            ret = ret * 10 + (ch - '0');
        }
    }
    return ret;
}

static void get_weather(void)
{

}


static void get_daliyWeather(short maxT[], short minT[])
{

}

static void UpdateTime_RTC(void)
{
    struct TimeStr t;

    struct tm timeinfo;
    time_t now;
    time(&now);
    localtime_r(&now, &timeinfo);

    t.month = timeinfo.tm_mon + 1;
    t.day = timeinfo.tm_mday;
    t.hour = timeinfo.tm_hour;
    t.minute = timeinfo.tm_min;
    t.second = timeinfo.tm_sec;
    t.weekday = timeinfo.tm_wday;
    // Serial.printf("time : %d-%d-%d\n",t.hour, t.minute, t.second);
    display_time(t, LV_SCR_LOAD_ANIM_NONE);
}

static int weather_init(AppController *sys)
{
    screen.setSwapBytes(true);
    weather_gui_init();
    // 获取配置信息
    read_config(&cfg_data);

    // 初始化运行时参数
    run_data = (WeatherAppRunData *)calloc(1, sizeof(WeatherAppRunData));
    ESP_RETURN_ON_FALSE(NULL != run_data, 0, __FUNCTION__, "no mem for weather app");
    memset((char *)&run_data->wea, 0, sizeof(Weather));
    run_data->preNetTimestamp = 1577808000000; // 上一次的网络时间戳 初始化为2020-01-01 00:00:00
    run_data->errorNetTimestamp = 2;
    run_data->preLocalTimestamp = millis(); // 上一次的本地机器时间戳
    run_data->clock_page = 0;
    run_data->preWeatherMillis = 0;
    run_data->preTimeMillis = 0;
    // 强制更新
    run_data->coactusUpdateFlag = 0x01;
    run_data->update_type = 0x00; // 表示什么也不需要更新

    return 0;
}

static void weather_process(AppController *sys)
{
    APP_ACTIVE_TYPE act_info = sys->act_info;
    lv_scr_load_anim_t anim_type = LV_SCR_LOAD_ANIM_NONE;

    if (APP_RETURN == act_info) {
        sys->app_exit();
        return;
    } else if (APP_GO_FORWORD == act_info) {
        // 间接强制更新
        run_data->coactusUpdateFlag = 0x01;
        delay(500); // 以防间接强制更新后，生产很多请求 使显示卡顿
    } else if (APP_TURN_RIGHT == act_info) {
        anim_type = LV_SCR_LOAD_ANIM_MOVE_RIGHT;
        run_data->clock_page = (run_data->clock_page + 1) % WEATHER_PAGE_SIZE;
    } else if (APP_TURN_LEFT == act_info) {
        anim_type = LV_SCR_LOAD_ANIM_MOVE_LEFT;
        // 以下等效与 clock_page = (clock_page + WEATHER_PAGE_SIZE - 1) % WEATHER_PAGE_SIZE;
        // +3为了不让数据溢出成负数，而导致取模逻辑错误
        run_data->clock_page = (run_data->clock_page + WEATHER_PAGE_SIZE - 1) % WEATHER_PAGE_SIZE;
    }

    // 界面刷新
    if (run_data->clock_page == 0) {
        // lvgl_acquire();
        display_weather(run_data->wea, anim_type);
        // lvgl_release();
        if (0x01 == run_data->coactusUpdateFlag || doDelayMillisTime(cfg_data.weatherUpdataInterval, &run_data->preWeatherMillis, false)) {
            sys->send_to(WEATHER_APP_NAME, CTRL_NAME, APP_MESSAGE_WIFI_CONN, (void *)UPDATE_NOW, NULL);
            sys->send_to(WEATHER_APP_NAME, CTRL_NAME, APP_MESSAGE_WIFI_CONN, (void *)UPDATE_DAILY, NULL);
        }

        if (0x01 == run_data->coactusUpdateFlag || doDelayMillisTime(cfg_data.timeUpdataInterval, &run_data->preTimeMillis, false)) {
            // 尝试同步网络上的时钟
            sys->send_to(WEATHER_APP_NAME, CTRL_NAME, APP_MESSAGE_WIFI_CONN, (void *)UPDATE_NTP, NULL);
        } else if (millis() - run_data->preLocalTimestamp > 400) {
            UpdateTime_RTC();
        }
        run_data->coactusUpdateFlag = 0x00; // 取消强制更新标志
        // lvgl_acquire();
        display_space();
        // lvgl_release();
        delay(30);
    } else if (run_data->clock_page == 1) {
        // 仅在切换界面时获取一次未来天气
        display_curve(run_data->wea.daily_max, run_data->wea.daily_min, anim_type);
        delay(300);
    }
}

static void weather_background_task(AppController *sys)
{
    // 本函数为后台任务，主控制器会间隔一分钟调用此函数
    // 本函数尽量只调用"常驻数据",其他变量可能会因为生命周期的缘故已经释放
}

static int weather_exit_callback(AppController *sys)
{
    weather_gui_del();

    // 查杀异步任务
    if (run_data->xReturned_task_task_update == pdPASS) {
        vTaskDelete(run_data->xHandle_task_task_update);
    }

    // 释放运行数据
    if (NULL != run_data) {
        free(run_data);
        run_data = NULL;
    }
    return 0;
}

static void weather_message_handle(const char *from, const char *to,
                                   APP_MESSAGE_TYPE type, void *message,
                                   void *ext_info)
{
    ESP_LOGI(__FUNCTION__, "%s:%d", __FILE__, __LINE__);
    switch (type) {
    case APP_MESSAGE_WIFI_CONN: {
        Serial.println(F("----->weather_event_notification"));
        int event_id = (int)message;
        switch (event_id) {
        case UPDATE_NOW: {
            ESP_LOGI(TAG, "weather update.");
            run_data->update_type |= UPDATE_WEATHER;

            get_weather();
            if (run_data->clock_page == 0) {
                display_weather(run_data->wea, LV_SCR_LOAD_ANIM_NONE);
            }
        };
        break;
        case UPDATE_NTP: {
            ESP_LOGI(TAG, "ntp update.");
            run_data->update_type |= UPDATE_TIME;

            // long long timestamp = get_timestamp(TIME_API); // nowapi时间API
            // if (run_data->clock_page == 0) {
            //     UpdateTime_RTC(timestamp);
            // }
        };
        break;
        case UPDATE_DAILY: {
            ESP_LOGI(TAG, "daliy update.");
            run_data->update_type |= UPDATE_DALIY_WEATHER;

            get_daliyWeather(run_data->wea.daily_max, run_data->wea.daily_min);
            if (run_data->clock_page == 1) {
                display_curve(run_data->wea.daily_max, run_data->wea.daily_min, LV_SCR_LOAD_ANIM_NONE);
            }
        };
        break;
        default:
            break;
        }
    }
    break;
    case APP_MESSAGE_GET_PARAM: {
        char *param_key = (char *)message;
        if (!strcmp(param_key, "tianqi_appid")) {
            snprintf((char *)ext_info, 32, "%s", cfg_data.tianqi_appid.c_str());
        } else if (!strcmp(param_key, "tianqi_appsecret")) {
            snprintf((char *)ext_info, 32, "%s", cfg_data.tianqi_appsecret.c_str());
        } else if (!strcmp(param_key, "tianqi_addr")) {
            snprintf((char *)ext_info, 32, "%s", cfg_data.tianqi_addr.c_str());
        } else if (!strcmp(param_key, "weatherUpdataInterval")) {
            snprintf((char *)ext_info, 32, "%u", cfg_data.weatherUpdataInterval);
        } else if (!strcmp(param_key, "timeUpdataInterval")) {
            snprintf((char *)ext_info, 32, "%u", cfg_data.timeUpdataInterval);
        } else {
            snprintf((char *)ext_info, 32, "%s", "NULL");
        }
    }
    break;
    case APP_MESSAGE_SET_PARAM: {
        char *param_key = (char *)message;
        char *param_val = (char *)ext_info;
        if (!strcmp(param_key, "tianqi_appid")) {
            cfg_data.tianqi_appid = param_val;
        } else if (!strcmp(param_key, "tianqi_appsecret")) {
            cfg_data.tianqi_appsecret = param_val;
        } else if (!strcmp(param_key, "tianqi_addr")) {
            cfg_data.tianqi_addr = param_val;
        } else if (!strcmp(param_key, "weatherUpdataInterval")) {
            cfg_data.weatherUpdataInterval = atol(param_val);
        } else if (!strcmp(param_key, "timeUpdataInterval")) {
            cfg_data.timeUpdataInterval = atol(param_val);
        }
    }
    break;
    case APP_MESSAGE_READ_CFG: {
        read_config(&cfg_data);
    }
    break;
    case APP_MESSAGE_WRITE_CFG: {
        write_config(&cfg_data);
    }
    break;
    default:
        break;
    }
}

APP_OBJ weather_app = {WEATHER_APP_NAME, &app_weather, "",
                       weather_init, weather_process, weather_background_task,
                       weather_exit_callback, weather_message_handle
                      };