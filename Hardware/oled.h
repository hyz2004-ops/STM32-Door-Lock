#ifndef __OLED_H
#define __OLED_H
#include "stm32f4xx.h"

void OLED_Init(void);
void OLED_Clear(void);
void OLED_On(void);                     /* 开显示 */
void OLED_Off(void);                    /* 关显示(熄屏省电) */
void OLED_ShowChar(u8 x, u8 page, char ch);          /* 8x16 ASCII */
void OLED_ShowString(u8 x, u8 page, char *str);      /* 支持中英文混显(中文按GBK) */
void OLED_ShowNum(u8 x, u8 page, u32 num, u8 len);   /* 无符号数字 */

#endif
