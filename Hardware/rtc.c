#include "rtc.h"
#include <stdio.h>
#include "delay.h"

#define RTC_BKP_FLAG_VALUE  0x32F5

/* SPL的RTC_ByteToBcd2是static不对外, 这里本地实现一个: 十进制转BCD */
static uint8_t ToBcd(uint8_t val)
{
    return (uint8_t)(((val / 10) << 4) | (val % 10));
}

/* RTC初始化(流程与验证通过的独立例程保持一致):
   备份寄存器有标记=已初始化过(掉电由纽扣电池走时), 跳过不动;
   无标记=首次上电, 按 rtc.h 宏用 BCD 格式设置日期时间 */
void RTC_Config(void)
{
    RTC_InitTypeDef RTC_InitStructure;
    RTC_DateTypeDef RTC_DateStructure;
    RTC_TimeTypeDef RTC_TimeStructure;
    uint32_t timeout;

    /* 使能PWR时钟, 允许访问RTC备份域 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    PWR_BackupAccessCmd(ENABLE);

    if (RTC_ReadBackupRegister(RTC_BKP_DR0) == RTC_BKP_FLAG_VALUE)
    {
        printf("rtc: backup flag found, keep running\r\n");
        RTC_WaitForSynchro();
        return;
    }

    printf("rtc: first init, set time\r\n");

    /* LSE(32.768kHz)起振需1~2秒, 等足3秒; 不起振回退LSI */
    RCC_LSEConfig(RCC_LSE_ON);
    for (timeout = 0; timeout < 3000; timeout++) {
        if (RCC_GetFlagStatus(RCC_FLAG_LSERDY) != RESET) break;
        delay_ms(1);
    }
    if (RCC_GetFlagStatus(RCC_FLAG_LSERDY) != RESET) {
        RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
        RTC_InitStructure.RTC_SynchPrediv  = 0xFF;   /* 256 */
        RTC_InitStructure.RTC_AsynchPrediv = 0x7F;   /* 128 → 32768/256/128 = 1Hz */
        printf("rtc: clock=LSE\r\n");
    } else {
        RCC_LSICmd(ENABLE);                          /* F4的LSI默认关闭, 必须先使能 */
        while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET);
        RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
        RTC_InitStructure.RTC_SynchPrediv  = 0xF9;   /* 250 */
        RTC_InitStructure.RTC_AsynchPrediv = 0x7F;   /* 128 → 32000/250/128 = 1Hz */
        printf("rtc: clock=LSI (LSE未起振!)\r\n");
    }
    RTC_InitStructure.RTC_HourFormat = RTC_HourFormat_24;
    RCC_RTCCLKCmd(ENABLE);
    RTC_WaitForSynchro();
    RTC_Init(&RTC_InitStructure);

    /* 设置初始日期时间: BCD格式(与验证例程一致), 值来自 rtc.h 宏 */
    RTC_DateStructure.RTC_Year    = ToBcd(RTC_SET_YEAR);
    RTC_DateStructure.RTC_Month   = ToBcd(RTC_SET_MONTH);
    RTC_DateStructure.RTC_Date    = ToBcd(RTC_SET_DAY);
    RTC_DateStructure.RTC_WeekDay = ToBcd(RTC_SET_WEEKDAY);   
    RTC_SetDate(RTC_Format_BCD, &RTC_DateStructure);

    RTC_TimeStructure.RTC_H12     = RTC_H12_AM;
    RTC_TimeStructure.RTC_Hours   = ToBcd(RTC_SET_HOUR);
    RTC_TimeStructure.RTC_Minutes = ToBcd(RTC_SET_MIN);
    RTC_TimeStructure.RTC_Seconds = ToBcd(RTC_SET_SEC);
    RTC_SetTime(RTC_Format_BCD, &RTC_TimeStructure);

    RTC_WriteBackupRegister(RTC_BKP_DR0, RTC_BKP_FLAG_VALUE);
    printf("rtc: set %02d:%02d:%02d ok\r\n", RTC_SET_HOUR, RTC_SET_MIN, RTC_SET_SEC);
}

/* 串口打印日期时间 (先读时间再读日期, 解锁影子寄存器) */
void RTC_TimeShow(void)
{
    RTC_DateTypeDef RTC_DateStructure;
    RTC_TimeTypeDef RTC_TimeStructure;

    RTC_GetTime(RTC_Format_BIN, &RTC_TimeStructure);
    RTC_GetDate(RTC_Format_BIN, &RTC_DateStructure);

    printf("20%02d/%02d/%02d week%02d\r\n",
           RTC_DateStructure.RTC_Year,
           RTC_DateStructure.RTC_Month,
           RTC_DateStructure.RTC_Date,
           RTC_DateStructure.RTC_WeekDay);
    printf("%02d:%02d:%02d\r\n",
           RTC_TimeStructure.RTC_Hours,
           RTC_TimeStructure.RTC_Minutes,
           RTC_TimeStructure.RTC_Seconds);
}

void RTC_ReadHMS(uint8_t *h, uint8_t *m, uint8_t *s)
{
    RTC_TimeTypeDef t;
    RTC_DateTypeDef d;
    RTC_GetTime(RTC_Format_BIN, &t);
    RTC_GetDate(RTC_Format_BIN, &d);    /* 必须读一次日期, 否则影子寄存器不解锁, 时间冻住 */
    *h = t.RTC_Hours;
    *m = t.RTC_Minutes;
    *s = t.RTC_Seconds;
}

uint32_t RTC_Packed(void)
{
    uint8_t h, m, s;
    RTC_ReadHMS(&h, &m, &s);
    return ((uint32_t)h << 16) | ((uint32_t)m << 8) | s;
}
