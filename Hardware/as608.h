#ifndef __AS608_H
#define __AS608_H
#include "stm32f4xx.h"

/* 确认码(模块返回) */
#define AS608_OK          0x00   /* 成功 */
#define AS608_NOFINGER    0x02   /* 没检测到手指 */
#define AS608_NOTFOUND    0x09   /* 没搜索到 */

u8  AS608_Init(void);                     /* 返回0=模块在线 */
int AS608_CheckFinger(void);              /* 自动识别: 返回指纹ID, 无手指-1, 未匹配-2 */
u8  AS608_Enroll(u16 id);                 /* 录入指纹到指定ID, 返回确认码 */
u8  AS608_Delete(u16 id);                 /* 删除指定ID */
u8  AS608_Empty(void);                    /* 清空指纹库 */
u16 AS608_GetNum(void);                   /* 读已存模板数, 0xFFFF=失败 */
u8  AS608_ReadIndex(u8 *bits);   /* 读索引表: bits[32], bit=1表示该ID已登记, 返回0成功 */
#endif
