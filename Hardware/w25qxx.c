#include "w25qxx.h"
#include "delay.h"

/* W25Q128 指令集 */
#define W25X_WriteEnable      0x06
#define W25X_ReadStatusReg1   0x05
#define W25X_ReadData         0x03
#define W25X_PageProgram      0x02
#define W25X_SectorErase      0x20
#define W25X_ChipErase        0xC7
#define W25X_ReadJedecID      0x9F

/* SPI1: SCK=PB3  MISO=PB4  MOSI=PB5 (AF5), CS=PB14(普通IO)
   注意: PB3/PB4 默认是 JTAG 脚, F4 上直接配置成 AF5 即可覆盖,
   但烧录调试必须用 SWD 模式, 不能用 JTAG 模式 */
void W25QXX_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    SPI_InitTypeDef   SPI_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);

    /* PB3(SCK) PB4(MISO) PB5(MOSI) 复用为 SPI1 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource3, GPIO_AF_SPI1);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource4, GPIO_AF_SPI1);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource5, GPIO_AF_SPI1);

    /* PB14 片选, 普通推挽输出 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_14;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    W25QXX_CS_HIGH();

    /* SPI1: 模式0(CPOL=0,CPHA=0), 主机, 8bit, MSB先行
       APB2=84MHz, 2分频=42MHz, W25Q128 最高支持104MHz */
    SPI_I2S_DeInit(SPI1);
    SPI_InitStructure.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode              = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize          = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL              = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA              = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS               = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
    SPI_InitStructure.SPI_FirstBit          = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial     = 7;
    SPI_Init(SPI1, &SPI_InitStructure);

    SPI_Cmd(SPI1, ENABLE);
}

/* SPI 收发一个字节 */
static uint8_t SPI1_SwapByte(uint8_t txData)
{
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SPI1, txData);
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
    return SPI_I2S_ReceiveData(SPI1);
}

/* 读 JEDEC ID, W25Q128 应为 0xEF(厂商) 0x40 0x18(容量) */
uint32_t W25QXX_ReadJEDEC(void)
{
    uint32_t id = 0;
    W25QXX_CS_LOW();
    SPI1_SwapByte(W25X_ReadJedecID);
    id  = (uint32_t)SPI1_SwapByte(0xFF) << 16;
    id |= (uint32_t)SPI1_SwapByte(0xFF) << 8;
    id |= (uint32_t)SPI1_SwapByte(0xFF);
    W25QXX_CS_HIGH();
    return id;
}

static void W25QXX_WriteEnable(void)
{
    W25QXX_CS_LOW();
    SPI1_SwapByte(W25X_WriteEnable);
    W25QXX_CS_HIGH();
}

/* 等待芯片空闲(写/擦除操作后必须等待) */
static void W25QXX_WaitBusy(void)
{
    W25QXX_CS_LOW();
    SPI1_SwapByte(W25X_ReadStatusReg1);
    while (SPI1_SwapByte(0xFF) & 0x01);
    W25QXX_CS_HIGH();
}

/* 读数据: 任意地址, 任意长度 */
void W25QXX_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    W25QXX_CS_LOW();
    SPI1_SwapByte(W25X_ReadData);
    SPI1_SwapByte((addr >> 16) & 0xFF);
    SPI1_SwapByte((addr >> 8)  & 0xFF);
    SPI1_SwapByte(addr & 0xFF);
    while (len--) *buf++ = SPI1_SwapByte(0xFF);
    W25QXX_CS_HIGH();
}

/* 页编程: 单次最多256字节, 不能跨页! 写入前对应区域必须已擦除(为0xFF) */
void W25QXX_WritePage(uint32_t addr, const uint8_t *buf, uint16_t len)
{
    W25QXX_WriteEnable();
    W25QXX_CS_LOW();
    SPI1_SwapByte(W25X_PageProgram);
    SPI1_SwapByte((addr >> 16) & 0xFF);
    SPI1_SwapByte((addr >> 8)  & 0xFF);
    SPI1_SwapByte(addr & 0xFF);
    while (len--) SPI1_SwapByte(*buf++);
    W25QXX_CS_HIGH();
    W25QXX_WaitBusy();
}

/* 跨页写(自动分页, 不负责擦除) */
void W25QXX_Write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint16_t pageRemain = W25QXX_PAGE_SIZE - (addr % W25QXX_PAGE_SIZE);
    if (len <= pageRemain) pageRemain = len;

    while (1)
    {
        W25QXX_WritePage(addr, buf, pageRemain);
        if (pageRemain == len) break;
        buf  += pageRemain;
        addr += pageRemain;
        len  -= pageRemain;
        pageRemain = (len > W25QXX_PAGE_SIZE) ? W25QXX_PAGE_SIZE : len;
    }
}

/* 擦除一个4KB扇区, 擦除后全为0xFF, 约需几十ms */
void W25QXX_EraseSector(uint32_t sectorAddr)
{
    W25QXX_WriteEnable();
    W25QXX_CS_LOW();
    SPI1_SwapByte(W25X_SectorErase);
    SPI1_SwapByte((sectorAddr >> 16) & 0xFF);
    SPI1_SwapByte((sectorAddr >> 8)  & 0xFF);
    SPI1_SwapByte(sectorAddr & 0xFF);
    W25QXX_CS_HIGH();
    W25QXX_WaitBusy();
}

/* 整片擦除, 约需20s+, 一般不用 */
void W25QXX_EraseChip(void)
{
    W25QXX_WriteEnable();
    W25QXX_CS_LOW();
    SPI1_SwapByte(W25X_ChipErase);
    W25QXX_CS_HIGH();
    W25QXX_WaitBusy();
}