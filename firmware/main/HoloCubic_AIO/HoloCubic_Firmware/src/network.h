#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <stdint.h>

#ifdef __cplusplus

class Network {
private:
    unsigned long m_preDisWifiConnInfoMillis; // 保存上一回显示连接状态的时间戳

public:
    Network();
    void config(void);
    bool isconnected(void);
    // void search_wifi(void);
    // boolean start_conn_wifi(const char *ssid, const char *password);
    // boolean end_conn_wifi(void);
    // boolean close_wifi(void);
    // boolean open_ap(const char *ap_ssid = AP_SSID, const char *ap_password = NULL);
};

#endif

#endif
