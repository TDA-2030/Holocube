#ifndef APP_HEARBEAT_H
#define APP_HEARBEAT_H

#include "sys/interface.h"

#ifndef MQTT_SEND_MSG
#define MQTT_SEND_MSG "HeartBeat"
#endif


extern APP_OBJ heartbeat_app;

#endif