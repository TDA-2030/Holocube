#ifndef __IMU_MPU6050_H__
#define __IMU_MPU6050_H__

#include <stdint.h>

#ifdef __cplusplus

#include <MPU6050.h>

#define ACTION_HISTORY_BUF_LEN 5


enum ACTIVE_TYPE {
    TURN_RIGHT = 0,
    RETURN,
    TURN_LEFT,
    UP,
    DOWN,
    GO_FORWORD,
    SHAKE,
    UNKNOWN
};

// 方向类型
enum MPU_DIR_TYPE {
    NORMAL_DIR_TYPE = 0,
    X_DIR_TYPE = 0x01,
    Y_DIR_TYPE = 0x02,
    Z_DIR_TYPE = 0x04,
    XY_DIR_TYPE = 0x08
};

struct SysMpuConfig {
    int16_t x_gyro_offset;
    int16_t y_gyro_offset;
    int16_t z_gyro_offset;

    int16_t x_accel_offset;
    int16_t y_accel_offset;
    int16_t z_accel_offset;
};

struct ImuAction {
    volatile ACTIVE_TYPE active;
    bool isValid;
    bool long_time;
    int16_t v_ax; // v表示虚拟参数（用于调整6050的初始方位）
    int16_t v_ay;
    int16_t v_az;
    int16_t v_gx;
    int16_t v_gy;
    int16_t v_gz;
};

class IMU {
private:
    MPU6050 mpu;
    long last_update_time;
    uint8_t order; // 表示方位，x与y是否对换
    ImuAction action_info;
    void getVirtureMotion6(ImuAction *action_info);

    ACTIVE_TYPE act_info_history[ACTION_HISTORY_BUF_LEN];
    int act_info_history_ind; // 标志储存的位置

public:
    IMU();
    void init(uint8_t order, uint8_t auto_calibration);
    const char *getActionname(ACTIVE_TYPE active);
    void setOrder(uint8_t order); // 设置方向
    void update_attitude(void);
    ImuAction *getAction(void); // 获取动作
};

#endif

#endif
