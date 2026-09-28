#include "storage.h"
#include "w25qxx.h"
#include "rtc.h"
#include <string.h>

/*------------ Flash 地址规划 ------------*/
#define CFG_ADDR     0x000000     /* 扇区0: 配置(密码) */
#define CARD_ADDR    0x001000     /* 扇区1: 卡白名单 */
#define LOG_START    0x004000     /* 扇区4: 记录区起点 */
#define LOG_END      0x014000     /* 扇区20: 记录区终点(16扇区=4096条) */

#define CFG_MAGIC    0xD0C4D0C4
#define CARD_MAGIC   0xCAC0CAC0
#define LOG_VALID    0xA5

/*------------ RAM 缓存 ------------*/
static char    g_pwd[STORAGE_PWD_LEN + 1];
static uint8_t g_pwdValid;
static uint8_t g_cards[STORAGE_MAX_CARDS][4];
static uint16_t g_cardCount;
static uint32_t g_logAddr;        /* 下一条记录写入地址 */
static uint32_t g_seq;            /* 记录递增序号 */

/*======================== 配置区(密码) ========================*/
static void Config_Load(void)
{
    uint32_t magic;
    W25QXX_Read(CFG_ADDR, (uint8_t *)&magic, 4);
    if (magic == CFG_MAGIC)
    {
        W25QXX_Read(CFG_ADDR + 4, (uint8_t *)g_pwd, STORAGE_PWD_LEN);
        g_pwd[STORAGE_PWD_LEN] = '\0';
        g_pwdValid = 1;
    }
    else
    {
        strcpy(g_pwd, "123456");   /* 默认密码 */
        g_pwdValid = 0;
    }
}

uint8_t Storage_GetPassword(char *pwd)
{
    strcpy(pwd, g_pwd);
    return g_pwdValid;
}

uint8_t Storage_SetPassword(const char *pwd)
{
    uint8_t buf[16] = {0xFF};
    uint32_t magic = CFG_MAGIC;
    uint8_t i;

    if (strlen(pwd) != STORAGE_PWD_LEN) return 0;
    for (i = 0; i < STORAGE_PWD_LEN; i++)
        if (pwd[i] < '0' || pwd[i] > '9') return 0;

    W25QXX_EraseSector(CFG_ADDR);          /* 扇区0只存配置, 整体重写 */
    memcpy(buf, &magic, 4);
    memcpy(buf + 4, pwd, STORAGE_PWD_LEN);
    W25QXX_Write(CFG_ADDR, buf, 16);

    strcpy(g_pwd, pwd);
    g_pwdValid = 1;
    return 1;
}

/*======================== 卡白名单 ========================*/
static void Card_Load(void)
{
    uint32_t magic;
    uint16_t count;
    W25QXX_Read(CARD_ADDR, (uint8_t *)&magic, 4);
    W25QXX_Read(CARD_ADDR + 4, (uint8_t *)&count, 2);
    if (magic == CARD_MAGIC && count <= STORAGE_MAX_CARDS)
    {
        W25QXX_Read(CARD_ADDR + 8, (uint8_t *)g_cards, count * 4);
        g_cardCount = count;
    }
    else
    {
        g_cardCount = 0;
    }
}

/* 白名单整体回写(扇区1只存卡, 可整体擦写) */
static void Card_Save(void)
{
    uint32_t magic = CARD_MAGIC;
    W25QXX_EraseSector(CARD_ADDR);
    W25QXX_Write(CARD_ADDR,     (uint8_t *)&magic, 4);
    W25QXX_Write(CARD_ADDR + 4, (uint8_t *)&g_cardCount, 2);
    if (g_cardCount > 0)
        W25QXX_Write(CARD_ADDR + 8, (uint8_t *)g_cards, g_cardCount * 4);
}

uint8_t Storage_CheckCard(const uint8_t uid[4])
{
    uint16_t i;
    for (i = 0; i < g_cardCount; i++)
        if (memcmp(g_cards[i], uid, 4) == 0) return 1;
    return 0;
}

uint8_t Storage_AddCard(const uint8_t uid[4])
{
    if (g_cardCount >= STORAGE_MAX_CARDS) return 0;
    if (Storage_CheckCard(uid)) return 0;
    memcpy(g_cards[g_cardCount], uid, 4);
    g_cardCount++;
    Card_Save();
    return 1;
}

uint8_t Storage_DelCard(const uint8_t uid[4])
{
    uint16_t i;
    for (i = 0; i < g_cardCount; i++)
    {
        if (memcmp(g_cards[i], uid, 4) == 0)
        {
            /* 用最后一张覆盖被删的 */
            memcpy(g_cards[i], g_cards[g_cardCount - 1], 4);
            g_cardCount--;
            Card_Save();
            return 1;
        }
    }
    return 0;
}

uint16_t Storage_CardCount(void) { return g_cardCount; }

void Storage_ClearCards(void)
{
    g_cardCount = 0;
    Card_Save();
}

/*======================== 开锁记录(循环区) ========================*/
/* 扫描记录区, 找到第一个空槽作为写入位置, 同时恢复最大序号 */
static void Log_Scan(void)
{
    uint8_t  chunk[256];
    uint32_t a;
    uint16_t off;
    LogRecord *rec;

    g_logAddr = LOG_END;    /* 先假设已满 */
    g_seq = 0;

    for (a = LOG_START; a < LOG_END; a += 256)
    {
        W25QXX_Read(a, chunk, 256);
        for (off = 0; off < 256; off += 16)
        {
            rec = (LogRecord *)(chunk + off);
            if (rec->valid == 0xFF)        /* 找到空槽 */
            {
                g_logAddr = a + off;
                return;
            }
            if (rec->valid == LOG_VALID && rec->seq > g_seq)
                g_seq = rec->seq;
        }
    }

    /* 没找到空槽说明记满了: 从起点擦除重来 */
    W25QXX_EraseSector(LOG_START);
    g_logAddr = LOG_START;
}

void Storage_AddLog(uint8_t type, uint16_t id)
{
    LogRecord rec;
    rec.valid    = LOG_VALID;
    rec.type     = type;
    rec.id       = id;
    rec.seq      = ++g_seq;
    rec.time     = RTC_Packed();
    rec.reserved = 0;

    /* 进入一个新区块前先擦除(循环覆盖最旧记录) */
    if ((g_logAddr % W25QXX_SECTOR_SIZE) == 0)
        W25QXX_EraseSector(g_logAddr);

    W25QXX_Write(g_logAddr, (uint8_t *)&rec, sizeof(LogRecord));

    g_logAddr += sizeof(LogRecord);
    if (g_logAddr >= LOG_END) g_logAddr = LOG_START;
}

/* 倒序读最近的记录, 返回实际读到的条数 */
uint16_t Storage_ReadLastLogs(LogRecord *out, uint16_t maxCount)
{
    LogRecord rec;
    uint32_t addr = g_logAddr;
    uint16_t n = 0;
    uint16_t capacity = (LOG_END - LOG_START) / sizeof(LogRecord);

    while (n < maxCount && n < capacity)
    {
        addr = (addr == LOG_START) ? (LOG_END - sizeof(LogRecord))
                                   : (addr - sizeof(LogRecord));
        W25QXX_Read(addr, (uint8_t *)&rec, sizeof(LogRecord));
        if (rec.valid != LOG_VALID) break;
        out[n++] = rec;
    }
    return n;
}

uint32_t Storage_LogSeq(void) { return g_seq; }

void Storage_ClearLogs(void)
{
	  uint32_t a;
	  for(a = LOG_START; a < LOG_END; a += W25QXX_SECTOR_SIZE)
	     W25QXX_EraseSector(a);       /*16个扇区*/
	  g_logAddr = LOG_START;
	  g_seq = 0;
}

/*======================== 初始化 ========================*/
void Storage_Init(void)
{
    W25QXX_Init();
    Config_Load();
    Card_Load();
    Log_Scan();
}

/* 读取第index张卡的UID(index从0开始), 返回1成功 */
uint8_t Storage_GetCard(uint16_t index, uint8_t uid[4])
{
    if (index >= g_cardCount) return 0;
    memcpy(uid, g_cards[index], 4);
    return 1;
}
