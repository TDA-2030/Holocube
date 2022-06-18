#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdint.h>

#ifdef __cplusplus

#define AIO_VERSION "2.0.7"

#include "Arduino.h"
#include "driver/disk_fs.h"
#include "driver/display.h"
#include "driver/imu.h"
#include "network.h"
#include "esp_log.h"
#include "esp_check.h"

// MUP6050
#define IMU_I2C_SDA 17
#define IMU_I2C_SCL 16

extern IMU mpu; // 原则上只提供给主程序调用
extern Disk_FS tf;
extern Disk_FS g_flashCfg;

extern Network g_network;  // 网络连接
// extern FlashFS g_flashCfg; // flash中的文件系统（替代原先的Preferences）
extern Display screen;     // 屏幕对象

boolean doDelayMillisTime(unsigned long interval,
                          unsigned long *previousMillis,
                          boolean state);


// TFT屏幕接口
#define LCD_BL_PIN 38
#define LCD_BL_PWM_CHANNEL 0

#define DEBUG_LINE ESP_LOGI(__FUNCTION__, "%s:%d", __FILE__, __LINE__)

#define VSPI_HOST SPI2_HOST

#endif

#endif