#ifndef APP_CONTROLLER_H
#define APP_CONTROLLER_H

#include "stdint.h"
#include <list>

#define CTRL_NAME "AppCtrl"
#define APP_MAX_NUM 20             // 最大的可运行的APP数量
#define WIFI_LIFE_CYCLE 60000      // wifi的生命周期（60s）
#define MQTT_ALIVE_CYCLE 1000      // mqtt重连周期
#define EVENT_LIST_MAX_LENGTH 10   // 消息队列的容量
#define APP_CONTROLLER_NAME_LEN 16 // app控制器的名字长度

enum APP_ACTIVE_TYPE {
    APP_TURN_RIGHT = 0,
    APP_RETURN,
    APP_TURN_LEFT,
    APP_UP,
    APP_DOWN,
    APP_GO_FORWORD,
    APP_SHAKE,
    APP_UNKNOWN
};

struct SysUtilConfig {

    uint8_t power_mode;           // 功耗模式（0为节能模式 1为性能模式）
    uint8_t backLight;            // 屏幕亮度（1-100）
    uint8_t rotation;             // 屏幕旋转方向
    uint8_t auto_calibration_mpu; // 是否自动校准陀螺仪 0关闭自动校准 1打开自动校准
    uint8_t mpu_order;            // 操作方向
};


enum APP_MESSAGE_TYPE {
    APP_MESSAGE_WIFI_CONN = 0, // 开启连接
    APP_MESSAGE_WIFI_AP,       // 开启AP事件
    APP_MESSAGE_WIFI_ALIVE,    // wifi开关的心跳维持
    APP_MESSAGE_WIFI_DISCONN,  // 连接断开
    APP_MESSAGE_UPDATE_TIME,
    APP_MESSAGE_MQTT_DATA, // MQTT客户端收到消息
    APP_MESSAGE_GET_PARAM, // 获取参数
    APP_MESSAGE_SET_PARAM, // 设置参数
    APP_MESSAGE_READ_CFG,  // 向磁盘读取参数
    APP_MESSAGE_WRITE_CFG, // 向磁盘写入参数

    APP_MESSAGE_NONE
};

enum APP_TYPE {
    APP_TYPE_REAL_TIME = 0, // 实时应用
    APP_TYPE_BACKGROUND,    // 后台应用

    APP_TYPE_NONE
};

class AppController;
struct ImuAction;

struct APP_OBJ {
    // 应用程序名称 及title
    const char *app_name;

    // APP的图片存放地址    APP应用图标 128*128
    const void *app_image;

    // 应用程序的其他信息 如作者、版本号等等
    const char *app_info;

    // APP的初始化函数 也可以为空或什么都不做（作用等效于arduino setup()函数）
    int (*app_init)(AppController *sys);

    // APP的主程序函数入口指针
    void (*main_process)(AppController *sys);

    // APP的任务的入口指针（一般一分钟内会调用一次）
    void (*background_task)(AppController *sys);

    // 退出之前需要处理的回调函数 可为空
    int (*exit_callback)(AppController *sys);

    // 消息处理机制
    void (*message_handle)(const char *from, const char *to, APP_MESSAGE_TYPE type, void *message, void *ext_info);
};

struct EVENT_OBJ {
    const APP_OBJ *from;       // 发送请求服务的APP
    APP_MESSAGE_TYPE type;     // app的事件类型
    void *info;                // 请求携带的信息
    uint8_t retryMaxNum;       // 重试次数
    uint8_t retryCount;        // 重试计数
    unsigned long nextRunTime; // 下次运行的时间戳
};

bool analyseParam(char *info, int argc, char **argv);

class AppController {
public:
    AppController(const char *name = CTRL_NAME);
    ~AppController();
    void init(void);
    void Display(void); // 显示接口
    // 将APP注册到app_controller中
    int app_install(APP_OBJ *app,
                    APP_TYPE app_type = APP_TYPE_REAL_TIME);
    // 将APP从app_controller中卸载（删除）
    int app_uninstall(const APP_OBJ *app);
    // 将APP的后台任务从任务队列中移除(自能通过APP退出的时候，移除自身的后台任务)
    int remove_backgroud_task(void);
    int main_process(APP_ACTIVE_TYPE active);
    void app_exit(void); // 提供给app退出的系统调用
    // 消息发送
    int send_to(const char *from, const char *to, APP_MESSAGE_TYPE type, void *message, void *ext_info);
    void deal_config(APP_MESSAGE_TYPE type, const char *key, char *value);
    // 事件处理
    int req_event_deal(void);
    void read_config(SysUtilConfig *cfg);
    void write_config(SysUtilConfig *cfg);


private:
    APP_OBJ *getAppByName(const char *name);
    int getAppIdxByName(const char *name);
    int app_is_legal(const APP_OBJ *app_obj);

private:
    char name[APP_CONTROLLER_NAME_LEN]; // app控制器的名字
    APP_OBJ *appList[APP_MAX_NUM];      // 预留APP_MAX_NUM个APP注册位
    APP_TYPE appTypeList[APP_MAX_NUM];  // 对应APP的运行类型
    // std::list<const APP_OBJ *> app_list; // APP注册位(为了C语言可移植，放弃使用链表)
    std::list<EVENT_OBJ> eventList;   // 用来储存事件
    bool m_wifi_status;            // 表示是wifi状态 true开启 false关闭
    unsigned long m_preWifiReqMillis; // 保存上一回请求的时间戳
    unsigned int app_num;
    bool app_exit_flag; // 表示是否退出APP应用
    int cur_app_index;     // 当前运行的APP下标
    int pre_app_index;     // 上一次运行的APP下标

public:
    SysUtilConfig sys_cfg;
    APP_ACTIVE_TYPE act_info;
};

#endif