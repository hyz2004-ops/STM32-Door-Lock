#ifndef __RC522_H
#define __RC522_H
#include "stm32f4xx.h"

void RC522_Init(void);
u8   RC522_CheckCard(u8 *uid);      /* 读到卡返回0, uid存4字节卡号; 无卡返回1 */
u8   RC522_CheckCardDbg(u8 *uid);   /* 带调试输出版, 加卡排障用 */   
                                    /* 读到卡返回0, uid存4字节卡号; 无卡返回1 */
#endif