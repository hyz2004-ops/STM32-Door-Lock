// beep.h
#ifndef __BEEP_H
#define __BEEP_H
#include "stm32f4xx.h"
void Beep_Init(void);
void Buzzer_SetFreq(uint16_t freq);   // freq=0 静音
void Beep_Buzz(uint16_t freq, uint16_t ms);  // 响 ms 毫秒
#endif