#include "as608.h"
#include "usart.h"
#include "delay.h"
#include <string.h>
#include <stdio.h>

#define AS608_BAUD   57600          /* 模块默认波特率(不行就试9600) */

/*---------------- 接收缓冲(中断填充) ----------------*/
static volatile u8  as608_buf[64];
static volatile u16 as608_idx = 0;

/*---------------- USART2初始化: PA2=TX, PA3=RX ----------------*/
static void AS608_UART_Init(uint32_t baud)
{
    GPIO_InitTypeDef  GPIO_InitStruct;
    USART_InitTypeDef USART_InitStruct;
    NVIC_InitTypeDef  NVIC_InitStruct;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    GPIO_InitStruct.GPIO_Pin   = GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOA, &GPIO_InitStruct);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);

    USART_InitStruct.USART_BaudRate            = baud;
    USART_InitStruct.USART_WordLength          = USART_WordLength_8b;
    USART_InitStruct.USART_StopBits            = USART_StopBits_1;
    USART_InitStruct.USART_Parity              = USART_Parity_No;
    USART_InitStruct.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART2, &USART_InitStruct);

    NVIC_InitStruct.NVIC_IRQChannel                   = USART2_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStruct.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStruct);

    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART2, ENABLE);
}

void USART2_IRQHandler(void)
{
    if(USART_GetITStatus(USART2, USART_IT_RXNE) == SET) {
        u8 c = USART_ReceiveData(USART2);
        if(as608_idx < sizeof(as608_buf) - 1)
            as608_buf[as608_idx++] = c;
    }
}

/*---------------- 协议帧收发 ----------------*/
/* 发帧: EF01 + 地址(FFFFFFFF) + PID + 长度 + 数据 + 校验和 */
static void AS608_SendPack(u8 pid, const u8 *data, u16 len)
{
    u8  head[9];
    u16 sum = 0, i;

    head[0] = 0xEF; head[1] = 0x01;
    head[2] = 0xFF; head[3] = 0xFF; head[4] = 0xFF; head[5] = 0xFF;
    head[6] = pid;
    head[7] = (len + 2) >> 8;               /* 长度 = 校验和2字节 + 数据 */
    head[8] = (len + 2) & 0xFF;

    for(i = 0; i < 9; i++) {
        while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
        USART_SendData(USART2, head[i]);
    }
    sum = pid + head[7] + head[8];
    for(i = 0; i < len; i++) {
        while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
        USART_SendData(USART2, data[i]);
        sum += data[i];
    }
    while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    USART_SendData(USART2, sum >> 8);
    while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    USART_SendData(USART2, sum & 0xFF);
}

/* 等一帧应答, 校验通过后把数据段拷到 data, 返回数据长度; 超时返回 -1 */
static int AS608_WaitAck(u8 *data, u16 timeout_ms)
{
    while(timeout_ms--) {
        if(as608_idx >= 9) {
            u16 len = ((u16)as608_buf[7] << 8) | as608_buf[8];
            if(as608_idx >= 9 + len) {               /* 收完一整帧 */
                u16 sum = 0, i, chk;
                if(as608_buf[0] != 0xEF || as608_buf[1] != 0x01) goto fail;
                for(i = 6; i < 9 + len - 2; i++) sum += as608_buf[i];
                chk = ((u16)as608_buf[9 + len - 2] << 8) | as608_buf[9 + len - 1];
                if(sum != chk) goto fail;
                memcpy(data, (void*)&as608_buf[9], len - 2);
                as608_idx = 0;
                return len - 2;
            }
        }
        delay_ms(1);
    }
fail:
    as608_idx = 0;
    return -1;
}

/* 发一条命令并取确认码; 返回0xFF=超时/帧错误, 其他=模块确认码 */
static u8 AS608_Cmd(u8 cmd, u8 *param, u16 plen, u8 *ack_data)
{
    u8  buf[8];
    u8  data[32];
    int n;

    buf[0] = cmd;
    if(param && plen) memcpy(&buf[1], param, plen);

    as608_idx = 0;
    AS608_SendPack(0x01, buf, plen + 1);          /* PID=01 命令包 */
    n = AS608_WaitAck(data, 1200);
    if(n < 1) return 0xFF;
    if(ack_data && n > 1) memcpy(ack_data, &data[1], n - 1);
    return data[0];                               /* data[0] = 确认码 */
}

/*---------------- 基本指令 ----------------*/
static u8 AS608_GenImg(void)  { return AS608_Cmd(0x01, NULL, 0, NULL); }   /* 采图 */
static u8 AS608_Img2Tz(u8 b)  { u8 p[1]={b}; return AS608_Cmd(0x02, p, 1, NULL); } /* 转特征 */
static u8 AS608_RegModel(void){ return AS608_Cmd(0x05, NULL, 0, NULL); }   /* 合成模板 */

static u8 AS608_Store(u8 bufId, u16 id)
{
    u8 p[3] = {bufId, (u8)(id >> 8), (u8)(id & 0xFF)};
    return AS608_Cmd(0x06, p, 3, NULL);
}

/*---------------- 对外接口 ----------------*/

/* 初始化并验证模块在线 */
u8 AS608_Init(void)
{
    u8 pwd[4] = {0x00, 0x00, 0x00, 0x00};       /* 默认口令 */
    u8 ret;

    AS608_UART_Init(AS608_BAUD);
    delay_ms(200);

    ret = AS608_Cmd(0x13, pwd, 4, NULL);        /* 验证口令 */
    if(ret == AS608_OK) { printf("AS608 online\r\n"); return 0; }
    printf("AS608 not found! ret=%02X\r\n", ret);
    return 1;
}

/* 待机自动检测: 有手指→搜索指纹库 */
int AS608_CheckFinger(void)
{
    u8 ack[8];
    u8 p[5] = {1, 0x00, 0x00, 0x00, 0x63};      /* 缓冲区1, 搜索0~99号 */

    if(AS608_GenImg() != AS608_OK) return -1;   /* 没手指 */
    if(AS608_Img2Tz(1) != AS608_OK) return -1;  /* 图像太差 */
    if(AS608_Cmd(0x04, p, 5, ack) != AS608_OK) return -2;   /* 未匹配 */

    return ((int)ack[0] << 8) | ack[1];         /* 匹配到的指纹ID */
}

/* 录入指纹: 采两次图像→合成模板→存入id */
u8 AS608_Enroll(u16 id)
{
    u8  ret;
    u16 t;

    /* 第一次采图 */
    for(t = 0; t < 100; t++) {
        if(AS608_GenImg() == AS608_OK) break;
        delay_ms(50);
    }
    if(t >= 100) return AS608_NOFINGER;
    if(AS608_Img2Tz(1) != AS608_OK) return 0x03;

    delay_ms(1500);                             /* 抬手时间 */

    /* 第二次采图 */
    for(t = 0; t < 100; t++) {
        if(AS608_GenImg() == AS608_OK) break;
        delay_ms(50);
    }
    if(t >= 100) return AS608_NOFINGER;
    if(AS608_Img2Tz(2) != AS608_OK) return 0x03;

    ret = AS608_RegModel();
    if(ret != AS608_OK) return ret;

    return AS608_Store(1, id);
}

u8 AS608_Delete(u16 id)
{
    u8 p[4] = {(u8)(id >> 8), (u8)(id & 0xFF), 0x00, 0x01};  /* 删1枚 */
    return AS608_Cmd(0x0C, p, 4, NULL);
}

u8 AS608_Empty(void)
{
    return AS608_Cmd(0x0D, NULL, 0, NULL);
}

u16 AS608_GetNum(void)
{
    u8 ack[4];
    if(AS608_Cmd(0x1D, NULL, 0, ack) != AS608_OK) return 0xFFFF;
    return ((u16)ack[0] << 8) | ack[1];
}

/* 读索引表(指令0x1F, 页0覆盖ID 0~255): bits[32]每一位代表一个ID是否已登记 */
u8 AS608_ReadIndex(u8 *bits)
{
    u8  cmd[2] = {0x1F, 0x00};      /* 指令 + 页码0 */
    u8  data[40];
    int n;

    as608_idx = 0;
    AS608_SendPack(0x01, cmd, 2);
    n = AS608_WaitAck(data, 1200);
    if (n < 33) return 0xFF;        /* 应答 = 确认码1字节 + 索引表32字节 */
    if (data[0] != AS608_OK) return data[0];
    memcpy(bits, &data[1], 32);
    return AS608_OK;
}
