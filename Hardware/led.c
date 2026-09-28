// led.c
#include "led.h"

void LED_GPIO_Init(void)
{
    GPIO_InitTypeDef myGPIO;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE | RCC_AHB1Periph_GPIOF, ENABLE);

    myGPIO.GPIO_Pin   = GPIO_Pin_9 | GPIO_Pin_10;
    myGPIO.GPIO_Mode  = GPIO_Mode_OUT;
    myGPIO.GPIO_OType = GPIO_OType_PP;
    myGPIO.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    myGPIO.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOF, &myGPIO);

    myGPIO.GPIO_Pin   = GPIO_Pin_13 | GPIO_Pin_14;
    GPIO_Init(GPIOE, &myGPIO);

    LED0_OFF(); LED1_OFF(); LED2_OFF(); LED3_OFF();
}