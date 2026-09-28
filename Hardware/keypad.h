// keypad.h
#ifndef __KEYPAD_H
#define __KEYPAD_H
#include "stm32f4xx.h"
void    Keypad_Init(void);
uint8_t Keypad_Scan(void);     // 返回 0~15，无键返回 0xFF
char    Keypad_GetKey(void);   // 带消抖+松手检测，返回键字符，无键返回 0
#endif