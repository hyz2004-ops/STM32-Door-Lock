#ifndef __SCHEDULER_H
#define __SCHEDULER_H

#include "stm32f4xx.h"

/* 毫秒时基: 由 main.c 主循环累加维护 */
extern volatile uint32_t g_ms;

typedef void (*TaskFunc_t)(void);

typedef struct {
    TaskFunc_t func;      /* 任务函数 */
    uint32_t   period;    /* 执行周期(ms) */
    uint32_t   lastRun;   /* 上次执行时刻 */
} Task_t;

void Scheduler_Init(void);
void Scheduler_Run(void);   /* 放在 while(1) 里反复调用 */

#endif