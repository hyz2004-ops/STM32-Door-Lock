#include "oled.h"
#include "oledfont.h"
#include "delay.h"

/*---------------- 软件I2C引脚定义: SCL=PB8  SDA=PB9 ----------------*/
#define OLED_SCL_H   GPIO_SetBits(GPIOB, GPIO_Pin_8)
#define OLED_SCL_L   GPIO_ResetBits(GPIOB, GPIO_Pin_8)
#define OLED_SDA_H   GPIO_SetBits(GPIOB, GPIO_Pin_9)
#define OLED_SDA_L   GPIO_ResetBits(GPIOB, GPIO_Pin_9)

#define OLED_ADDR    0x78        /* SSD1306 I2C地址 */

/*---------------- GPIO初始化 ----------------*/
static void OLED_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    GPIO_InitStruct.GPIO_Pin   = GPIO_Pin_8 | GPIO_Pin_9;
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_OD;   /* 开漏输出 */
    GPIO_InitStruct.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    OLED_SCL_H; OLED_SDA_H;
}

/*---------------- 软件I2C底层 ----------------*/
static void I2C_Start(void)
{
    OLED_SDA_H; OLED_SCL_H; delay_us(2);
    OLED_SDA_L; delay_us(2);
    OLED_SCL_L;
}

static void I2C_Stop(void)
{
    OLED_SDA_L; OLED_SCL_H; delay_us(2);
    OLED_SDA_H; delay_us(2);
}

static void I2C_WriteByte(u8 dat)
{
    u8 i;
    for(i = 0; i < 8; i++) {
        if(dat & 0x80) OLED_SDA_H;
        else           OLED_SDA_L;
        delay_us(2);
        OLED_SCL_H; delay_us(2);
        OLED_SCL_L; delay_us(2);
        dat <<= 1;
    }
    OLED_SDA_H; delay_us(2);   /* 释放SDA等待ACK(不判断，提高兼容性) */
    OLED_SCL_H; delay_us(2);
    OLED_SCL_L;
}

/*---------------- 写命令/数据 ----------------*/
static void OLED_WR_Cmd(u8 cmd)
{
    I2C_Start();
    I2C_WriteByte(OLED_ADDR);
    I2C_WriteByte(0x00);       /* 控制字节: 命令 */
    I2C_WriteByte(cmd);
    I2C_Stop();
}

static void OLED_WR_Data(u8 dat)
{
    I2C_Start();
    I2C_WriteByte(OLED_ADDR);
    I2C_WriteByte(0x40);       /* 控制字节: 数据 */
    I2C_WriteByte(dat);
    I2C_Stop();
}

/*---------------- 设置光标: x(0~127)列, page(0~7)页 ----------------*/
static void OLED_SetPos(u8 x, u8 page)
{
    OLED_WR_Cmd(0xB0 + page);
    OLED_WR_Cmd(((x & 0xF0) >> 4) | 0x10);
    OLED_WR_Cmd(x & 0x0F);
}

/*---------------- SSD1306初始化 ----------------*/
void OLED_Init(void)
{
    OLED_GPIO_Init();
    delay_ms(100);

    OLED_WR_Cmd(0xAE);                      /* 关显示 */
    OLED_WR_Cmd(0xD5); OLED_WR_Cmd(0x80);   /* 时钟分频 */
    OLED_WR_Cmd(0xA8); OLED_WR_Cmd(0x3F);   /* 64路 */
    OLED_WR_Cmd(0xD3); OLED_WR_Cmd(0x00);   /* 显示偏移 */
    OLED_WR_Cmd(0x40);                      /* 起始行0 */
    OLED_WR_Cmd(0x8D); OLED_WR_Cmd(0x14);   /* 电荷泵使能 */
    OLED_WR_Cmd(0x20); OLED_WR_Cmd(0x02);   /* 页寻址模式 */
    OLED_WR_Cmd(0xA1);                      /* 段重映射(左右翻转) */
    OLED_WR_Cmd(0xC8);                      /* COM扫描(上下翻转) */
    OLED_WR_Cmd(0xDA); OLED_WR_Cmd(0x12);   /* COM硬件配置 */
    OLED_WR_Cmd(0x81); OLED_WR_Cmd(0xCF);   /* 对比度 */
    OLED_WR_Cmd(0xD9); OLED_WR_Cmd(0xF1);   /* 预充电 */
    OLED_WR_Cmd(0xDB); OLED_WR_Cmd(0x40);   /* VCOMH */
    OLED_WR_Cmd(0xA4);                      /* 显示跟随RAM */
    OLED_WR_Cmd(0xA6);                      /* 正常显示(非反色) */
    OLED_Clear();
    OLED_WR_Cmd(0xAF);                      /* 开显示 */
}

void OLED_On(void)  { OLED_WR_Cmd(0x8D); OLED_WR_Cmd(0x14); OLED_WR_Cmd(0xAF); }
void OLED_Off(void) { OLED_WR_Cmd(0x8D); OLED_WR_Cmd(0x10); OLED_WR_Cmd(0xAE); }

void OLED_Clear(void)
{
    u8 page, x;
    for(page = 0; page < 8; page++) {
        OLED_SetPos(0, page);
        for(x = 0; x < 128; x++) OLED_WR_Data(0x00);
    }
}

/*---------------- 8x16 ASCII: x(0~120), page只能取0/2/4/6 ----------------*/
void OLED_ShowChar(u8 x, u8 page, char ch)
{
    u8 i;
    const u8 *font = &F8X16[(ch - ' ') * 16];

    OLED_SetPos(x, page);
    for(i = 0; i < 8; i++) OLED_WR_Data(font[i]);       /* 上半 */
    OLED_SetPos(x, page + 1);
    for(i = 0; i < 8; i++) OLED_WR_Data(font[8 + i]);   /* 下半 */
}

/*---------------- 16x16 中文(按GBK码查表) ----------------*/
static int OLED_FindHZ(u8 high, u8 low)
{
    u8 i;
    for(i = 0; i < HZ_NUM; i++)
        if(F16x16_Idx[i][0] == high && F16x16_Idx[i][1] == low)
            return i;
    return -1;
}

static void OLED_ShowHZ(u8 x, u8 page, u8 high, u8 low)
{
    int idx = OLED_FindHZ(high, low);
    u8 i;
    if(idx < 0) {                        /* 字库里没有，显示空白 */
        OLED_SetPos(x, page);
        for(i = 0; i < 16; i++) OLED_WR_Data(0);
        OLED_SetPos(x, page + 1);
        for(i = 0; i < 16; i++) OLED_WR_Data(0);
        return;
    }
    OLED_SetPos(x, page);
    for(i = 0; i < 16; i++) OLED_WR_Data(F16x16[idx][i]);       /* 上半 */
    OLED_SetPos(x, page + 1);
    for(i = 0; i < 16; i++) OLED_WR_Data(F16x16[idx][16 + i]);  /* 下半 */
}

/*---------------- 字符串: 中英文混显，自动识别GBK双字节 ----------------*/
void OLED_ShowString(u8 x, u8 page, char *str)
{
    while(*str) {
        if((u8)*str > 0x80) {            /* GBK双字节 → 中文 */
            OLED_ShowHZ(x, page, (u8)str[0], (u8)str[1]);
            x += 16; str += 2;
        } else {                         /* ASCII */
            OLED_ShowChar(x, page, *str);
            x += 8;  str += 1;
        }
        if(x > 120) break;               /* 超出屏幕宽度 */
    }
}

/*---------------- 无符号数字显示 ----------------*/
void OLED_ShowNum(u8 x, u8 page, u32 num, u8 len)
{
    u8 i, d, started = 0;
    for(i = 0; i < len; i++) {
        u32 div = 1;
        u8 j;
        for(j = 0; j < len - 1 - i; j++) div *= 10;
        d = (num / div) % 10;
        if(d || started || i == len - 1) {
            OLED_ShowChar(x, page, '0' + d);
            started = 1;
        } else {
            OLED_ShowChar(x, page, ' ');
        }
        x += 8;
    }
}
