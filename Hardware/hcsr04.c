// hcsr04.c
#include "hcsr04.h"
#include "delay.h"

#define TRIG_PIN  GPIO_Pin_8    // PC8
#define ECHO_PIN  GPIO_Pin_6    // PC6

void HC_SR04_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

    GPIO_InitStruct.GPIO_Pin   = TRIG_PIN;
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin  = ECHO_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_ResetBits(GPIOC, TRIG_PIN);
    delay_ms(10);
}

float Hcsr04_GetDistance(void)
{
    uint32_t t = 0, timeout = 60000;

    GPIO_SetBits(GPIOC, TRIG_PIN);
    delay_us(20);
    GPIO_ResetBits(GPIOC, TRIG_PIN);

    while(GPIO_ReadInputDataBit(GPIOC, ECHO_PIN) == 0)   // 等回响拉高
        if(--timeout == 0) return -1;
    while(GPIO_ReadInputDataBit(GPIOC, ECHO_PIN) == 1) { // 计时高电平
        delay_us(1);
        if(++t > 30000) return -1;
    }
    return t / 58.0f;    // 换算成 cm
}