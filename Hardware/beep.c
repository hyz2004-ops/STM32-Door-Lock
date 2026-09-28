// beep.c
#include "beep.h"
#include "delay.h"

void Beep_Init(void)   // PF8 - TIM13_CH1
{
    GPIO_InitTypeDef GPIO_InitStruct;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct;
    TIM_OCInitTypeDef TIM_OCInitStruct;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM13, ENABLE);

    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_Pin   = GPIO_Pin_8;
    GPIO_InitStruct.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOF, &GPIO_InitStruct);
    GPIO_PinAFConfig(GPIOF, GPIO_PinSource8, GPIO_AF_TIM13);

    TIM_TimeBaseInitStruct.TIM_Period        = 1000 - 1;
    TIM_TimeBaseInitStruct.TIM_Prescaler     = 168 - 1;
    TIM_TimeBaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStruct.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM13, &TIM_TimeBaseInitStruct);

    TIM_OCInitStruct.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCInitStruct.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OCInitStruct.TIM_Pulse       = 500;
    TIM_OCInitStruct.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OC1Init(TIM13, &TIM_OCInitStruct);

    TIM_OC1PreloadConfig(TIM13, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM13, ENABLE);
    TIM_Cmd(TIM13, ENABLE);

    TIM_SetCompare1(TIM13, 0);        // 默认静音
}

void Buzzer_SetFreq(uint16_t freq)
{
    if(freq == 0) {
        TIM_SetCompare1(TIM13, 0);
    } else {
        uint16_t arr = 500000 / freq - 1;
        TIM_SetAutoreload(TIM13, arr);
        TIM_SetCompare1(TIM13, arr / 2);
        TIM_GenerateEvent(TIM13, TIM_EventSource_Update);
    }
}

void Beep_Buzz(uint16_t freq, uint16_t ms)
{
    Buzzer_SetFreq(freq);
    delay_ms(ms);
    Buzzer_SetFreq(0);
}