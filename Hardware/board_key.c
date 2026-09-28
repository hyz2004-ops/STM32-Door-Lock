// board_key.c
#include "board_key.h"
#include "delay.h"

void KEY_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOE, ENABLE);

    GPIO_InitStruct.GPIO_Pin  = GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOE, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0;
    GPIO_Init(GPIOA, &GPIO_InitStruct);
}

uint8_t KEY_Scan(void)   // 按下为低电平
{
    if(GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == 0) { delay_ms(10);
        if(GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == 0) return 1; }
    if(GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_2) == 0) { delay_ms(10);
        if(GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_2) == 0) return 2; }
    if(GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_3) == 0) { delay_ms(10);
        if(GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_3) == 0) return 3; }
    if(GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_4) == 0) { delay_ms(10);
        if(GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_4) == 0) return 4; }
    return 0;
}