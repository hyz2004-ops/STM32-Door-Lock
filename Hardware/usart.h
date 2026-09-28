// usart.h
#ifndef __USART_H
#define __USART_H
#include "stm32f4xx.h"
#include <stdio.h>

#define U1_BUF_SIZE     64
#define U2_BUF_SIZE     64
#define ESP_RX_BUF_LEN  256

extern volatile uint8_t u1_buf[], u1_idx, u1_flag;   // 串口1: 调试
extern volatile uint8_t u2_buf[], u2_idx, u2_flag;   // 串口2: 蓝牙
extern char esp_rx_buf[];                            // 串口3: ESP8266
extern u16  esp_rx_idx;

void USART1_Config(uint32_t baud);
void USART2_Config(uint32_t baud);
void USART3_Config(uint32_t baud);
void USART_SendStr(USART_TypeDef* u, const char *s);
#endif