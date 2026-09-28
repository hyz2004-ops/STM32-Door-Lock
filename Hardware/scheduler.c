#include "scheduler.h"

/*================= 任务声明(在 main.c 中实现) =================*/
extern void Keypad_Task(void);        /* 键盘密码       */
extern void UartCmd_Task(void);       /* 串口1命令      */
extern void Fing_Task(void);          /* 指纹扫描       */
extern void Card_Task(void);          /* RC522刷卡      */
extern void Cloud_Task(void);         /* MQTT收包处理   */
extern void Cloud_KeepAlive(void);    /* 心跳/掉线/重连 */
extern void Lockout_Task(void);       /* 锁定倒计时     */
extern void UI_Task(void);            /* 界面定时恢复   */

/*================= 任务表: 加功能=加一行 =================*/
static Task_t s_tasks[] = {
    { Cloud_Task,       10,  0 },   /* MQTT收包最优先, 10ms  */
    { UartCmd_Task,     10,  0 },   /* 串口命令,      10ms  */
    { Keypad_Task,      20,  0 },   /* 键盘扫描,      20ms  */
    { Card_Task,        50,  0 },   /* 刷卡检测,      50ms  */
    { Fing_Task,       100,  0 },   /* 指纹检测,     100ms  */
    { Lockout_Task,    100,  0 },   /* 锁定管理,     100ms  */
    { UI_Task,          50,  0 },   /* 界面恢复,      50ms  */
    { Cloud_KeepAlive, 500,  0 },   /* 心跳/重连,    500ms  */
};

#define TASK_NUM  (sizeof(s_tasks) / sizeof(s_tasks[0]))

void Scheduler_Init(void)
{
    uint8_t i;
    for (i = 0; i < TASK_NUM; i++)
        s_tasks[i].lastRun = g_ms;
}

/* 协作式调度: 到期的任务依次执行一次 */
void Scheduler_Run(void)
{
    uint8_t  i;
    uint32_t now = g_ms;

    for (i = 0; i < TASK_NUM; i++)
    {
        if ((uint32_t)(now - s_tasks[i].lastRun) >= s_tasks[i].period)
        {
            s_tasks[i].lastRun = now;
            s_tasks[i].func();
        }
    }
}