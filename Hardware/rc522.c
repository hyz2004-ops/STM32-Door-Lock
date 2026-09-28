#include "rc522.h"
#include "delay.h"
#include <stdio.h>

/*---------------- 引脚定义(软件SPI) ----------------*/
/* 新接法: SDA=PD6  SCK=PD7  MOSI=PC6  MISO=PC8  RST=PC11 */
#define RC522_CS_H    GPIO_SetBits(GPIOD, GPIO_Pin_6)
#define RC522_CS_L    GPIO_ResetBits(GPIOD, GPIO_Pin_6)
#define RC522_SCK_H   GPIO_SetBits(GPIOD, GPIO_Pin_7)
#define RC522_SCK_L   GPIO_ResetBits(GPIOD, GPIO_Pin_7)
#define RC522_MOSI_H   GPIO_SetBits(GPIOC, GPIO_Pin_6)
#define RC522_MOSI_L   GPIO_ResetBits(GPIOC, GPIO_Pin_6)
#define RC522_MISO     GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_8)
#define RC522_RST_H   GPIO_SetBits(GPIOC, GPIO_Pin_11)
#define RC522_RST_L   GPIO_ResetBits(GPIOC, GPIO_Pin_11)

/*---------------- MFRC522 寄存器/命令 ----------------*/
#define CMD_REG        0x01
#define IRQ_EN_REG     0x02     /* ComIEnReg */
#define IRQ_REG        0x04     /* ComIrqReg */
#define FIFO_DATA_REG  0x09
#define FIFO_LVL_REG   0x0A
#define CTRL_REG       0x0C
#define BIT_FRAME_REG  0x0D
#define MODE_REG       0x11
#define TX_CTRL_REG    0x14
#define TXASK_REG      0x15
#define STATUS2_REG    0x08
#define ERROR_REG      0x06
#define TMODE_REG      0x2A
#define TPRESCALER_REG 0x2B
#define TRELOAD_H      0x2C
#define TRELOAD_L      0x2D

#define PCD_IDLE       0x00
#define PCD_TRANSCEIVE 0x0C
#define PCD_RESET      0x0F
#define PICC_REQIDL    0x26
#define PICC_ANTICOLL1 0x93

/*---------------- 软件SPI底层 ----------------*/
static void RC522_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOD, ENABLE);

    /* CS=PD6  SCK=PD7 推挽输出 */
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_InitStruct.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* MOSI=PC6  RST=PC11 推挽输出 */
    GPIO_InitStruct.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_11;
    GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* MISO=PC8 上拉输入 */
    GPIO_InitStruct.GPIO_Pin  = GPIO_Pin_8;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOC, &GPIO_InitStruct);

    RC522_CS_H; RC522_RST_H; RC522_SCK_L;
}

/* SPI收发一字节: 模式0, MSB先发, 时钟空闲低, 上升沿采样 */
static u8 RC522_SPI_Byte(u8 tx)
{
    u8 i, rx = 0;
    for(i = 0; i < 8; i++) {
        if(tx & 0x80) RC522_MOSI_H; else RC522_MOSI_L;
        tx <<= 1;
        delay_us(5);
        RC522_SCK_H;
        delay_us(5);
        rx = (rx << 1) | RC522_MISO;
        RC522_SCK_L;
    }
    return rx;
}

/*---------------- 寄存器读写 ----------------*/
static void RC522_WriteReg(u8 addr, u8 val)
{
    RC522_CS_L;
    RC522_SPI_Byte((addr << 1) & 0x7E);   /* 写: bit7=0 */
    RC522_SPI_Byte(val);
    RC522_CS_H;
}

static u8 RC522_ReadReg(u8 addr)
{
    u8 val;
    RC522_CS_L;
    RC522_SPI_Byte(((addr << 1) & 0x7E) | 0x80);  /* 读: bit7=1 */
    val = RC522_SPI_Byte(0x00);
    RC522_CS_H;
    return val;
}

static void RC522_SetBit(u8 addr, u8 mask) { RC522_WriteReg(addr, RC522_ReadReg(addr) | mask); }
static void RC522_ClearBit(u8 addr, u8 mask){ RC522_WriteReg(addr, RC522_ReadReg(addr) & ~mask); }

static void RC522_AntennaOn(void)
{
    if((RC522_ReadReg(TX_CTRL_REG) & 0x03) != 0x03)
        RC522_SetBit(TX_CTRL_REG, 0x03);   /* 打开天线 */
}

/*---------------- 卡片通讯核心(正点原子流程) ----------------*/
/* 与卡数据交换, 返回0成功; 1=无卡/超时/错误 */
static u8 RC522_ToCard(u8 cmd, u8 *tx, u8 txLen, u8 *rx, u16 *rxLen)
{
    u8  irqEn = 0x00, waitIrq = 0x00;
    u8  lastBits, n, status = 1;
    u16 i;

    if (cmd == PCD_TRANSCEIVE) { irqEn = 0x77; waitIrq = 0x30; }

    RC522_WriteReg(IRQ_EN_REG, irqEn | 0x80);   /* 使能对应中断信号 */
    RC522_ClearBit(IRQ_REG, 0x80);
    RC522_WriteReg(CMD_REG, PCD_IDLE);          /* 停止当前命令 */
    RC522_SetBit(FIFO_LVL_REG, 0x80);           /* 清FIFO */
    for (i = 0; i < txLen; i++) RC522_WriteReg(FIFO_DATA_REG, tx[i]);
    RC522_WriteReg(CMD_REG, cmd);               /* 启动命令 */
    if (cmd == PCD_TRANSCEIVE) RC522_SetBit(BIT_FRAME_REG, 0x80);  /* 开始收发 */

    /* 等命令完成(定时器中断 或 收发中断) */
    i = 6000;
    do {
        n = RC522_ReadReg(IRQ_REG);
        i--;
    } while (i && !(n & 0x01) && !(n & waitIrq));
    RC522_ClearBit(BIT_FRAME_REG, 0x80);

    if (i) {                                    /* 非超时退出 */
        if (!(RC522_ReadReg(ERROR_REG) & 0x1B)) /* 无协议/奇偶/CRC/溢出错误 */
        {
            status = 0;
            if ((n & irqEn & 0x01) && cmd == PCD_TRANSCEIVE)
                status = 1;                     /* 定时器到=无卡 */

            if (cmd == PCD_TRANSCEIVE && status == 0)
            {
                n = RC522_ReadReg(FIFO_LVL_REG);
                lastBits = RC522_ReadReg(CTRL_REG) & 0x07;
                if (lastBits) *rxLen = (n - 1) * 8 + lastBits;
                else          *rxLen = n * 8;
                if (n == 0) n = 1;
                if (n > 10) n = 10;
                for (i = 0; i < n; i++) rx[i] = RC522_ReadReg(FIFO_DATA_REG);
            }
        }
        else status = 1;
    }
    else status = 1;

    RC522_SetBit(CTRL_REG, 0x80);               /* 停定时器 */
    RC522_WriteReg(CMD_REG, PCD_IDLE);
    return status;
}

/*---------------- 对外接口 ----------------*/
/* 初始化: 复位 → ISO14443-A 配置 → 开天线 */
void RC522_Init(void)
{
    u16 t;

    RC522_GPIO_Init();
    RC522_RST_H;  delay_ms(1);
    RC522_RST_L;  delay_ms(1);
    RC522_RST_H;  delay_ms(1);                  /* 硬复位 */

    RC522_WriteReg(CMD_REG, PCD_RESET);         /* 软复位 */
    t = 1000;
    while ((RC522_ReadReg(CMD_REG) & 0x10) && t--) delay_ms(1);  /* 等自检结束 */
    delay_ms(1);

    RC522_ClearBit(STATUS2_REG, 0x08);          /* 关Crypto1 */
    RC522_WriteReg(MODE_REG,       0x3D);       /* CRC初值0x6363 */
    RC522_WriteReg(TXASK_REG,      0x40);       /* 100%ASK */
    RC522_WriteReg(0x18,           0x84);       /* RxThreshold */
    RC522_WriteReg(TPRESCALER_REG, 0x3E);
    RC522_WriteReg(TRELOAD_L,      30);
    RC522_WriteReg(TRELOAD_H,      0);
    RC522_WriteReg(TMODE_REG,      0x8D);

    RC522_AntennaOn();

    printf("RC522 ver: 0x%02X\r\n", RC522_ReadReg(0x37));  /* 应为0x91/0x92 */
}

/* 寻卡 + 防冲突读UID, 返回0=读到卡 */
u8 RC522_CheckCard(u8 *uid)
{
    u8  txBuf[4], rxBuf[10];
    u16 rxLen = 0;
    u8  i, bcc = 0;

    /* 寻卡: REQA, ATQA应为16位 */
    RC522_WriteReg(BIT_FRAME_REG, 0x07);        /* 发7位 */
    txBuf[0] = PICC_REQIDL;
    if(RC522_ToCard(PCD_TRANSCEIVE, txBuf, 1, rxBuf, &rxLen) != 0) return 1;
    if(rxLen != 16) return 1;

    /* 防冲突: 4字节UID+BCC = 40位 */
    RC522_WriteReg(BIT_FRAME_REG, 0x00);
    txBuf[0] = PICC_ANTICOLL1; txBuf[1] = 0x20;
    if(RC522_ToCard(PCD_TRANSCEIVE, txBuf, 2, rxBuf, &rxLen) != 0) return 1;
    if(rxLen != 40) return 1;

    for(i = 0; i < 4; i++) { uid[i] = rxBuf[i]; bcc ^= rxBuf[i]; }
    if(bcc != rxBuf[4]) return 1;               /* BCC校验 */
    return 0;
}
/* 带调试输出的读卡: 加卡学习模式排障用 */
u8 RC522_CheckCardDbg(u8 *uid)
{
    u8  txBuf[4], rxBuf[10];
    u16 rxLen = 0;
    u8  i, bcc = 0;

    RC522_WriteReg(BIT_FRAME_REG, 0x07);
    txBuf[0] = PICC_REQIDL;
    if (RC522_ToCard(PCD_TRANSCEIVE, txBuf, 1, rxBuf, &rxLen) != 0) {
        printf("dbg: REQA no card\r\n");
        return 1;
    }
    printf("dbg: REQA ok bits=%u atqa=%02X %02X\r\n", rxLen, rxBuf[0], rxBuf[1]);
    if (rxLen != 16) { printf("dbg: atqa err\r\n"); return 1; }

    RC522_WriteReg(BIT_FRAME_REG, 0x00);
    txBuf[0] = PICC_ANTICOLL1; txBuf[1] = 0x20;
    if (RC522_ToCard(PCD_TRANSCEIVE, txBuf, 2, rxBuf, &rxLen) != 0) {
        printf("dbg: anticoll fail\r\n");
        return 1;
    }
    printf("dbg: bits=%u uid=%02X %02X %02X %02X bcc=%02X\r\n",
           rxLen, rxBuf[0], rxBuf[1], rxBuf[2], rxBuf[3], rxBuf[4]);
    if (rxLen != 40) { printf("dbg: uid len err\r\n"); return 1; }

    for (i = 0; i < 4; i++) { uid[i] = rxBuf[i]; bcc ^= rxBuf[i]; }
    if (bcc != rxBuf[4]) { printf("dbg: bcc err\r\n"); return 1; }
    return 0;
}