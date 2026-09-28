#ifndef __STORAGE_H
#define __STORAGE_H

#include "stm32f4xx.h"

#define STORAGE_PWD_LEN    6      /* 密码固定6位数字 */
#define STORAGE_MAX_CARDS  64     /* 白名单容量 */

/* 开锁记录类型 */
#define LOG_TYPE_PWD     0        /* 密码开锁 */
#define LOG_TYPE_FING    1        /* 指纹开锁 */
#define LOG_TYPE_CARD    2        /* 刷卡开锁 */
#define LOG_TYPE_CLOUD   3        /* 云端远程开锁 */

/* 开锁记录, 固定16字节 */
#pragma pack(1)
typedef struct
{
    uint8_t  valid;     /* 0xA5=有效, 0xFF=空 */
    uint8_t  type;      /* LOG_TYPE_xxx */
    uint16_t id;        /* 指纹ID或卡序号, 无则0 */
    uint32_t seq;       /* 全局递增序号 */
    uint32_t time;      /* 预留(可做RTC时间戳) */
    uint32_t reserved;
} LogRecord;
#pragma pack()

void     Storage_Init(void);

/* 密码 */
uint8_t  Storage_GetPassword(char *pwd);            /* pwd至少7字节缓冲 */
uint8_t  Storage_SetPassword(const char *pwd);      /* 必须6位数字, 返回1成功 */

/* IC卡白名单 */
uint8_t  Storage_AddCard(const uint8_t uid[4]);     /* 1成功 0已满/已存在 */
uint8_t  Storage_DelCard(const uint8_t uid[4]);     /* 1成功 0不存在 */
uint8_t  Storage_CheckCard(const uint8_t uid[4]);   /* 1=在白名单内 */
uint16_t Storage_CardCount(void);
void     Storage_ClearCards(void);

/* 开锁记录 */
void     Storage_AddLog(uint8_t type, uint16_t id);
uint16_t Storage_ReadLastLogs(LogRecord *out, uint16_t maxCount); /* 最新在前 */
uint32_t Storage_LogSeq(void);                      /* 当前序号, 可用于判断是否新记录 */
void   Storage_ClearLogs(void);                     /* 清空所有开锁记录 */
uint8_t  Storage_GetCard(uint16_t index, uint8_t uid[4]); /* 按序号取卡UID */

#endif