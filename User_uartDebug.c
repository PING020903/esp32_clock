#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_wifi_types.h" // wifi类型定义
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_check.h"
#include "driver/uart.h"

#include "User_IO_init.h"
#include "User_NVSuse.h"
#include "User_WIFI.h"
#include "User_timer.h"
#include "User_SNTP.h"
#include "User_MQTT.h"
#include "cJSON.h"
#include "User_uartDebug.h"

#define COMMAND_LENGTH (128)
#define DEFAULT_UART_CH UART_NUM_0
#define UART_RX_BUF_SIZE (1024)

#define HELP_COMMAND "help"
#define RESTART_COMMAND "restart"
#define CHECK_COMMAND "check"
#define UPDATE_COMMAND "update"
#define ABORT_COMMAND "abort"
#define MQTT_COMMAND "mqtt"
#define SET_COMMAND "set"
#define MODE_COMMAND "mode"

#define COMMAND_SPACE ' '
#define COMMAND_EQUAL '='

#define STR_TIME "time"
#define STR_CLOCK "clock"
#define STR_BELL "bell"
#define STR_HEAP "heap"
#define STR_RESTART "restart"
#define STR_CHIP "chip"
#define STR_LED "led"
#define STR_NTP "ntp"
#define STR_WIFI "wifi"
#define STR_ALL "all"
#define STR_COUNTDOWN "countdown"
#define STR_INIT "init"
#define STR_DEINIT "deinit"
#define STR_SUBSCRIBE "subscribe"
#define STR_UNSUBSCRIBE "unsubscribe"
#define STR_PUBLISH "publish"
#define STR_CONNECT "connect"
#define STR_DISCONNECT "disconnect"
#define STR_TEST "test"
#define STR_CJSON "cjson"
#define STR_TASKLIST "tasklist"
#define STR_STOPWATCH "stopwatch"
#define STR_STATUS "status"
#define STR_LEDPERIOD "ledperiod"

#define STREND '\0'
#define TIME_SEPARATOR ':'
#define WIFI_SEPARATOR ','
#define STR_TIME_SEPARATOR ":"
#define STR_WIFI_SEPARATOR ","
#define STR_EQUAL "="

#define TIME_PARSE_DEBUG 0
#define WIFI_PRASE_DEBUG 0

static const char *TAG = "User_Debug";
static unsigned char command[COMMAND_LENGTH] = {0};

#define CheckTimeDescription "print current time"
#define CheckClockDescription "print target ring time and how long until the bell rings"
#define CheckHeapDescription "print current heap size"
#define CheckRestartDescription "last restart reason"
#define CheckChipDescription "print chip info"
#define CheckWifiDescription "print wifi connection status"

#define SetClockDescription "set target ring time"
#define SetWifiDescription "set wifi connect ssid, password and auth mode"
#define SetBellDescription "set bell task status"
#define SetLedDescription "set LED task status"

#define UpdateNtpDescription "update time from NTP server"

extern TaskHandle_t LED_handle;
extern TaskHandle_t Bell_handle;

enum
{
    COMMAND_NO_PARAMETERS = 0,
    COMMAND_CHECK,
    COMMAND_SET,
    COMMAND_UPDATE,
    COMMAND_ABORT,
    COMMAND_HELP,
    COMMAND_MQTT,
    COMMAND_MODE,
    COMMAND_UNKNOWN,
};

/// @brief cJSON打印函数
/// @param root
/// @return
static int print_preallocated(cJSON *root)
{
    /* declarations */
    char *out = NULL;
    char *buf = NULL;
    char *buf_fail = NULL;
    size_t len = 0;
    size_t len_fail = 0;

    /* formatted print */
    out = cJSON_Print(root);

    /* create buffer to succeed */
    /* the extra 5 bytes are because of inaccuracies when reserving memory */
    len = strlen(out) + 5;
    buf = (char *)malloc(len);
    if (buf == NULL)
    {
        printf("Failed to allocate memory.\n");
        exit(1);
    }

    /* create buffer to fail */
    len_fail = strlen(out);
    buf_fail = (char *)malloc(len_fail);
    if (buf_fail == NULL)
    {
        printf("Failed to allocate memory.\n");
        exit(1);
    }

    /* Print to buffer */
    if (cJSON_PrintPreallocated(root, buf, (int)len, 1) == false)
    {
        printf("cJSON_PrintPreallocated failed!\n");
        if (strcmp(out, buf) != 0)
        {
            printf("cJSON_PrintPreallocated not the same as cJSON_Print!\n");
            printf("cJSON_Print result:\n%s\n", out);
            printf("cJSON_PrintPreallocated result:\n%s\n", buf);
        }
        free(out);
        free(buf_fail);
        free(buf);
        return -1;
    }

    /* success */
    printf("%s\n", buf);

    /* force it to fail */
    if (cJSON_PrintPreallocated(root, buf_fail, (int)len_fail, 1))
    {
        printf("cJSON_PrintPreallocated failed to show error with insufficient memory!\n");
        printf("cJSON_Print result:\n%s\n", out);
        printf("cJSON_PrintPreallocated result:\n%s\n", buf_fail);
        free(out);
        free(buf_fail);
        free(buf);
        return -1;
    }

    free(out);
    free(buf_fail);
    free(buf);
    return 0;
}

/// @brief 获取时间字符串中数字部分
/// @param str 格式: "NUMhNUMmNUMs", exsample: "1h2m3s" "2m3s" "3s"
/// @return 时间值加起来的秒数
static unsigned int FindCountFromStrEnd(const char *str)
{
    char *pos = str;
    unsigned int ret = 0;
    for (; *pos != '\0'; pos++)
        ;

    /* 判断时间单位(s, m, h) */
    switch (*(--pos))
    {
    case 's':
    case 'S':
    {
        pos--;
        for (; *pos >= '0' && *pos <= '9'; pos--)
            ; // 此时pos指向到头一个数字的前一个字母
        ret = (unsigned int)atoi(pos + 1);
    }
    break;

    case 'm':
    case 'M':
    {
        pos--;
        for (; *pos >= '0' && *pos <= '9'; pos--)
            ;
        ret = ((unsigned int)atoi(pos + 1)) * 60;
    }
    break;

    case 'h':
    case 'H':
    {
        pos--;
        for (; *pos >= '0' && *pos <= '9'; pos--)
            ;
        ret = ((unsigned int)atoi(pos + 1)) * 60 * 60;
    }
    break;
    default:
        break;
    }

    /* 判断时间单位(m, h) */
    switch (*pos)
    {
    case 'm':
    case 'M':
    {
        pos--;
        for (; *pos >= '0' && *pos <= '9'; pos--)
            ;
        ret = (((unsigned int)atoi(pos + 1)) * 60) + ret;
    }
    break;

    case 'h':
    case 'H':
    {
        pos--;
        for (; *pos >= '0' && *pos <= '9'; pos--)
            ;
        ret = (((unsigned int)atoi(pos + 1)) * 60 * 60) + ret;
    }
    default:
        break;
    }

    /* 判断时间单位(h) */
    switch (*pos)
    {
    case 'h':
    case 'H':
    {
        pos--;
        for (; *pos >= '0' && *pos <= '9'; pos--)
            ;
        ret = (((unsigned int)atoi(pos + 1)) * 60 * 60) + ret;
    }
    break;
    default:
        break;
    }
    return ret;
}

/// @brief 倒计时解析
/// @param str 待解析的字符串
/// @return
static esp_err_t CountDownParse(const char *str, uint64_t *countDown)
{
    char *pos = NULL;

    pos = str + strlen(STR_COUNTDOWN);
    *countDown = FindCountFromStrEnd(pos) * 1000000ULL;
    if (*countDown <= 0)
        return ESP_FAIL;

    return ESP_OK;
}

/// @brief WIFI连接信息解析
/// @param str 待解析的字符串
/// @param connectSSID 存储SSID
/// @param connectPassword 存储连接密码
/// @param auth_mode 存储连接模式
/// @return OK:ESP_OK, ERROR:ESP_ERR_INVALID_ARG ( 字符串格式有误 ), ESP_FAIL ( 字符串信息有误 )
/// @note 命令格式："set+wifi=ssid:YOUR_SSID,password:YOUR_PASSWORD,auth:AUTH_TYPE_NUM"
static esp_err_t WifiStrParse(const char *str,
                              char *connectSSID,
                              char *connectPassword,
                              int *auth_type)
{
    const char *ssid = "ssid:";
    const char *password = "password:";
    const char *auth = "auth:";
    char *pos = NULL;
    char *pos2 = NULL;
    *auth_type = WIFI_AUTH_OPEN;

    /******* ssid * WIFI PARSE ********/
    pos = strstr(str, ssid);
    if (pos == NULL)
        return ESP_FAIL;
    if (*(pos - 1) != COMMAND_EQUAL)
        return ESP_ERR_INVALID_ARG;

    pos += strlen(ssid);
    pos2 = strstr(str, password);
    if (pos2 == NULL)
        return ESP_FAIL;
    if (*(pos2 - 1) != WIFI_SEPARATOR)
        return ESP_ERR_INVALID_ARG;

    pos2--; // 回退一位到','
    for (size_t i = 0; i < (pos2 - pos); i++)
    {
        *(connectSSID + i) = *(pos + i);
    }
#if WIFI_PRASE_DEBUG
    ESP_LOGI(__func__, "ssid:%s", connectSSID);
#endif

    /******* password * WIFI PARSE ********/
    pos = pos2 + 1;
    pos += strlen(password);
    pos2 = strstr(str, auth);
    if (pos2 == NULL)
        return ESP_FAIL;
    if (*(pos2 - 1) != WIFI_SEPARATOR)
        return ESP_ERR_INVALID_ARG;

    pos2--; // 回退一位到','
    for (size_t i = 0; i < (pos2 - pos); i++)
    {
        *(connectPassword + i) = *(pos + i);
    }
#if WIFI_PRASE_DEBUG
    ESP_LOGI(__func__, "password:%s", connectPassword);
#endif
    if (*connectPassword == '\0') // 密码为空
        return ESP_OK;

    /****** auth * WIFI PARSE ******/
    pos = strstr(str, auth);
    if (pos2 == NULL)
        return ESP_FAIL;

    pos += strlen(auth);
    *auth_type = atoi(pos);
#if WIFI_PRASE_DEBUG
    ESP_LOGI(__func__, "auth:%d", atoi(pos));
#endif

    return ESP_OK;
}

static void ChipInfoParse(const esp_chip_info_t chip_info)
{
    static char *chipInfoStrings[] = {
        "ESP32",
        "ESP32-S2",
        "ESP32-S3",
        "ESP32-C2",
        "ESP32-C3",
        "ESP32-C6",
        "ESP32-H2",
        "POSIX_LINUX",
        "unknown"};
    char *infoStr = NULL;
    switch (chip_info.model)
    {
    case CHIP_ESP32:
        infoStr = chipInfoStrings[0];
        break;
    case CHIP_ESP32S2:
        infoStr = chipInfoStrings[1];
        break;
    case CHIP_ESP32S3:
        infoStr = chipInfoStrings[2];
        break;
    case CHIP_ESP32C2:
        infoStr = chipInfoStrings[3];
        break;
    case CHIP_ESP32C3:
        infoStr = chipInfoStrings[4];
        break;
    case CHIP_ESP32C6:
        infoStr = chipInfoStrings[5];
        break;
    case CHIP_ESP32H2:
        infoStr = chipInfoStrings[6];
        break;
    case CHIP_POSIX_LINUX:
        infoStr = chipInfoStrings[7];
        break;
    default:
        infoStr = chipInfoStrings[8];
        break;
        ;
    }

    ESP_LOGI(TAG, "This is %s chip with %d CPU core(s), %s%s%s%s",
             infoStr,
             chip_info.cores,
             (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
             (chip_info.features & CHIP_FEATURE_BT) ? "BT/" : "",
             (chip_info.features & CHIP_FEATURE_BLE) ? "BLE/" : "",
             (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    ESP_LOGI(TAG, "silicon revision v%" PRId16 ".%d, ",
             chip_info.revision / 100, chip_info.revision % 100);

    uint32_t flash_size;
    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK)
    {
        printf("Get flash size failed");
        return;
    }

    ESP_LOGI(TAG, "%" PRIu32 "MB %s flash", flash_size / (uint32_t)(1024 * 1024),
             (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
}

static void RestartReasonParse(esp_reset_reason_t reason)
{
    switch (reason)
    {
    case ESP_RST_UNKNOWN:
        ESP_LOGW(TAG, "reset reason: unknow...");
        break;
    case ESP_RST_POWERON:
        ESP_LOGI(TAG, "reset reason: Reset due to power-on event...");
        break;
    case ESP_RST_EXT:
        ESP_LOGI(TAG, "reset reason: Reset by external pin (not applicable for ESP32)...");
        break;
    case ESP_RST_SW:
        ESP_LOGI(TAG, "reset reason: Software reset via esp_restart...");
        break;
    case ESP_RST_PANIC:
        ESP_LOGI(TAG, "reset reason: Software reset due to exception/panic...");
        break;
    case ESP_RST_INT_WDT:
        ESP_LOGI(TAG, "reset reason: Reset (software or hardware) due to interrupt watchdog...");
        break;
    case ESP_RST_TASK_WDT:
        ESP_LOGI(TAG, "reset reason: Reset due to task watchdog...");
        break;
    case ESP_RST_WDT:
        ESP_LOGI(TAG, "reset reason: Reset due to other watchdogs...");
        break;
    case ESP_RST_DEEPSLEEP:
        ESP_LOGI(TAG, "reset reason: Reset after exiting deep sleep mode...");
        break;
    case ESP_RST_BROWNOUT:
        ESP_LOGI(TAG, "reset reason: Brownout reset (software or hardware)...");
        break;
    case ESP_RST_SDIO:
        ESP_LOGI(TAG, "reset reason: Reset over SDIO...");
        break;
    default:
        ESP_LOGW(TAG, "reset reason: reset reason is not in list...");
        break;
    }
}

/// @brief bool状态解析
/// @param str
/// @return OK:true, FAIL:false
/// @note 仅限无符号数字&有符号数字
static bool BoolStateParse(const char *str)
{
    char *pos = strstr(str, STR_EQUAL);
    if (pos == NULL)
    {
        ESP_LOGW(__func__, "parse error");
    }
    size_t state = atoi(++pos);

    return (state) ? true : false;
}

/// @brief 时间字符串解析
/// @param str 含有时间格式的字符串
/// @param hour 要被赋值的参数
/// @param minute 要被赋值的参数
/// @param second 要被赋值的参数
/// @return OK:ESP_OK, ERROR: ESP_FAIL( 数字解析失败 )
/// @note 时间格式为 HH:MM:SS
static esp_err_t TimeParse(const char *str, int *hour, int *minute, int *second)
{
    char *pos = NULL;

    pos = strstr(str, STR_TIME_SEPARATOR); // 找第一个冒号
    if (pos == NULL)
    {
        *hour = ESP_FAIL;
        ESP_LOGE(TAG, "hour parse error");
        return ESP_FAIL;
    }
    else if (*(pos - 3) == COMMAND_EQUAL) // hours占2位的时候
    {
        *hour = atoi(pos - 2); // atoi()遇到非数字就返回
#if TIME_PARSE_DEBUG
        ESP_LOGI(__func__, "hours: %d", *hour);
#endif
    }
    else if (*(pos - 2) == COMMAND_EQUAL) // hours 占1位的时候
    {
        *hour = atoi(pos - 1);
#if TIME_PARSE_DEBUG
        ESP_LOGI(__func__, "hours: %d", *hour);
#endif
    }
    pos++;

    pos = strstr(pos, STR_TIME_SEPARATOR); // 找第二个冒号
    if (pos == NULL)
    {
        *minute = ESP_FAIL;
        ESP_LOGE(TAG, "minute parse error");
        return ESP_FAIL;
    }
    else if (*(pos - 3) == TIME_SEPARATOR) // minutes 占2位的时候
    {
        *minute = atoi(pos - 2); // atoi()遇到非数字就返回
#if TIME_PARSE_DEBUG
        ESP_LOGI(__func__, "minute: %d", *minute);
#endif
    }
    else if (*(pos - 2) == TIME_SEPARATOR) // minutes 占1位的时候
    {
        *minute = atoi(pos - 1);
#if TIME_PARSE_DEBUG
        ESP_LOGI(__func__, "minute: %d", *minute);
#endif
    }
    pos++;

    *second = atoi(pos); // 此时已经没有冒号
#if TIME_PARSE_DEBUG
    ESP_LOGI(__func__, "second: %d", *second);
#endif
    return ESP_OK;
}

/// @brief 字符解析器
/// @param str 字符串
/// @return
/// @note 将大写字符转小写, 其余直接返回
static char UserCharacterParse(const char *str)
{
    if ('A' <= *str && *str <= 'Z')
        return (*str) + 32;

    return (str)
               ? *str
               : '\0';
}

/// @brief 词解析器
/// @param str 待解析字符串
/// @param CMD 命令
/// @return
static char *UserWordParse(const char *str, const char *CMD)
{
    char cmd[12] = {0};
    const size_t len = strlen(CMD);
    size_t i = 0;

    for (; i < len; i++)
    {
        cmd[i] = UserCharacterParse(str + i);
    }

    return (strstr(cmd, CMD))
               ? str
               : NULL;
}

/// @brief 为pos, cmdLen赋值
/// @param cmdStr 经过检查后的源命令字符串
/// @param pos 要被赋值的
/// @param cmd 命令
/// @param cmdLen 命令长度
/// @return OK:1, ERROR:0
static int Assign(const char *cmdStr, char **pos, const char *cmd, size_t *cmdLen)
{
    if (cmdStr == NULL || cmd == NULL || cmdLen == NULL)
        return 0;

    *pos = cmdStr;
    *cmdLen = strlen(cmd);
    return 1;
}

/// @brief 命令解析
/// @param
/// @return ERROR: COMMAND_NO_PARAMETERS, COMMAND_UNKNOWN
static int CommandParse(void)
{
    char *pos = NULL;
    size_t len;
#define COMMAND_COMPARE(true_command, false_command) (*(pos + len) == COMMAND_SPACE && \
                                                      *(pos + len + 1) != STREND &&    \
                                                      (unsigned char *)pos == command) \
                                                         ? true_command                \
                                                         : false_command

    pos = UserWordParse((char *)command, RESTART_COMMAND);
    if ((unsigned char *)pos == command && *(pos + strlen(RESTART_COMMAND)) == STREND)
        esp_restart();

    // find the string from command, not found return null
    if (Assign(UserWordParse((char *)command, ABORT_COMMAND), &pos, ABORT_COMMAND, &len))
    {
        len = strlen(ABORT_COMMAND);
        return COMMAND_COMPARE(COMMAND_ABORT, COMMAND_NO_PARAMETERS);
    }
    else if (Assign(UserWordParse((char *)command, CHECK_COMMAND), &pos, CHECK_COMMAND, &len))
    {
        return COMMAND_COMPARE(COMMAND_CHECK, COMMAND_NO_PARAMETERS);
    }
    else if (Assign(UserWordParse((char *)command, HELP_COMMAND), &pos, HELP_COMMAND, &len))
    {
        return ((unsigned char *)pos == command && *(pos + len) == STREND)
                   ? COMMAND_HELP
                   : COMMAND_NO_PARAMETERS;
    }
    else if (Assign(UserWordParse((char *)command, MQTT_COMMAND), &pos, MQTT_COMMAND, &len))
    {
        return COMMAND_COMPARE(COMMAND_MQTT, COMMAND_NO_PARAMETERS);
    }
    else if (Assign(UserWordParse((char *)command, MODE_COMMAND), &pos, MODE_COMMAND, &len))
    {
        return COMMAND_COMPARE(COMMAND_MODE, COMMAND_NO_PARAMETERS);
    }
    else if (Assign(UserWordParse((char *)command, SET_COMMAND), &pos, SET_COMMAND, &len))
    {
        return COMMAND_COMPARE(COMMAND_SET, COMMAND_NO_PARAMETERS);
    }
    else if (Assign(UserWordParse((char *)command, UPDATE_COMMAND), &pos, UPDATE_COMMAND, &len))
    {
        return COMMAND_COMPARE(COMMAND_UPDATE, COMMAND_NO_PARAMETERS);
    }

    return COMMAND_UNKNOWN;
}

/// @brief 参数解析和命令执行
/// @param paramete 参数
/// @param StrEnd 参数结尾
/// @param func 需要执行的操作
/// @param arg func的参数( StrEnd 为 COMMAND_EQUAL 时,
// 给 NULL 传递 command paramete 后跟的字符串 )
/// @return OK: ESP_OK; ERROR: ESP_FAIL;
static esp_err_t ParameterCheckAndOperate(const char *paramete,
                                          const char StrEnd,
                                          void(func)(void *arg),
                                          void *arg)
{
    char *pos = strstr((char *)command, paramete);
    if (func == NULL)
    {
        ESP_LOGE(__func__, "No handler function");
        return ESP_FAIL;
    }

    if (pos && *(pos - 1) == ' ' && *(pos + strlen(paramete)) == StrEnd)
        goto CHECK_OK;
    else
        return ESP_FAIL;
CHECK_OK:

    switch (StrEnd)
    {
    case STREND:
        func(arg);
        break;
    case COMMAND_EQUAL:
        pos = pos + strlen(paramete);
        (arg == NULL)
            ? func((void *)pos)
            : func(arg);
        break;
    default:
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void CheckTime(void *arg)
{
    My_tm time;
    get_time_from_timer_v2(&time);
    ESP_LOGI(TAG, "local time: %d-%d-%d %d:%d:%d, day: %d",
             time.year, time.month, time.day,
             time.hour, time.minute, time.second,
             GetDayOfWeek(time.year, time.month, time.day));
}
static void CheckClock(void *arg)
{
    uint64_t time = User_clock();
    time /= 1000000ULL;
    ESP_LOGI(TAG, "target time: %d:%d:%d, remaining time: %lld:%lld:%lld",
             GetTargetTime(0), GetTargetTime(1), GetTargetTime(2),
             time / (MINUTE_OF_HOUR * SECOND_OF_MINUTE),
             (time % (MINUTE_OF_HOUR * SECOND_OF_MINUTE)) / SECOND_OF_MINUTE,
             time % SECOND_OF_MINUTE);
}
static void CheckHeap(void *arg)
{
    ESP_LOGI(TAG, "free heap: %" PRId32 ", minimum free heap size: %" PRIu32 " bytes",
             esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
}
static void CheakRestartReason(void *arg)
{
    RestartReasonParse(esp_reset_reason());
}
static void CheckChip(void *arg)
{
    esp_chip_info_t chipInfo;
    esp_chip_info(&chipInfo);
    ChipInfoParse(chipInfo);
}
static void CheckWifi(void *arg)
{
    Show_Wifi_connectionInfo_sta();
}
static void CheckTaskList(void *arg)
{
    const size_t task_cnt = uxTaskGetNumberOfTasks();
    const size_t AtaskInfoMaxByte = 40;
    /*
    SYStask: ipc0, ipc1, IDLE(0), IDLE(1), sys_evt,
      esp_timer, Tmr Svc, tiT, main
    USERtask: wifi, LED, bell, mqtt, UART-Debug_task,
      User_main
    */
    char *taskList = (char *)malloc(AtaskInfoMaxByte * task_cnt);
    vTaskList(taskList);
    printf("%s\n", taskList);
    free(taskList);
}
static void CheckStopwatch_(void *arg)
{
    CheckStopwatch();
}
static void CheckLed(void *arg)
{
    show_LED_status();
}
static void CheckStatus(void *arg)
{
    uint8_t hour, minute, second;
    My_tm time;
    uint64_t timeTemp = User_clock();
    timeTemp /= 1000000ULL;
    get_time_from_timer_v2(&time);
    ESP_LOGI(TAG, "free heap: %" PRId32 ", minimum free heap size: %" PRIu32 " bytes",
             esp_get_free_heap_size(), esp_get_minimum_free_heap_size());

    ESP_LOGI(TAG, "local time: %d-%d-%d %d:%d:%d, day: %d",
             time.year, time.month, time.day,
             time.hour, time.minute, time.second,
             GetDayOfWeek(time.year, time.month, time.day));

    ESP_LOGI(TAG, "target time: %d:%d:%d, remaining time: %lld:%lld:%lld",
             GetTargetTime(0), GetTargetTime(1), GetTargetTime(2),
             timeTemp / (MINUTE_OF_HOUR * SECOND_OF_MINUTE),
             (timeTemp % (MINUTE_OF_HOUR * SECOND_OF_MINUTE)) / SECOND_OF_MINUTE,
             timeTemp % SECOND_OF_MINUTE);
    CheckStopwatch();
    Show_Wifi_connectionInfo_sta();

    ReadPeriodicCloseLedTargetTime(&hour, &minute, &second);
    ESP_LOGI(TAG, "set periodic close LED target time: %u:%u:%u", hour, minute, second);
}
static void CheckPeriodicCloseLed(void *arg)
{
    uint8_t hour, minute, second;
    ReadPeriodicCloseLedTargetTime(&hour, &minute, &second);
    ESP_LOGI(TAG, "set periodic close LED target time: %u:%u:%u", hour, minute, second);
}
static void SetClock(void *arg)
{
    int hour, minute, second;
    char *pos = (char *)arg;
    uint64_t time;

    TimeParse(pos, &hour, &minute, &second);
    SetTargetTime(hour, minute, second);
    if (restartOfRunning_UserClockTimer())
        ESP_LOGW(TAG, "restartOfRunning_UserClockTimer failed");

    time = User_clock();
    time /= 1000000ULL;
    ESP_LOGI(TAG, "set target time: %d:%d:%d, remaining time: %lld:%lld:%lld",
             GetTargetTime(0), GetTargetTime(1), GetTargetTime(2),
             time / (MINUTE_OF_HOUR * SECOND_OF_MINUTE),
             (time % (MINUTE_OF_HOUR * SECOND_OF_MINUTE)) / SECOND_OF_MINUTE,
             time % SECOND_OF_MINUTE);
}
static void SetLed(void *arg)
{
    char *pos = (char *)arg;
    bool state;
    if (pos == NULL || Bell_handle == NULL)
        return;

    state = BoolStateParse(pos);
    (state) ? vTaskResume(LED_handle) : vTaskSuspend(LED_handle);
    User_LEDall_off();
    ESP_LOGI(TAG, "LED state: %d", state);
}
static void SetPeriodicCloseLed(void *arg)
{
    char *pos = (char *)arg;
    int hour, minute, second;
    esp_err_t ret;

    ret = TimeParse(pos, &hour, &minute, &second);
    if (ret != ESP_OK)
        ESP_LOGW(TAG, "TimeParse failed");
    ret = SetPeriodicCloseLedTargetTime((uint8_t)hour, (uint8_t)minute, (uint8_t)second);
    if (ret != ESP_OK)
        ESP_LOGW(TAG, "SetPeriodicCloseLedTargetTime failed err = %d", ret);

    ReadPeriodicCloseLedTargetTime((uint8_t *)&hour, (uint8_t *)&minute, (uint8_t *)&second);
    ESP_LOGI(TAG, "set periodic close LED target time: %d:%d:%d", hour, minute, second);
}
static void SetBell(void *arg)
{
    char *pos = (char *)arg;
    bool state;
    if (pos == NULL || Bell_handle == NULL)
        return;

    state = BoolStateParse(pos);
    (state) ? vTaskResume(Bell_handle) : vTaskSuspend(Bell_handle);
    set_user_bell(false);
    ESP_LOGI(TAG, "bell task state: %d", state);
}
static void SetWifi(void *arg)
{
    char *pos = (char *)arg;
    char ssid[CONNECT_SSID_LEN] = {0};
    char passwd[CONNECT_PASSWD_LEN] = {0};
    int auth = 0;

    WifiStrParse(pos, ssid, passwd, &auth);
    ESP_LOGI(TAG, "ssid: %s, passwd: %s, auth: %d", ssid, passwd, auth);
    SetConnectionInfo(ssid, passwd, auth);
    User_WIFI_changeConnection_sta();
}
static void SetCountDown(void *arg)
{
    My_tm time;
    char *pos = (char *)arg;
    CountDownParse(pos, SetCountdown());
    startCountdownTimer(GetCountdown());
    get_time_from_timer_v2(&time);
    uint64_t temp = GetCountdown() / 1000000ULL;
    ESP_LOGI(TAG, "countdown:%lld(h)%lld(m)%lld(s), NOW: %d:%d:%d",
             temp / 3600ULL, (temp % 3600ULL) / 60ULL, temp % 60ULL,
             time.hour, time.minute, time.second);
}
static void UpdateNtp(void *arg)
{
    uint32_t task_mark = SNTP_SYNC;
    xQueueSend(GetMainEventQueueHandle(), &task_mark, 0);
}
static void UserMqttInit(void *arg)
{
    if (GetMQTTinitReady() == false)
        mqtt_client_init();
    else
        ESP_LOGW(TAG, "MQTT client is initialized.");
}
static void UserMqttDeinit(void *arg)
{
    if (GetMQTTinitReady() == true)
        mqtt_client_deinit();
    else
        ESP_LOGW(TAG, "MQTT client is not initialized.");
}
static void UserMqttSubscribe(void *arg)
{
    My_tm Time;
    char *pos = (char *)arg;
    char ch = *(pos + 1);
    get_time_from_timer_v2(&Time);
    if (GetMQTTinitReady() == false)
    {
        ESP_LOGI(TAG, "MQTT client is not initialized.");
        return;
    }
    User_mqtt_client_subscribe(ch);
    ESP_LOGI(TAG, "%d-%d-%d %d:%d:%d    MQTT client is subscribed.",
             Time.year, Time.month, Time.day,
             Time.hour, Time.minute, Time.second);
}
static void UserMqttUnsubscribe(void *arg)
{
    My_tm Time;
    char *pos = (char *)arg;
    char ch = *(pos + 1);
    get_time_from_timer_v2(&Time);
    if (GetMQTTinitReady() == false)
    {
        ESP_LOGI(TAG, "MQTT client is not initialized.");
        return;
    }
    User_mqtt_client_unsubscribe(ch);
    ESP_LOGI(TAG, "%d-%d-%d %d:%d:%d    MQTT client is unsubscribed.",
             Time.year, Time.month, Time.day,
             Time.hour, Time.minute, Time.second);
}
static void UserMqttPublish(void *arg)
{
    My_tm Time;
    char *pos = (char *)arg;
    char ch = *(pos + 1);
    get_time_from_timer_v2(&Time);
    if (GetMQTTinitReady() == false)
    {
        ESP_LOGI(TAG, "MQTT client is not initialized.");
        return;
    }
    User_mqtt_client_publish(ch);
    ESP_LOGI(TAG, "%d-%d-%d %d:%d:%d    MQTT client has published.",
             Time.year, Time.month, Time.day,
             Time.hour, Time.minute, Time.second);
}
static void UserMqttConnect(void *arg)
{
    if (GetMQTTinitReady() == false)
    {
        ESP_LOGI(TAG, "MQTT client is not initialized.");
        return;
    }
    User_mqtt_client_connect();
}
static void UserMqttDisconnect(void *arg)
{
    if (GetMQTTinitReady() == false)
    {
        ESP_LOGI(TAG, "MQTT client is not initialized.");
        return;
    }
    User_mqtt_client_disconnect();
}
static void UserMqttTest(void *arg)
{
    char *pos = (char *)arg;
    char ch = *(pos + 1);
    switch (ch)
    {
    case '1':
        User_mqtt_test1();
        break;
    case '2':
        User_mqtt_test2();
        break;
    case '3':
        User_mqtt_test3();
        break;
    default:
        break;
    }
    return;
}
static void ModeClock(void *arg)
{
CLOCK_MODE:
    if (SetClockMode() == true)
        goto CLOCK_MODE;
    ESP_LOGI(TAG, "Now is clock Mode");
}
static void ModeStopwatch(void *arg)
{
STOPWATCH_MODE:
    if (SetClockMode() == false)
        goto STOPWATCH_MODE;
    ESP_LOGI(TAG, "Now is stopwatch Mode");
    SetStopwatch_record();
}

static esp_err_t ParametersParseOfCommand(void)
{
    char *pos = (char *)command;
    esp_err_t err = ESP_OK;
    const char *help_str = "You can input \"help\" to get help";
    int expression = CommandParse();

    switch (expression)
    {
    case COMMAND_NO_PARAMETERS:
        ESP_LOGW(TAG, "no parameters or invalid command... %s", help_str);
        break;
    case COMMAND_CHECK:
    {
        pos += strlen(CHECK_COMMAND) + 1;
        switch (*pos)
        {
        case 'c':
            err = ParameterCheckAndOperate(STR_CLOCK, STREND, CheckClock, NULL);
            err = ParameterCheckAndOperate(STR_CHIP, STREND, CheckChip, NULL);
            break;
        case 't':
            err = ParameterCheckAndOperate(STR_TIME, STREND, CheckTime, NULL);
            err = ParameterCheckAndOperate(STR_TASKLIST, STREND, CheckTaskList, NULL);
            break;
        case 'w':
            err = ParameterCheckAndOperate(STR_WIFI, STREND, CheckWifi, NULL);
            break;
        case 'r':
            err = ParameterCheckAndOperate(STR_RESTART, STREND, CheakRestartReason, NULL);
            break;
        case 'h':
            err = ParameterCheckAndOperate(STR_HEAP, STREND, CheckHeap, NULL);
            break;
        case 's':
            err = ParameterCheckAndOperate(STR_STOPWATCH, STREND, CheckStopwatch_, NULL);
            err = ParameterCheckAndOperate(STR_STATUS, STREND, CheckStatus, NULL);
            break;
        case 'l':
            err = ParameterCheckAndOperate(STR_LED, STREND, CheckLed, NULL);
            err = ParameterCheckAndOperate(STR_LEDPERIOD, STREND, CheckPeriodicCloseLed, NULL);
            break;
        default:
            ESP_LOGW(TAG, "%s%c( unknown parameter )", CHECK_COMMAND, COMMAND_SPACE);
            break;
        }
    }
    break;
    case COMMAND_SET:
    {

        pos += strlen(SET_COMMAND) + 1;
        switch (*pos)
        {
        case 'c':
            err = ParameterCheckAndOperate(STR_CLOCK, COMMAND_EQUAL,
                                           SetClock, NULL);
            err = ParameterCheckAndOperate(STR_COUNTDOWN, COMMAND_EQUAL,
                                           SetCountDown, NULL);
            break;
        case 'l':
            err = ParameterCheckAndOperate(STR_LED, COMMAND_EQUAL,
                                           SetLed, NULL);
            err = ParameterCheckAndOperate(STR_LEDPERIOD, COMMAND_EQUAL,
                                           SetPeriodicCloseLed, NULL);
            break;
        case 'b':
            err = ParameterCheckAndOperate(STR_BELL, COMMAND_EQUAL,
                                           SetBell, NULL);
            break;
        case 'w':
            err = ParameterCheckAndOperate(STR_WIFI, COMMAND_EQUAL,
                                           SetWifi, NULL);
            break;
        default:
            ESP_LOGW(TAG, "%s%c( unknown parameter )", SET_COMMAND, COMMAND_SPACE);
            break;
        }
    }
    break;
    case COMMAND_UPDATE:
    {
        pos += strlen(UPDATE_COMMAND) + 1;
        switch (*pos)
        {
        case 'n':
            err = ParameterCheckAndOperate(STR_NTP, STREND, UpdateNtp, NULL);
            break;
        default:
            ESP_LOGW(TAG, "%s%c( unknown parameter )", UPDATE_COMMAND, COMMAND_SPACE);
            break;
        }
    }
    break;
    case COMMAND_ABORT:
    {
        pos = (char *)command;
        esp_system_abort(pos + 1);
    }
    break;
    case COMMAND_HELP:
    {
#define PRINT_SPACES_COMMAND(COMMAND) \
    printf("command:\n        %s\n( %s )parameter:\n", COMMAND, COMMAND)

#define PRINT_SPACES_PARAMETER_CHECK(PARAMETER, EXPLANATION) \
    printf("        %s    %s\n", PARAMETER, EXPLANATION)

#define PRINT_SPACES_PARAMETER_SET(PARAMETER, FORMAT_DATA, EXPLANATION) \
    printf("        %s%c%s    %s\n", PARAMETER, COMMAND_EQUAL, FORMAT_DATA, EXPLANATION)

#define PRINT_CUT_UP_LINE(NUM)       \
    for (size_t i = 0; i < NUM; i++) \
        putchar('-');                \
    putchar('\n')

        PRINT_CUT_UP_LINE(18);
        PRINT_SPACES_COMMAND(CHECK_COMMAND);
        PRINT_SPACES_PARAMETER_CHECK(STR_TIME, CheckTimeDescription);
        PRINT_SPACES_PARAMETER_CHECK(STR_CLOCK, CheckClockDescription);
        PRINT_SPACES_PARAMETER_CHECK(STR_HEAP, CheckHeapDescription);
        PRINT_SPACES_PARAMETER_CHECK(STR_RESTART, CheckRestartDescription);
        PRINT_SPACES_PARAMETER_CHECK(STR_CHIP, CheckChipDescription);
        PRINT_SPACES_PARAMETER_CHECK(STR_WIFI, CheckWifiDescription);
        PRINT_CUT_UP_LINE(18);

        PRINT_SPACES_COMMAND(SET_COMMAND);
        PRINT_SPACES_PARAMETER_SET(STR_CLOCK, "hh:mm:ss", SetClockDescription);
        PRINT_SPACES_PARAMETER_SET(STR_LED, "NUM ( 0:off, other:on )", SetLedDescription);
        PRINT_SPACES_PARAMETER_SET(STR_BELL, "NUM ( 0:off, other:on )", SetBellDescription);
        PRINT_SPACES_PARAMETER_SET(STR_WIFI,
                                   "ssid:YOUR_SSID,password:YOUR_PASSWORD,auth:AUTH_TYPE_NUM",
                                   SetWifiDescription);
        printf("AUTH_TYPE_NUM:\n");
        printf("        0 = WIFI_AUTH_OPEN\n");
        printf("        1 = WIFI_AUTH_WEP\n");
        printf("        2 = WIFI_AUTH_WPA_PSK\n");
        printf("        3 = WIFI_AUTH_WPA2_PSK\n");
        printf("        6 = WIFI_AUTH_WPA3_PSK\n");
        PRINT_CUT_UP_LINE(18);

        PRINT_SPACES_COMMAND(UPDATE_COMMAND);
        PRINT_SPACES_PARAMETER_CHECK(STR_NTP, UpdateNtpDescription);
        PRINT_CUT_UP_LINE(18);

        PRINT_SPACES_COMMAND(ABORT_COMMAND);
        printf("        ( The parameters here are user-defined strings. )\n");
        PRINT_CUT_UP_LINE(18);
    }
    break;
    case COMMAND_MQTT:
    {
        My_tm time;
        get_time_from_timer_v2(&time);
        pos += strlen(MQTT_COMMAND) + 1;
        switch (*pos)
        {
        case 'i':
            err = ParameterCheckAndOperate(STR_INIT, STREND,
                                           UserMqttInit, NULL);
            break;
        case 'u':
            err = ParameterCheckAndOperate(STR_UNSUBSCRIBE,
                                           COMMAND_EQUAL,
                                           UserMqttUnsubscribe,
                                           NULL);
            break;
        case 'd':
            err = ParameterCheckAndOperate(STR_DEINIT, STREND,
                                           UserMqttDeinit, NULL);
            err = ParameterCheckAndOperate(STR_DISCONNECT, STREND,
                                           UserMqttDisconnect, NULL);
            break;
        case 's':
            err = ParameterCheckAndOperate(STR_SUBSCRIBE, COMMAND_EQUAL,
                                           UserMqttSubscribe, NULL);
            break;
        case 'p':
            err = ParameterCheckAndOperate(STR_PUBLISH, COMMAND_EQUAL,
                                           UserMqttPublish, NULL);
            break;
        case 'c':
            err = ParameterCheckAndOperate(STR_CONNECT, STREND,
                                           UserMqttConnect, NULL);
            break;
        case 't':
            err = ParameterCheckAndOperate(STR_TEST, COMMAND_EQUAL,
                                           UserMqttTest, NULL);

            break;
        default:
            ESP_LOGW(TAG, "%s%c( unknown parameter )", MQTT_COMMAND, COMMAND_SPACE);
            break;
        }
    }
    break;
    case COMMAND_MODE:
    {
        pos += strlen(MODE_COMMAND) + 1;
        switch (*pos)
        {
        case 'c': // clock
            err = ParameterCheckAndOperate(STR_CLOCK, STREND, ModeClock, NULL);
            break;
        case 's': // stopwatch
            err = ParameterCheckAndOperate(STR_STOPWATCH, STREND, ModeStopwatch, NULL);
            break;
        default:
            ESP_LOGI(TAG, "clock unknown mode");
            break;
        }
    }
    break;
    default:
        ESP_LOGW(TAG, "unknown command. %s", help_str);
        break;
    }

    return ESP_OK;
}

static void read_command(void *arg)
{

    esp_err_t ret = 0;
    while (1)
    {
        memset(command, 0, COMMAND_LENGTH);
        ret = uart_read_bytes(DEFAULT_UART_CH,
                              (void *)command,
                              COMMAND_LENGTH,
                              1000 / portTICK_PERIOD_MS);
#if 0
        if (ret)
        {
            // ESP_LOGI(TAG, "read strings: <%s>", command);
            ParametersParseOfCommand();
        }
        else
            vTaskDelay(1);
#endif
        (ret)
            ? ParametersParseOfCommand()
            : vTaskDelay(1);
    }
}

/// @brief 串口DEBUG初始化
/// @param
/// @return
esp_err_t uart_debug_init(void)
{
    esp_err_t err;
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    err = uart_driver_install(DEFAULT_UART_CH,
                              UART_RX_BUF_SIZE * 2,
                              0,
                              0,
                              NULL,
                              0);
    if (err)
        goto exit;

    err = uart_param_config(DEFAULT_UART_CH, &uart_config);
    if (err)
        goto exit;

    err = uart_set_pin(DEFAULT_UART_CH,
                       UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE);
    if (err)
        goto exit;

    xTaskCreatePinnedToCore(read_command, "UART-Debug_task", 4096, NULL, 6, NULL, 1);
    return ESP_OK;

exit:
    uart_driver_delete(DEFAULT_UART_CH);
    return err;
}
