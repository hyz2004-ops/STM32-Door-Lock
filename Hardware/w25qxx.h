#ifndef __W25QXX_H
#define __W25QXX_H

#include "stm32f4xx.h"

/* W25Q128: 16MB = 4096 个扇区(4KB/扇区) = 65536 页(256B/页) */
#define W25QXX_SECTOR_SIZE   4096
#define W25QXX_PAGE_SIZE     256

/* 片选: PB14, 低电平有效 */
#define W25QXX_CS_LOW()    GPIO_ResetBits(GPIOB, GPIO_Pin_14)
#define W25QXX_CS_HIGH()   GPIO_SetBits(GPIOB, GPIO_Pin_14)

void     W25QXX_Init(void);
uint32_t W25QXX_ReadJEDEC(void);                       /* 应返回 0x00EF4018 */
void     W25QXX_Read(uint32_t addr, uint8_t *buf, uint32_t len);
void     W25QXX_WritePage(uint32_t addr, const uint8_t *buf, uint16_t len); /* 单页内写, <=256 */
void     W25QXX_Write(uint32_t addr, const uint8_t *buf, uint32_t len);     /* 跨页写(不擦除) */
void     W25QXX_EraseSector(uint32_t sectorAddr);      /* 擦除4KB扇区, 地址需4KB对齐 */
void     W25QXX_EraseChip(void);

#endif