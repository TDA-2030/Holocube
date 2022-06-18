

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/socket.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include <unistd.h>
#include "screen_share.h"
#include "screen_share_gui.h"
#include "sys/app_controller.h"
#include "driver/display.h"
#include "esp_check.h"
#include "mjpeg.h"

static const char *TAG = "screen share";

#define SCREEN_SHARE_APP_NAME "Screen share"

#define RECV_BUFFER_SIZE 50*1024   // 理论上是JPEG_BUFFER_SIZE的两倍就够了

LV_IMG_DECLARE(app_screen);
extern Display screen;     // 屏幕对象


struct ScreenShareAppRunData {
    TaskHandle_t taskhandle;
    int listen_socket;                             // our masterSocket(socket that listens for RTSP client connections)
    int client_socket;

    bool tcp_start; // 标志是否开启web server服务，0为关闭 1为开启

    uint8_t *recvBuf;              // 接收缓冲区
    uint8_t *mjpeg_start;          // 指向一帧mpjeg的图片的开头
    uint8_t *mjpeg_end;            // 指向一帧mpjeg的图片的结束
    int32_t pos;           // 指向 recvBuf 中所保存的最后一个数据所在下标


    unsigned long pre_wifi_alive_millis; // 上一次发送维持心跳的本地时间戳
};

static ScreenShareAppRunData *run_data = NULL;

// This next function will be called during decoding of the jpeg file to render each
// 16x16 or 8x8 image tile (Minimum Coding Unit) to the screen.
static void screen_share_tft_output(int x, int y, int w, int h, uint16_t *bitmap)
{
    // Stop further decoding as image is running off bottom of screen
    if (y >= screen.height()) {
        return ;
    }

    // STM32F767 processor takes 43ms just to decode (and not draw) jpeg (-Os compile option)
    // Total time to decode and also draw to TFT:
    // SPI 54MHz=71ms, with DMA 50ms, 71-43 = 28ms spent drawing, so DMA is complete before next MCU block is ready
    // Apparent performance benefit of DMA = 71/50 = 42%, 50 - 43 = 7ms lost elsewhere
    // SPI 27MHz=95ms, with DMA 52ms. 95-43 = 52ms spent drawing, so DMA is *just* complete before next MCU block is ready!
    // Apparent performance benefit of DMA = 95/52 = 83%, 52 - 43 = 9ms lost elsewhere

    screen.pushImage(x, y, w, h, bitmap); // Initiate DMA - blocking only if last DMA is not complete
    // The DMA transfer of image block to the TFT is now in progress...
}

// static bool readJpegFromBuffer(uint8_t *const end)
// {
//     // 默认从 run_data->recvBuf 中读数据
//     // end标记的当前最后一个数据的地址
//     bool isFound = false;                     // 标志着是否找到一帧完整的mjpeg图像数据
//     uint8_t *pfind = run_data->last_find_pos; // 开始查找的指针
//     if (NULL == run_data->mjpeg_start) {
//         // 没找到帧头的时候执行
//         while (pfind < end) {
//             if (*pfind == 0xFF && *(pfind + 1) == 0xD8) {
//                 run_data->mjpeg_start = pfind; // 帧头
//                 break;
//             }
//             ++pfind;
//         }
//         run_data->last_find_pos = pfind;
//     } else if (NULL == run_data->mjpeg_end) {
//         // 找帧尾
//         while (pfind < end) {
//             if (*pfind == 0xFF && *(pfind + 1) == 0xD9) {
//                 run_data->mjpeg_end = pfind + 1; // 帧头，标记的是最后一个0xD9
//                 isFound = true;
//                 break;
//             }
//             ++pfind;
//         }
//         run_data->last_find_pos = pfind;
//     }
//     return isFound;
// }


static esp_err_t ss_create_server(ScreenShareAppRunData *session)
{
    uint16_t port = 8081;

    struct sockaddr_in ServerAddr;  // server address parameters
    ServerAddr.sin_family      = AF_INET;
    ServerAddr.sin_addr.s_addr = INADDR_ANY;
    ServerAddr.sin_port        = htons(port); // listen on port
    session->listen_socket     = socket(AF_INET, SOCK_STREAM, 0);
    ESP_RETURN_ON_FALSE(session->listen_socket > 0, ESP_FAIL, TAG, "Unable to create socket");

    int enable = 1;
    int res = 0;
    res = setsockopt(session->listen_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    ESP_RETURN_ON_FALSE(res == 0, ESP_FAIL, TAG, "setsockopt(SO_REUSEADDR) failed");

    // bind our listen socket to the port and listen for a client connection
    res = bind(session->listen_socket, (struct sockaddr *)&ServerAddr, sizeof(ServerAddr));
    ESP_RETURN_ON_FALSE(res == 0, ESP_FAIL, TAG, "error can't bind port");
    res = listen(session->listen_socket, 5);
    ESP_RETURN_ON_FALSE(res == 0, ESP_FAIL, TAG, "error can't listen socket (%d:%s)", errno, strerror(errno));
}

int socketrecv(int sock, char *buf, size_t buflen)
{
    int res = recv(sock, buf, buflen, 0);
    if (res > 0) {
        return res;
    } else if (res == 0) {
        return 0; // client dropped connection
    } else {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return -1;
        } else {
            return 0;    // unknown error, just claim client dropped it
        }
    };
}
ssize_t socketsend(int sockfd, const void *buf, size_t len)
{
    // ESP_LOGE(TAG, "TCP send\n");
    return send(sockfd, buf, len, 0);
}

typedef struct {
    uint32_t head;
    uint32_t size;
    uint64_t timestamp;
} frame_t;

static void ss_task(void *args)
{
    ScreenShareAppRunData *session = (ScreenShareAppRunData *)args;

    ss_create_server(session);

    tcpip_adapter_ip_info_t if_ip_info;
    char ip_str[64] = {0};
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &if_ip_info);
    sprintf(ip_str, "ipv4:%d.%d.%d.%d", IP2STR(&if_ip_info.ip));
    ESP_LOGI(TAG, "Creating session [%s:%hu]", ip_str, 8081);
    while (1) {

        display_screen_share(TAG, ip_str, "8081", "info", LV_SCR_LOAD_ANIM_NONE);

        ESP_LOGI(TAG, "waiting for client");
        struct sockaddr_in clientaddr;                                   // address parameters of a new RTSP client
        socklen_t clientaddrLen = sizeof(clientaddr);
        session->client_socket = accept(session->listen_socket, (struct sockaddr *)&clientaddr, &clientaddrLen);
        ESP_LOGI(TAG, "Client connected. Client address: %s", inet_ntoa(clientaddr.sin_addr));
        session->pos = 0;
        size_t frame_size = 0;
        uint8_t step = 0;
        while (1) {

            char *buffer = (char *)session->recvBuf;
            memset(buffer, 0x00, RECV_BUFFER_SIZE);
            int res = socketrecv(session->client_socket, buffer, sizeof(frame_t));
            if (res > 0) {
                // printf("recv[");
                // char *p = buffer + session->pos;
                // for (size_t i = 0; i < res; i++) {
                //     printf("%x, ", p[i]);
                // } printf("]\n");

                frame_t *frame = (frame_t *)buffer;
                if ('$' != frame->head) {
                    ESP_LOGE(TAG, "frame head error[%x, %x, %x]", frame->head, frame->size, frame->timestamp);
                } else {
                    ESP_LOGI(TAG, "head ok size=%d", frame->size);
                    session->pos = 0;
                    frame_size = frame->size;
                    res = socketrecv(session->client_socket, buffer, frame_size);
                    if (res > 0) {
                        ESP_LOGI(TAG, "reaced len=%d", res);
                        // 在左上角的0,0处绘制图像——在这个草图中，DMA请求在回调tft_output()中处理
                        mjpegdraw((const uint8_t *)buffer, frame_size, NULL, screen_share_tft_output);
                    } else if (0 == res) {
                        ESP_LOGI(TAG, "client closed socket, exiting");
                        session->pos = 0;
                        frame_size = 0;
                        step = 0;
                        break;
                    }
                }


                // session->pos += res;
                // if (0 == step) {
                //     frame_t *frame = (frame_t *)buffer;
                //     if ('$' != frame->head) {
                //         ESP_LOGE(TAG, "frame head error[%x, %x, %x]", frame->head, frame->size, frame->timestamp);
                //     } else {
                //         ESP_LOGI(TAG, "head ok size=%d", frame->size);
                //         step = 1;
                //         session->pos = 0;
                //         frame_size = frame->size;
                //     }
                // } else if (1 == step) {
                //     if (session->pos >= frame_size) {
                //         ESP_LOGI(TAG, "reaced len=%d", session->pos);
                //         // 在左上角的0,0处绘制图像——在这个草图中，DMA请求在回调tft_output()中处理
                //         JRESULT jpg_ret = TJpgDec.drawJpg(0, 0, session->mjpeg_start, frame_size);
                //         socketsend(session->client_socket, "ok", 3);
                //         step = 0;
                //         session->pos = 0;
                //     }
                // }

            } else if (0 == res) {
                ESP_LOGI(TAG, "client closed socket, exiting");
                session->pos = 0;
                frame_size = 0;
                step = 0;
                break;
            }
        }
    }
}

static int screen_share_init(AppController *sys)
{


    screen_share_gui_init();
    // 初始化运行时参数
    run_data = (ScreenShareAppRunData *)calloc(1, sizeof(ScreenShareAppRunData));
    run_data->recvBuf = (uint8_t *)malloc(RECV_BUFFER_SIZE);
    run_data->mjpeg_start = NULL;
    run_data->mjpeg_end = NULL;

    run_data->pre_wifi_alive_millis = 0;


    // // The decoder must be given the exact name of the rendering function above
    // SketchCallback callback = (SketchCallback)&screen_share_tft_output; //强制转换func()的类型
    // TJpgDec.setCallback(callback);
    // // The jpeg image can be scaled down by a factor of 1, 2, 4, or 8
    // TJpgDec.setJpgScale(1);

    // // 因为其他app里是对tft直接设置的，所以此处尽量了不要使用TJpgDec的setSwapBytes
    // TJpgDec.setSwapBytes(true);

    xTaskCreate(ss_task, "ss_task", 4096, run_data, 10, &run_data->taskhandle);

    return 0;
}

static void screen_share_process(AppController *sys)
{
    APP_ACTIVE_TYPE act_info = sys->act_info;
    lv_scr_load_anim_t anim_type = LV_SCR_LOAD_ANIM_NONE;

    if (APP_RETURN == act_info) {
        sys->app_exit();
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(500));

}

static void screen_background_task(AppController *sys)
{
    // 本函数为后台任务，主控制器会间隔一分钟调用此函数
    // 本函数尽量只调用"常驻数据",其他变量可能会因为生命周期的缘故已经释放
}

static int screen_exit_callback(AppController *sys)
{
    vTaskDelete(run_data->taskhandle);
    screen_share_gui_del();
    if (NULL != run_data->recvBuf) {
        free(run_data->recvBuf);
        run_data->recvBuf = NULL;
    }

    // 释放运行数据
    if (NULL != run_data) {
        free(run_data);
        run_data = NULL;
    }
    return 0;
}

static void screen_message_handle(const char *from, const char *to,
                                  APP_MESSAGE_TYPE type, void *message,
                                  void *ext_info)
{

}

APP_OBJ screen_share_app = {SCREEN_SHARE_APP_NAME, &app_screen, "",
                            screen_share_init, screen_share_process, screen_background_task,
                            screen_exit_callback, screen_message_handle
                           };
