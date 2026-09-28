// lock.c
#include "lock.h"
#include "delay.h"

void Lock_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);

    GPIO_InitStruct.GPIO_Pin   = GPIO_Pin_11;
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOF, &GPIO_InitStruct);

    RELAY_OFF();               /* 上电默认: 锁关闭(关键!) */
}

void Lock_Open(void)
{
    RELAY_ON();                /* 继电器吸合 → 锁通电弹开 */
    delay_ms(3000);            /* 保持3秒 */
    RELAY_OFF();               /* 断电 → 弹簧复位上锁 */
}

void Lock_Close(void)
{
    RELAY_OFF();
}