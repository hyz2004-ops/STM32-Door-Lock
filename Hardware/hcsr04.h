// hcsr04.h
#ifndef __HCSR04_H
#define __HCSR04_H
#include "stm32f4xx.h"
void  HC_SR04_Init(void);
float Hcsr04_GetDistance(void);   // 返回距离 cm，超时返回 -1
#endif