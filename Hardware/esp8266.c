#include "esp8266.h"
#include "usart.h"
#include "delay.h"
#include <string.h>
#include <stdio.h>

void ESP8266_ClearBuf(void)
{
    esp_rx_idx = 0;
    esp_rx_buf[0] = '\0';
}

u8 ESP8266_WaitStr(char *ack, u16 timeout_ms)
{
    while(timeout_ms--) {
        if(strstr(esp_rx_buf, ack) != NULL) return 0;
        delay_ms(1);
    }
    return 1;
}

/* 发AT指令并等待应答 */
static u8 ESP_SendCmd(char *cmd, char *ack, u16 timeout_ms)
{
    ESP8266_ClearBuf();
    USART_SendStr(USART3, cmd);
    USART_SendStr(USART3, "\r\n");
    return ESP8266_WaitStr(ack, timeout_ms);
}

/* 透传模式下发送原始字节流 */
void ESP8266_SendRaw(u8 *data, u16 len)
{
    u16 i;
    for(i = 0; i < len; i++) {
        while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
        USART_SendData(USART3, data[i]);
    }
}

/* ESP8266初始化: 连WiFi → 连巴法云 → 进入透传 */
u8 ESP8266_Init(void)
{
    char cmd[80];

    delay_ms(2000);                                     /* 等模块上电就绪 */

    /* AT探测: 先试5次(覆盖模块上电慢), 再试退出透传, 仍失败才是真故障 */
    {
        u8 i, ok = 1;

        for(i = 0; i < 5; i++) {
            if(ESP_SendCmd("AT", "OK", 1000) == 0) { ok = 0; break; }
            delay_ms(500);
        }

        if(ok) {                                        /* 无回应: 可能卡在透传模式 */
            ESP8266_ClearBuf();
            USART_SendStr(USART3, "+++");               /* 退出透传(不带\r\n) */
            delay_ms(1500);                             /* +++前后需1秒空闲 */
            for(i = 0; i < 5; i++) {
                if(ESP_SendCmd("AT", "OK", 1000) == 0) { ok = 0; break; }
                delay_ms(500);
            }
        }

        if(ok) {
            printf("ESP8266 no response! raw=[%s]\r\n", esp_rx_buf);
            return 1;
        }
    }

    ESP_SendCmd("AT+CIPSHUT", "OK", 3000);              /* 清掉残留TCP连接(结果忽略) */
    ESP_SendCmd("ATE0", "OK", 1000);                    /* 关回显 */
    if(ESP_SendCmd("AT+CWMODE=1", "OK", 1000)) return 2;/* STA模式 */

    sprintf(cmd, "AT+CWJAP=\"%s\",\"%s\"", WIFI_SSID, WIFI_PWD);
    if(ESP_SendCmd(cmd, "OK", 15000))          return 3;/* 连WiFi */

    if(ESP_SendCmd("AT+CIPMUX=0", "OK", 1000)) return 4;/* 单连接 */

    /* 连巴法云(失败自动重试3次) */
    {
        u8 retry;
        for(retry = 0; retry < 3; retry++) {
            sprintf(cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d", BEMFA_HOST, BEMFA_PORT);
            if(ESP_SendCmd(cmd, "CONNECT", 8000) == 0) break;
            delay_ms(2000);
            if(retry == 2) return 5;
        }
    }

    if(ESP_SendCmd("AT+CIPMODE=1", "OK", 2000)) return 6;

    /* 进入透传发送(失败重试3次) */
    {
        u8 retry;
        for(retry = 0; retry < 3; retry++) {
            if(ESP_SendCmd("AT+CIPSEND", ">", 3000) == 0) break;
            delay_ms(1000);
            if(retry == 2) return 7;
        }
    }

    ESP8266_ClearBuf();        /* 清空AT应答, 之后缓冲区只剩MQTT数据 */
    printf("ESP8266 ready\r\n");
    return 0;
}