// lock.h
#ifndef __LOCK_H
#define __LOCK_H
#include "stm32f4xx.h"

/* 触发方式: 1=高电平触发(H)  0=低电平触发(L) */
#define LOCK_TRIG_HIGH   1

#if LOCK_TRIG_HIGH
    #define RELAY_ON()   GPIO_SetBits(GPIOF, GPIO_Pin_11)
    #define RELAY_OFF()  GPIO_ResetBits(GPIOF, GPIO_Pin_11)
#else
    #define RELAY_ON()   GPIO_ResetBits(GPIOF, GPIO_Pin_11)
    #define RELAY_OFF()  GPIO_SetBits(GPIOF, GPIO_Pin_11)
#endif

void Lock_Init(void);
void Lock_Open(void);          /* 开锁3秒后自动上锁 */
void Lock_Close(void);
#endif