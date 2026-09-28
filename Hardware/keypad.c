#include "keypad.h"
#include "delay.h"

/* 引脚定义保持不变 */
#define ROW1_GPIO GPIOB
#define ROW1_PIN  GPIO_Pin_7
#define ROW2_GPIO GPIOA
#define ROW2_PIN  GPIO_Pin_4
#define ROW3_GPIO GPIOG
#define ROW3_PIN  GPIO_Pin_15
#define ROW4_GPIO GPIOC
#define ROW4_PIN  GPIO_Pin_7

#define COL1_GPIO GPIOC
#define COL1_PIN  GPIO_Pin_9
#define COL2_GPIO GPIOB
#define COL2_PIN  GPIO_Pin_6
#define COL3_GPIO GPIOE
#define COL3_PIN  GPIO_Pin_6
#define COL4_GPIO GPIOA
#define COL4_PIN  GPIO_Pin_8

static GPIO_TypeDef* ROW_GPIO[4] = {ROW1_GPIO, ROW2_GPIO, ROW3_GPIO, ROW4_GPIO};
static uint16_t      ROW_PIN[4]  = {ROW1_PIN,  ROW2_PIN,  ROW3_PIN,  ROW4_PIN};
static GPIO_TypeDef* COL_GPIO[4] = {COL1_GPIO, COL2_GPIO, COL3_GPIO, COL4_GPIO};
static uint16_t      COL_PIN[4]  = {COL1_PIN,  COL2_PIN,  COL3_PIN,  COL4_PIN};

static const char KeyMap[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

void Keypad_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    uint8_t i;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB |
                           RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOE |
                           RCC_AHB1Periph_GPIOG, ENABLE);

    /* 行：推挽输出，初始高电平 */
    GPIO_InitStruct.GPIO_Pin   = 0;
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStruct.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    for(i = 0; i < 4; i++) {
        GPIO_InitStruct.GPIO_Pin = ROW_PIN[i];
        GPIO_Init(ROW_GPIO[i], &GPIO_InitStruct);
        GPIO_SetBits(ROW_GPIO[i], ROW_PIN[i]);
    }

    /* 列：上拉输入 */
    GPIO_InitStruct.GPIO_Pin   = 0;
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_IN;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;   // 输入模式下无效，但显式赋值
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStruct.GPIO_PuPd  = GPIO_PuPd_UP;
    for(i = 0; i < 4; i++) {
        GPIO_InitStruct.GPIO_Pin = COL_PIN[i];
        GPIO_Init(COL_GPIO[i], &GPIO_InitStruct);
    }
}

uint8_t Keypad_Scan(void)
{
    uint8_t row, col, i;

    for(row = 0; row < 4; row++) {
        /* 所有行置高 */
        for(i = 0; i < 4; i++) {
            GPIO_SetBits(ROW_GPIO[i], ROW_PIN[i]);
        }
        /* 当前行拉低 */
        GPIO_ResetBits(ROW_GPIO[row], ROW_PIN[row]);
        
        /* 建立时间：如果用 delay_us，建议 20~50us；如果只有 delay_ms，保留 1ms */
        delay_us(50);   // 需要你的 delay 模块支持；不支持则改回 delay_ms(1)

        for(col = 0; col < 4; col++) {
            if(GPIO_ReadInputDataBit(COL_GPIO[col], COL_PIN[col]) == Bit_RESET)
                return row * 4 + col;
        }
    }
    return 0xFF;
}

char Keypad_GetKey(void)
{
    uint8_t k = Keypad_Scan();
    if(k == 0xFF) return 0;

    delay_ms(20);                       // 消抖：建议 20ms
    if(Keypad_Scan() != k) return 0;

    /* 等待松手，加超时防止死等（约 500ms） */
    uint16_t timeout = 0;
    while(Keypad_Scan() != 0xFF) {
        delay_ms(1);
        if(++timeout > 500) break;      // 超时强制退出
    }

    return KeyMap[k / 4][k % 4];
}