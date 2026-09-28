// delay.c
#include "delay.h"

void delay_ms(int nms)
{
    while(nms--) {
        SysTick->CTRL = 0;
        SysTick->LOAD = 168000 - 1;   /* 1ms @ 168MHz */
        SysTick->VAL  = 0;
        SysTick->CTRL = 5;
        while((SysTick->CTRL & 0x00010000) == 0);
        SysTick->CTRL = 0;
    }
}

void delay_us(uint32_t nus)           /* 供 OLED 软件I2C、超声波使用 */
{
    SysTick->CTRL = 0;
    SysTick->LOAD = 168 * nus - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = 5;
    while((SysTick->CTRL & 0x00010000) == 0);
    SysTick->CTRL = 0;
}