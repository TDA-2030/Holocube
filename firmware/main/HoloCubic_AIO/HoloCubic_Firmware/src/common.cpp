#include "common.h"
#include "network.h"

IMU mpu;
Disk_FS tf(SD);
Disk_FS g_flashCfg(SPIFFS);

Network g_network;  // 网络连接
Display screen;     // 屏幕对象

boolean doDelayMillisTime(unsigned long interval, unsigned long *previousMillis, boolean state)
{
    unsigned long currentMillis = millis();
    if (currentMillis - *previousMillis >= interval)
    {
        *previousMillis = currentMillis;
        state = !state;
    }
    return state;
}
