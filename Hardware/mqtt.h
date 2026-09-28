#ifndef __MQTT_H
#define __MQTT_H
#include "stm32f4xx.h"

u8   MQTT_Connect(const char *clientID);    /* 返回0成功 */
u8   MQTT_Subscribe(const char *topic);
void MQTT_Publish(const char *topic, const char *payload);
void MQTT_Ping(void);                       /* 心跳, 30秒调一次 */
void MQTT_Process(void);                    /* 主循环调用, 解析云端下行消息 */
u8 MQTT_LinkEvent(void);    /* 查询是否发生过TCP断开(读后清零) */
u8 MQTT_CheckAlive(void);   /* 活性检测: 0=链路活着 1=已死 */

/* 收到云端指令时自动调用, 在 main.c 中实现此函数 */
void MQTT_RecvHandler(char *topic, char *payload);
#endif