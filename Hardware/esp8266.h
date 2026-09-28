#ifndef __ESP8266_H
#define __ESP8266_H
#include "stm32f4xx.h"

/*================= 用户配置区(改成你自己的) =================*/
#define WIFI_SSID    "doorlock"
#define WIFI_PWD     "hyz485938770"
//#define BEMFA_HOST   "bemfa.com" /*巴法云看*/
//#define BEMFA_PORT   9501
#define BEMFA_HOST   "192.168.137.1"  /*本地部署网页看*/
#define BEMFA_PORT   1883
#define BEMFA_KEY    "95dd5769b70a4726a0b93cb5cd7d9aa2"     /* 即 MQTT ClientID */
/*===========================================================*/

u8   ESP8266_Init(void);                    /* 返回0成功, 其他为失败步骤号 */
void ESP8266_SendRaw(u8 *data, u16 len);    /* 透传模式下发原始数据 */
void ESP8266_ClearBuf(void);
u8   ESP8266_WaitStr(char *ack, u16 timeout_ms);
#endif