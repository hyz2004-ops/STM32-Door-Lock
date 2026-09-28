#include "mqtt.h"
#include "esp8266.h"
#include "usart.h"
#include "delay.h"
#include <string.h>
#include <stdio.h>

static u16 mqtt_pkt_id = 1;
static volatile u8 mqtt_linkEvent = 0;

/* 编码MQTT剩余长度(报文<128字节用1字节, 否则2字节, 本项目足够) */
static u8 MQTT_EncodeLen(u8 *buf, u16 len)
{
    if(len < 128) { buf[0] = (u8)len; return 1; }
    buf[0] = 0x80 | (len & 0x7F);
    buf[1] = (u8)(len >> 7);
    return 2;
}

/* 追加 "两字节长度 + 字符串" 字段 */
static u16 MQTT_AppendField(u8 *buf, u16 pos, const char *str)
{
    u16 l = strlen(str);
    buf[pos++] = (l >> 8) & 0xFF;
    buf[pos++] = l & 0xFF;
    memcpy(&buf[pos], str, l);
    return pos + l;
}

/* 组包并发送: 内容从buf[3]开始构建, 固定头回填 */
static void MQTT_Send(u8 *buf, u16 pos, u8 fixedByte)
{
    u16 bodyLen = pos - 3;
    u8  rlBytes[2];
    u8  rlLen = MQTT_EncodeLen(rlBytes, bodyLen);
    u8  head  = 2 - rlLen;               /* rlLen=1→head=1, rlLen=2→head=0 */

    buf[head] = fixedByte;
    memcpy(&buf[head + 1], rlBytes, rlLen);
    ESP8266_SendRaw(&buf[head], 1 + rlLen + bodyLen);
}

/*---------------- MQTT CONNECT ----------------*/
u8 MQTT_Connect(const char *clientID)
{
    u8  buf[128];
    u16 pos = 3, t;

    pos = MQTT_AppendField(buf, pos, "MQTT");   /* 协议名 */
    buf[pos++] = 0x04;                          /* 协议级别 MQTT3.1.1 */
    buf[pos++] = 0x02;                          /* Clean Session */
    buf[pos++] = 0x00;                          /* KeepAlive 高字节 */
    buf[pos++] = 60;                            /* KeepAlive = 60秒 */
    pos = MQTT_AppendField(buf, pos, clientID); /* ClientID=私钥 */

    MQTT_Send(buf, pos, 0x10);

    /* 等待 CONNACK: 20 02 00 00 */
    t = 3000;
    while(t--) {
        if(esp_rx_idx >= 4 && (u8)esp_rx_buf[0] == 0x20 && esp_rx_buf[3] == 0) {
            ESP8266_ClearBuf();
            printf("MQTT connect OK\r\n");
            return 0;
        }
        delay_ms(1);
    }
    return 1;
}

/*---------------- MQTT SUBSCRIBE (QoS0) ----------------*/
u8 MQTT_Subscribe(const char *topic)
{
    u8  buf[64];
    u16 pos = 3;

    buf[pos++] = (mqtt_pkt_id >> 8) & 0xFF;     /* 报文标识符 */
    buf[pos++] = mqtt_pkt_id++ & 0xFF;
    pos = MQTT_AppendField(buf, pos, topic);
    buf[pos++] = 0x00;                          /* 要求QoS0 */

    MQTT_Send(buf, pos, 0x82);
    return 0;
}

/*---------------- MQTT PUBLISH (QoS0) ----------------*/
void MQTT_Publish(const char *topic, const char *payload)
{
    u8  buf[256];
    u16 pos = 3, pl = strlen(payload);

    pos = MQTT_AppendField(buf, pos, topic);
    memcpy(&buf[pos], payload, pl);
    pos += pl;

    MQTT_Send(buf, pos, 0x30);
}

/*---------------- MQTT 心跳 ----------------*/
void MQTT_Ping(void)
{
    u8 ping[2] = {0xC0, 0x00};
    ESP8266_SendRaw(ping, 2);
}

/*---------------- 下行消息解析(主循环调用) ----------------*/
void MQTT_Process(void)
{
		static char topic[32], payload[200];
	  u16 reLen, rlBytes, topicLen, pl, total;
	
	  /*AT层的连接关闭提示优先检测*/
	  if(esp_rx_idx >= 6 && strstr(esp_rx_buf, "CLOSE") != NULL)
		{
		   printf("TCP closed!\r\n");
			 mqtt_linkEvent =1;
			 ESP8266_ClearBuf();
			 return;
		}
		
		while(esp_rx_idx >=2)
		{
       /*解析MQTT剩余长度（1-2字节）*/
			 reLen = (u8)esp_rx_buf[1];
			 rlBytes = 1;
			 if(reLen & 0x80)
			 {
			    if(esp_rx_idx < 3) return;
				  reLen = (reLen & 0x7F) | ((u16)(u8)esp_rx_buf[2] << 7);
				  rlBytes = 2;
			 }
			 
			 total = 1 + rlBytes +reLen;
			 /*清空垃圾数据*/
			 if(reLen > 300 || total >ESP_RX_BUF_LEN)
			 {
				  ESP8266_ClearBuf();
				  return;
			 }
			 
			 if(esp_rx_idx < total) return;/*没收完，等下一轮*/
			 
			 if(((u8)esp_rx_buf[0] & 0xF0) == 0x30 )
			 {
			    topicLen = ((u8)esp_rx_buf[1 + rlBytes] << 8) | (u8)esp_rx_buf[2 + rlBytes];
				  pl = reLen - 2 - topicLen;
				  if(topicLen < sizeof(topic) && pl < sizeof(payload))
					{
						 memcpy(topic, &esp_rx_buf[1 + rlBytes + 2], topicLen);
						 topic[topicLen] = '\0';
						 memcpy(payload, &esp_rx_buf[1 + rlBytes + 2 + topicLen], pl);
						payload[pl] = '\0';
						ESP8266_ClearBuf();
						MQTT_RecvHandler(topic, payload);
						return;
					}
			 }
			 /*其他报文(SUBACK/PINGRESP/PUBACK等): 丢包，继续解析后面的*/
			 if(esp_rx_idx == total)
			 {
				  ESP8266_ClearBuf();
				  return;
			 }
			 memmove(esp_rx_buf, &esp_rx_buf[total], esp_rx_idx - total);
			 esp_rx_idx -= total;
			 esp_rx_buf[esp_rx_idx] = '\0';
		}
}

/*---------------- 断线检测 ----------------*/


u8 MQTT_LinkEvent(void)
{
    if (mqtt_linkEvent) { mqtt_linkEvent = 0; return 1; }
    return 0;
}

/* 活性检测: 发PINGREQ, 2秒内收到PINGRESP(0xD0)说明链路活着 */
u8 MQTT_CheckAlive(void)
{
    u16 t = 2000;
    u8  ping[2] = {0xC0, 0x00};

    ESP8266_ClearBuf();
    ESP8266_SendRaw(ping, 2);
    while (t--)
    {
        if (esp_rx_idx >= 2 && (u8)esp_rx_buf[0] == 0xD0)
        {
            ESP8266_ClearBuf();
            return 0;                       /* 活着 */
        }
        delay_ms(1);
    }
    return 1;                               /* 没人理, 链路死了 */
}

/* 弱定义: main.c 中重新实现即可覆盖 */
__weak void MQTT_RecvHandler(char *topic, char *payload)
{
    printf("MQTT recv [%s]: %s\r\n", topic, payload);
}