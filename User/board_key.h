// board_key.h
#ifndef __BOARD_KEY_H
#define __BOARD_KEY_H
#include "stm32f4xx.h"
void    KEY_GPIO_Init(void);
uint8_t KEY_Scan(void);   // 0:无  1:KEY0(PA0)  2:KEY1(PE2)  3:KEY2(PE3)  4:KEY3(PE4)
#endif