#ifndef __RTC_H
#define __RTC_H
#include "stm32f4xx.h"

/* ===== 在这里设置时间: 仅首次上电(备份域无标记)时生效, 之后掉电不重置 ===== */
#define RTC_SET_YEAR    26        /* 2026年 */
#define RTC_SET_MONTH   9
#define RTC_SET_DAY     21
#define RTC_SET_WEEKDAY 1
#define RTC_SET_HOUR    15
#define RTC_SET_MIN     38
#define RTC_SET_SEC     0

void     RTC_Config(void);      /* 上电调用一次 */
void     RTC_TimeShow(void);    /* 串口打印日期时间 */
void     RTC_ReadHMS(uint8_t *h, uint8_t *m, uint8_t *s);   /* 读时分秒 */
uint32_t RTC_Packed(void);      /* 时<<16 | 分<<8 | 秒 */

#endif
