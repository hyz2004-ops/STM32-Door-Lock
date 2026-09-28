#include "stm32f4xx.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "delay.h"
#include "usart.h"
#include "led.h"
#include "beep.h"
#include "keypad.h"
#include "oled.h"
#include "oled_str.h"
#include "lock.h"
#include "as608.h"
#include "esp8266.h"
#include "mqtt.h"
#include "storage.h"
#include "rc522.h"
#include "rtc.h"
#include "scheduler.h"

/*================= 云端配置 =================*/
#define TOPIC_SUB    "doorlock002"     /* 下行: 小程序下发指令 */
#define TOPIC_PUB    "doorlockLog"     /* 上行: 开锁记录/应答 */
#define FING_MAX     5                 /* 指纹上限: ID 0~4 */

/*================= 全局状态 =================*/
volatile uint32_t g_ms = 0;            /* 毫秒时基(主循环累加, 调度器用) */
static uint8_t  g_cloudOnline = 0;     /* 云端是否在线 */
static uint32_t g_lastPing = 0;
static uint32_t g_lastRetry = 0;
static uint8_t  g_failCount = 0;       /* 密码连续错误次数 */
static uint32_t g_lockoutEnd = 0;      /* 锁定截止时间, 0=未锁定 */
static uint32_t g_cardLearnEnd = 0;    /* 加卡学习模式截止时间 */
static uint32_t g_cardNextAllow = 0;   /* 刷卡防连触发: 下次允许检测时刻 */
static uint32_t g_uiRestoreAt = 0;     /* 界面恢复时刻(指纹失败提示后) */
static uint32_t g_lastActive = 0;      /* 最后操作时刻: 10秒无操作熄屏 */
static uint8_t  g_oledSleep  = 0;      /* 1=OLED已熄屏 */
static uint8_t  g_inputLen   = 0;      /* 已输入密码位数(供唤醒后重绘*号) */

/* 是否处于锁定期 */
static uint8_t IsLocked(void)
{
    return (g_lockoutEnd && g_ms < g_lockoutEnd) ? 1 : 0;
}

/*================= OLED 界面 =================*/
static void UI_DrawBase(void)
{
    OLED_Clear();
    OLED_ShowString(32, 0, (char *)STR_TITLE);        /* 智能门锁 */
    OLED_ShowString(0,  2, (char *)STR_INPUT_PWD);    /* 请输入密码: */
    OLED_ShowString(0,  6, (char *)STR_STATE);        /* 状态: */
    OLED_ShowString(48, 6, g_cloudOnline ? (char *)STR_ONLINE
                                         : (char *)STR_OFFLINE);
}

/* 在第4页显示已输入的密码位数(用*号) */
static void UI_ShowInput(uint8_t len)
{
    uint8_t i;
    for (i = 0; i < STORAGE_PWD_LEN; i++)
        OLED_ShowChar(24 + i * 16, 4, (i < len) ? '*' : ' ');
}

/* 任意操作通知: 刷新活动时间; 若已熄屏则点亮并恢复登录界面 */
static void Activity_Notify(void)
{
    g_lastActive = g_ms;
    if (g_oledSleep)
    {
        g_oledSleep   = 0;
        g_uiRestoreAt = 0;
        OLED_On();
        UI_DrawBase();
        UI_ShowInput(g_inputLen);
    }
}

/* 建立MQTT连接 */
static void Cloud_Connect(void)
{
    if (MQTT_Connect(BEMFA_KEY) == 0)
    {
        MQTT_Subscribe(TOPIC_SUB);
        g_cloudOnline = 1;
        LED0_ON();
        printf("cloud online\r\n");
    }
}

/* 断线重连: 重建TCP链路 + 重新MQTT连接, 最多试3次 */
static void Cloud_Reconnect(void)
{
    u8 i;
    printf("cloud reconnect...\r\n");
    g_cloudOnline = 0;
    LED0_OFF();

    for (i = 0; i < 3; i++)
    {
        if (ESP8266_Init() == 0)            /* 内部含 +++/CIPSHUT/连WiFi/连TCP/透传 */
        {
            Cloud_Connect();
            if (g_cloudOnline)
            {
                UI_DrawBase();              /* 刷新OLED在线状态 */
                return;
            }
        }
        delay_ms(3000);
    }
    printf("reconnect fail, retry later\r\n");
    UI_DrawBase();
}

/*================= 统一开锁动作 =================*/
static void DoUnlock(uint8_t type, uint16_t id)
{
    char msg[64];
    const char *way[] = {"PWD", "FING", "CARD", "CLOUD"};

    Activity_Notify();                                /* 任意开锁方式先点亮OLED */
    OLED_ShowString(0, 6, (char *)STR_OPEN_OK);       /* 开锁成功 */
    Beep_Buzz(2000, 100);
    LED1_ON();

    Storage_AddLog(type, id);                          /* 本地存档 */
    {
        uint8_t hh, mm, ss;
        RTC_ReadHMS(&hh, &mm, &ss);
        printf("unlock: type=%s id=%u seq=%d %02d:%02d:%02d\r\n",
               way[type], id, Storage_LogSeq(), hh, mm, ss);
    }

    if (g_cloudOnline)                                 /* 记录上云(带时分秒) */
    {
        uint8_t hh, mm, ss;
        RTC_ReadHMS(&hh, &mm, &ss);
        sprintf(msg, "open,%s,id%u,no%d,%02d:%02d:%02d",
                way[type], id, Storage_LogSeq(), hh, mm, ss);
        MQTT_Publish(TOPIC_PUB, msg);
    }

    Lock_Open();                                       /* 开锁3s自动上锁 */
    LED1_OFF();
    g_failCount = 0;

    UI_DrawBase();                                     /* 恢复登录界面 */
    UI_ShowInput(0);
}

/*================= 密码错误处理 =================*/
static void OnPwdError(void)
{
    g_failCount++;
    OLED_ShowString(0, 6, (char *)STR_PWD_ERR);
    Beep_Buzz(500, 300);                               /* 低频长鸣=错误 */

    if (g_failCount >= 3)                              /* 连续3次错误, 锁定60s */
    {
        g_lockoutEnd = g_ms + 60000;
        OLED_ShowString(0, 6, (char *)STR_LOCKED);
        if (g_cloudOnline)
            MQTT_Publish(TOPIC_PUB, "alarm,pwd fail x3,locked 60s");
        printf("alarm: locked 60s\r\n");
    }
}

/* 找第一个空闲ID, 满了返回-1 */
static int Fing_FindFreeID(const u8 *bits)
{
    int i;
    for (i = 0; i < FING_MAX; i++)
        if (!(bits[i / 8] & (1 << (i % 8)))) return i;
    return -1;
}

/* 统计已登记数量(只算ID 0~4) */
static uint8_t Fing_Count(const u8 *bits)
{
    uint8_t n = 0, i;
    for (i = 0; i < FING_MAX; i++)
        if (bits[i / 8] & (1 << (i % 8))) n++;
    return n;
}

/*================= 云端指令处理(mqtt.c里是弱定义,在此实现) =================*/
/* 回复: 在线就发到云端, 同时打印到串口(串口调试时也能看到结果) */
static void Cloud_Reply(const char *msg)
{
    if (g_cloudOnline) MQTT_Publish(TOPIC_PUB, msg);
    printf("reply: %s\r\n", msg);
}

/* 去掉命令尾部的 # \r \n 空格 */
static void Cmd_Trim(char *s)
{
    int n = strlen(s);
    while (n > 0 && (s[n-1] == '#' || s[n-1] == '\r' ||
                     s[n-1] == '\n' || s[n-1] == ' '))
        s[--n] = '\0';
}

/* 统一命令执行器: 云端和串口1共用 */
static void Cmd_Execute(char *payload)
{
    char msg[64];
    const char *way[] = {"PWD", "FING", "CARD", "CLOUD"};

    Cmd_Trim(payload);
    if (payload[0] == '\0') return;
    printf("cmd exec: %s\r\n", payload);

    if (strcmp(payload, "on") == 0)                            /* 远程开锁 */
    {
        DoUnlock(LOG_TYPE_CLOUD, 0);
    }
    else if (strncmp(payload, "PWD:", 4) == 0)                 /* 改密码 */
    {
        Cloud_Reply(Storage_SetPassword(payload + 4)
                    ? "pwd changed ok" : "pwd change fail(4-8 digits)");
    }
    else if (strcmp(payload, "FING:ADD") == 0)                 /* 添加指纹(最多5个) */
    {
        u8  bits[32];
        int id;

        if (AS608_ReadIndex(bits) != AS608_OK)
            Cloud_Reply("finger read fail");
        else if ((id = Fing_FindFreeID(bits)) < 0)
            Cloud_Reply("finger full, max 5");
        else
        {
            Cloud_Reply("press finger twice");
            if (AS608_Enroll((uint16_t)id) == AS608_OK)
            {
                AS608_ReadIndex(bits);
                sprintf(msg, "finger add ok, id=%d (%u/5)", id, Fing_Count(bits));
                Cloud_Reply(msg);
            }
            else Cloud_Reply("finger add fail");
        }
    }
    else if (strncmp(payload, "FING:DEL:", 9) == 0)            /* 删除指定指纹 */
    {
        int id = atoi(payload + 9);
        u8  bits[32];

        if (id < 0 || id >= FING_MAX)
            Cloud_Reply("id must be 0-4");
        else if (AS608_Delete((uint16_t)id) == AS608_OK)
        {
            Cloud_Reply("finger del ok");
            if (AS608_ReadIndex(bits) == AS608_OK && Fing_Count(bits) == 0)
                Cloud_Reply("all fingers deleted!");           /* 删完提示 */
        }
        else Cloud_Reply("finger del fail(not exist)");
    }
    else if (strcmp(payload, "FING:CLEAR") == 0)               /* 清空指纹 */
    {
        Cloud_Reply(AS608_Empty() == AS608_OK
                    ? "all fingers deleted!" : "finger clear fail");
    }
    else if (strcmp(payload, "FING:LIST") == 0)                /* 查看已有指纹 */
    {
        u8 bits[32];
        if (AS608_ReadIndex(bits) != AS608_OK)
            Cloud_Reply("finger read fail");
        else
        {
            uint8_t i;
            char *p = msg;
            p += sprintf(p, "fingers %u/5: [", Fing_Count(bits));
            for (i = 0; i < FING_MAX; i++)
                if (bits[i / 8] & (1 << (i % 8)))
                    p += sprintf(p, "%u ", i);
            sprintf(p, "]");
            Cloud_Reply(msg);
        }
    }
    else if (strcmp(payload, "CARD:ADD") == 0)                 /* 进入加卡模式15秒 */
    {
        g_cardLearnEnd = g_ms + 15000;
        Cloud_Reply("card learn mode 15s, swipe now");
    }
    else if (strcmp(payload, "CARD:CLEAR") == 0)               /* 清空卡白名单 */
    {
        Storage_ClearCards();
        Cloud_Reply("all cards deleted!");
    }
    else if (strcmp(payload, "CARD:NUM") == 0)                 /* 查卡数量+明细 */
    {
        uint16_t i, n = Storage_CardCount();
        uint8_t uid[4];
        sprintf(msg, "cards: %u/64", n);
        Cloud_Reply(msg);
        for (i = 0; i < n; i++)
        {
            Storage_GetCard(i, uid);
            sprintf(msg, "card%u: %02X%02X%02X%02X", i,
                    uid[0], uid[1], uid[2], uid[3]);
            Cloud_Reply(msg);
        }
    }
    else if (strcmp(payload, "LOG") == 0)                      /* 最近5条 */
    {
        static LogRecord logs[5];               /* static: 不占栈 */
        uint16_t n = Storage_ReadLastLogs(logs, 5), i;
        Cloud_Reply("---- last 5 logs ----");
        for (i = 0; i < n; i++)
        {
            sprintf(msg, "no%d,%s,id%u,%02d:%02d:%02d",
                    logs[i].seq, way[logs[i].type], logs[i].id,
                    logs[i].time >> 16, (logs[i].time >> 8) & 0xFF, logs[i].time & 0xFF);
            Cloud_Reply(msg);
        }
    }
    else if (strcmp(payload, "LOG:ALL") == 0)                  /* 所有开锁记录 */
    {
        static LogRecord logs[100];             /* static: 1600字节不占栈, 防栈溢出死机 */
        uint16_t n = Storage_ReadLastLogs(logs, 100), i;
        sprintf(msg, "total %d logs, dump %u:", Storage_LogSeq(), n);
        Cloud_Reply(msg);
        for (i = 0; i < n; i++)
        {
            sprintf(msg, "no%d,%s,id%u,%02d:%02d:%02d",
                    logs[i].seq, way[logs[i].type], logs[i].id,
                    logs[i].time >> 16, (logs[i].time >> 8) & 0xFF, logs[i].time & 0xFF);
            Cloud_Reply(msg);
            delay_ms(200);
        }
        Cloud_Reply("---- end ----");
    }
    else if (strcmp(payload, "LOG:CLEAR") == 0)                /* 清空所有开锁记录 */
    {
        Storage_ClearLogs();
        Cloud_Reply("all logs cleared!");
    }
    else
    {
        Cloud_Reply("unknown cmd");                            /* 不认识的命令 */
    }
}

/* 云端入口: mqtt.c 弱定义被这里覆盖 */
void MQTT_RecvHandler(char *topic, char *payload)
{
    printf("cloud [%s]: %s\r\n", topic, payload);
    Cmd_Execute(payload);
}

/*============================================================*/
/*                       调 度 器 任 务                        */
/*============================================================*/

/* 串口1命令(以 # 或回车结尾) */
void UartCmd_Task(void)
{
    char cmd[U1_BUF_SIZE];
    if (u1_flag)
    {
        strcpy(cmd, (char *)u1_buf);
        u1_flag = 0;
        Cmd_Execute(cmd);
        Activity_Notify();                            /* 串口命令也算操作 */
    }
}

/* 键盘密码 */
void Keypad_Task(void)
{
    static char input[STORAGE_PWD_LEN + 1];
    char key;
    char pwd[STORAGE_PWD_LEN + 1];

    if (IsLocked()) { g_inputLen = 0; return; }      /* 锁定期丢弃按键 */

    key = Keypad_GetKey();
    if (key == 0) return;
    Beep_Buzz(3000, 30);                              /* 按键提示音 */
    Activity_Notify();                                /* 任意按键点亮OLED */

    if (key >= '0' && key <= '9')                     /* 数字键 */
    {
        if (g_inputLen < STORAGE_PWD_LEN)
        {
            input[g_inputLen++] = key;
            UI_ShowInput(g_inputLen);
        }
    }
    else if (key == '*')                              /* 清空重输 */
    {
        g_inputLen = 0;
        UI_ShowInput(0);
    }
    else if (key == '#')                              /* 确认 */
    {
        if (g_inputLen == STORAGE_PWD_LEN)
        {
            input[g_inputLen] = '\0';
            Storage_GetPassword(pwd);
            if (strcmp(input, pwd) == 0)
            {
                g_inputLen = 0;
                DoUnlock(LOG_TYPE_PWD, 0);
            }
            else
            {
                OnPwdError();
                g_inputLen = 0;
                UI_ShowInput(0);
            }
        }
        else g_inputLen = 0, UI_ShowInput(0);         /* 不满6位=清掉 */
    }
}

/* 指纹扫描 */
void Fing_Task(void)
{
    int finger;

    if (IsLocked()) return;

    finger = AS608_CheckFinger();
    if (finger >= 0)                                  /* 匹配成功 */
    {
        Activity_Notify();
        DoUnlock(LOG_TYPE_FING, (uint16_t)finger);
    }
    else if (finger == -2)                            /* 有手指但没匹配上 */
    {
        Activity_Notify();
        OLED_ShowString(0, 6, (char *)STR_FAIL);
        Beep_Buzz(500, 300);
        g_uiRestoreAt = g_ms + 800;                   /* 800ms后由UI_Task恢复界面 */
    }
}

/* RC522 刷卡 */
void Card_Task(void)
{
    u8 uid[4];
    u8 ret;
    static uint32_t dbgNext = 0;

    if (IsLocked()) return;
    if (g_ms < g_cardNextAllow) return;               /* 防同卡连触发(替代原来的delay 1s) */

    if (g_cardLearnEnd && g_ms < g_cardLearnEnd)      /* 学习模式: 带调试探测, 500ms一次防刷屏 */
    {
        if (g_ms < dbgNext) return;
        dbgNext = g_ms + 500;
        ret = RC522_CheckCardDbg(uid);
    }
    else
    {
        ret = RC522_CheckCard(uid);
    }

    if (ret == 0)
    {
        g_cardNextAllow = g_ms + 1000;
        Activity_Notify();                            /* 刷卡/加卡点亮OLED */

        if (g_cardLearnEnd && g_ms < g_cardLearnEnd)  /* 学习模式: 登记新卡 */
        {
            char msg[48];
            if (Storage_AddCard(uid))
                sprintf(msg, "card add ok: %02X%02X%02X%02X (%u张)",
                        uid[0], uid[1], uid[2], uid[3], Storage_CardCount());
            else
                sprintf(msg, "card exists or full");
            Cloud_Reply(msg);
            Beep_Buzz(2000, 100);
            g_cardLearnEnd = 0;
        }
        else if (Storage_CheckCard(uid))              /* 白名单卡 → 开锁 */
        {
            DoUnlock(LOG_TYPE_CARD, 0);
        }
        else                                          /* 未登记卡 */
        {
            Beep_Buzz(500, 300);
            Cloud_Reply("unknown card!");
        }
    }
}

/* MQTT 收包处理 */
void Cloud_Task(void)
{
    if (g_cloudOnline) MQTT_Process();
}

/* 心跳 / 掉线检测 / 重连 */
void Cloud_KeepAlive(void)
{
    if (g_cloudOnline)
    {
        /* 30秒一次MQTT心跳(保活, 防止被服务器踢掉) */
        if (g_ms - g_lastPing > 30000)
        {
            g_lastPing = g_ms;
            MQTT_Ping();
        }

        /* 断线事件立即重连; 每60秒做一次活性检测 */
        if (MQTT_LinkEvent())
        {
            Cloud_Reconnect();
        }
        else if (g_ms - g_lastRetry > 60000)
        {
            g_lastRetry = g_ms;
            if (MQTT_CheckAlive() != 0)
            {
                printf("ping no resp, link dead!\r\n");
                Cloud_Reconnect();
            }
        }
    }
    /* 离线状态: 每10秒尝试重连一次 */
    else if (g_ms - g_lastRetry > 10000)
    {
        g_lastRetry = g_ms;
        Cloud_Reconnect();
    }
}

/* 锁定倒计时管理 */
void Lockout_Task(void)
{
    static uint32_t lastTick = 0;

    if (IsLocked())
    {
        if (g_ms - lastTick >= 1000)                  /* 每秒滴答报警 */
        {
            lastTick = g_ms;
            Beep_Buzz(4000, 20);
        }
    }
    else if (g_lockoutEnd)                            /* 锁定刚结束 */
    {
        g_lockoutEnd = 0;
        g_failCount  = 0;
        UI_DrawBase();
        printf("lockout over\r\n");
    }
}

/* 界面定时恢复(指纹失败提示等) + 10秒无操作熄屏 */
void UI_Task(void)
{
    /* 通电后无任何操作满10秒 → 熄屏省电 */
    if (!g_oledSleep && g_ms - g_lastActive >= 10000)
    {
        g_oledSleep = 1;
        OLED_Off();
    }

    if (g_uiRestoreAt && g_ms >= g_uiRestoreAt)
    {
        g_uiRestoreAt = 0;
        if (!g_oledSleep)                 /* 熄屏中不写屏, 唤醒时统一重绘 */
        {
            UI_DrawBase();
            UI_ShowInput(0);
        }
    }
}

/*================= 主函数 =================*/
int main(void)
{
    uint8_t espStep;

    /*---- 硬件初始化 ----*/
    delay_ms(168);
    USART1_Config(115200);
    RTC_Config();                  /* 沿用已验证例程: 首次上电设时间, 之后纽扣电池走时 */
    RTC_TimeShow();                /* 开机串口显示一次日期时间 */
    USART2_Config(57600);          /* AS608 指纹 */
    USART3_Config(115200);         /* ESP8266 */
    LED_GPIO_Init();
    Beep_Init();
    Keypad_Init();
    Lock_Init();
    OLED_Init();
    RC522_Init();
    Storage_Init();                /* 内部含 W25QXX_Init */

    UI_DrawBase();
    OLED_ShowString(48, 6, (char *)STR_CONNECTING);

    /*---- 指纹模块 ----*/
    if (AS608_Init() == 0)
        printf("AS608 online, fingers=%u\r\n", AS608_GetNum());
    else
        printf("AS608 offline!\r\n");

    /*---- WiFi + MQTT ----*/
    espStep = ESP8266_Init();
    if (espStep == 0) Cloud_Connect();
    else printf("cloud offline (esp step %u)\r\n", espStep);
    UI_DrawBase();

    Beep_Buzz(2000, 100);
    printf("smart door lock ready\r\n");

    Scheduler_Init();

    /*================ 主循环: 时基累加 + 调度器 ================*/
    while (1)
    {
        delay_ms(2);
        g_ms += 2;
        Scheduler_Run();
    }
}
