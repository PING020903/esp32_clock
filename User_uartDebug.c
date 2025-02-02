#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <wchar.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_wifi_types.h" // wifi类型定义
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_check.h"
#include "esp_freertos_hooks.h"
#include "driver/uart.h"

#include "User_IO_init.h"
#include "User_NVSuse.h"
#include "User_WIFI.h"
#include "User_httpServer.h"
#include "User_timer.h"
#include "User_SNTP.h"
#include "User_MQTT.h"
#include "cJSON.h"
#include "User_main.h"
#include "User_taskManagement.h"
#include "CommandParse.h"
#include "User_uartDebug.h"

#define COMMAND_LENGTH COMMAND_SIZE
#define DEFAULT_UART_CH UART_NUM_0
#define UART_RX_BUF_SIZE (1024)
#define CMD_TASK_BUFFER_SIZE (3072)

#define HELP_COMMAND "help"
#define RESTART_COMMAND "restart"
#define CHECK_COMMAND "check"
#define UPDATA_COMMAND "updata"
#define ABORT_COMMAND "abort"
#define MQTT_COMMAND "mqtt"
#define SET_COMMAND "set"
#define MODE_COMMAND "mode"
#define TEST_COMMAND "test"

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
#define STR_WIFISCAN "wifiscan"
#define STR_WIFIAP "wifiap"
#define STR_DESC "desc"
#define STR_POWERTIME "powertime"
#define STR_HTTPSERVER "httpserver"
#define STR_CMDLIST "cmdlist"
#define STR_WATER "water"
#define STR_BELLDAYS "belldays"

#define STREND '\0'
#define LINE_END_CR "\r"
#define LINE_END_LF "\n"
#define LINE_END_CRLF "\r\n"
#define LINE_END_NONE ""
#define TIME_SEPARATOR ':'
#define WIFI_SEPARATOR ','
#define STR_TIME_SEPARATOR ":"
#define STR_WIFI_SEPARATOR ","
#define STR_EQUAL "="

#define TIME_PARSE_DEBUG 0
#define WIFI_PRASE_DEBUG 0

#define ENABLE_FUNCTION_TEST 1
#define ENABLE_FUNCTION_HELP 1
#define ENABLE_FUNCTION_CHECK 1
#define ENABLE_FUNCTION_SET 1
#define ENABLE_FUNCTION_MQTT 0
#define ENABLE_FUNCTION_MODE 1
#define ENABLE_FUNCTION_UPDATA 1

#define CHECK_BIT(var, pos) ((var & (1 << pos)) ? 1 : 0)

static const char *TAG = "User_Debug";

#define WIFI_STRING_FROMAT_DESCRIPTION "ssid:YOUR_SSID  \
password:YOUR_PASSWORD  \
auth:AUTH_TYPE_NUM"

#define CheckTimeDescription "(print current time)"
#define CheckClockDescription "(print target ring time and how long until the bell rings)"
#define CheckHeapDescription "(print current heap size)"
#define CheckRestartDescription "(last restart reason)"
#define CheckChipDescription "(print chip info)"
#define CheckWifiDescription "(print wifi connection status)"
#define CheckWifiScanDescription "(scan wifi ap)"

#define SetClockDescription "(set target ring time)"
#define SetWifiApDescription1 "(enable, set wifi ap ssid, password and auth mode)"
#define SetWifiApDescription2 "(disable wifi ap)"
#define SetWifiDescription "(set wifi connect ssid, password and auth mode)"
#define SetBellDescription "(set bell task status)"
#define SetLedDescription "(set LED task status)"

#define UpdataNtpDescription "(updata time from NTP server)"


#if 0
/// @brief 数学运算
/// @param str 待解析的字符串
/// @return
static int MathOperation(const char *str)
{
    return 0;
}

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
#endif

/// @brief 获取时间字符串中数字部分
/// @param str 格式: "NUMhNUMmNUMs", exsample: "1h2m3s" "2m3s" "3s"
/// @return 时间值加起来的秒数
static unsigned int FindCountFromStrEnd(const char *str)
{
    const char *pos = str;
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
    const char *pos = NULL;

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
/// @return OK: ESP_OK, ERROR: ESP_ERR_INVALID_ARG, ESP_FAIL
/// @note 命令格式: ssid:"YOUR_SSID",password:"YOUR_PASSWORD",auth:"AUTH_TYPE_NUM"
static esp_err_t WifiStrParse(const userString *data,
                              char *connectSSID,
                              char *connectPassword,
                              int *auth_type)
{
    const char *ssid = "ssid:";
    const char *password = "password:";
    const char *auth = "auth:";
    const char *pos = NULL;
    char tempStr[COMMAND_SIZE / 4] = {0};
    size_t len = 0;

    if (data == NULL || connectSSID == NULL || connectPassword == NULL || auth_type == NULL)
        return ESP_ERR_INVALID_ARG;

    *auth_type = WIFI_AUTH_OPEN;
    memset(connectSSID, 0, CONNECT_SSID_LEN);
    memset(connectPassword, 0, CONNECT_PASSWD_LEN);

    /******* ssid * WIFI PARSE ********/
    // 支持打乱参数顺序
    for (size_t i = 0; i < NodeGetUserParamsCnt(); i++)
    {
        memset(tempStr, 0, sizeof(tempStr));
        memcpy(tempStr, (data + i)->strHead, (data + i)->len);

        pos = strstr(tempStr, ssid);
        if (pos)
        {
            len = strlen(ssid);
            memset(connectSSID, 0, CONNECT_SSID_LEN);
            memcpy(connectSSID, pos + len, (data + i)->len - len);
            break;
        }
    }
#if WIFI_PRASE_DEBUG
    ESP_LOGI(__func__, "ssid:%s", connectSSID);
#endif

    /******* password * WIFI PARSE ********/
    for (size_t i = 0; i < NodeGetUserParamsCnt(); i++)
    {
        memset(tempStr, 0, sizeof(tempStr));
        memcpy(tempStr, (data + i)->strHead, (data + i)->len);

        pos = strstr(tempStr, password);
        if (pos)
        {
            len = strlen(password);
            memset(connectPassword, 0, CONNECT_PASSWD_LEN);
            if (((data + i)->len - len) < 8) // 密码少于8位, 跳过, 当作无密码
                break;
            memcpy(connectPassword, pos + len, (data + i)->len - len);
            break;
        }
    }
#if WIFI_PRASE_DEBUG
    ESP_LOGI(__func__, "password:%s", connectPassword);
#endif
    /****** auth * WIFI PARSE ******/
    for (size_t i = 0; i < NodeGetUserParamsCnt(); i++)
    {
        memset(tempStr, 0, sizeof(tempStr));
        memcpy(tempStr, (data + i)->strHead, (data + i)->len);

        pos = strstr(tempStr, auth);
        if (pos)
        {
            len = strlen(auth);
            *auth_type = atoi(pos + len);
            break;
        }
    }
    if (*connectPassword == '0') // 没有密码, 验证方式为 OPEN
        *auth_type = 0;
#if WIFI_PRASE_DEBUG
    ESP_LOGI(__func__, "auth:%d", *auth_type);
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
static bool BoolStateParse(const userString *data)
{
    char str[COMMAND_SIZE / 4] = {0};
    if (data == NULL)
        return false;
    memcpy(str, data->strHead, data->len);

    size_t state = atoi(str);
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
    // 找冒号, 数字字符不超过3位
#define FIND_TIME_SEPARATOR(POS, TagStr)           \
    do                                             \
    {                                              \
        for (size_t i = 0; i < 3; i++)             \
        {                                          \
            pos++;                                 \
            if (*(POS) == TIME_SEPARATOR)          \
                break;                             \
        }                                          \
        if (*(POS) != TIME_SEPARATOR)              \
        {                                          \
            ESP_LOGW(TagStr, "time format error"); \
            return ESP_FAIL;                       \
        }                                          \
    } while (0)

    const char *pos = str;
    if (pos == NULL)
        return ESP_FAIL;
#if TIME_PARSE_DEBUG
    ESP_LOGI(__func__, "str:%s", str);
#endif

    // 字符串的第一位不是数字, 数据格式错误
    if ((*pos) < '0' || (*pos) > '9')
    {
        ESP_LOGW(TAG, "time format error");
        return ESP_FAIL;
    }

    *hour = atoi(pos);
#if TIME_PARSE_DEBUG
    ESP_LOGW(__func__, "posStr = %s", pos);
    ESP_LOGI(__func__, "hours: %d", *hour);
#endif

    FIND_TIME_SEPARATOR(pos, "minutes");
    *minute = atoi(++pos);
#if TIME_PARSE_DEBUG
    ESP_LOGW(__func__, "posStr = %s", pos);
    ESP_LOGI(__func__, "minute: %d", *minute);
#endif

    FIND_TIME_SEPARATOR(pos, "seconds");
    *second = atoi(++pos);
#if TIME_PARSE_DEBUG
    ESP_LOGW(__func__, "posStr = %s", pos);
    ESP_LOGI(__func__, "second: %d", *second);
#endif
    return ESP_OK;
}

#if 0
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
#endif

#if ENABLE_FUNCTION_CHECK
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
    Show_Wifi_Info();
}
static void CheckWifisacn(void *arg)
{

    show_wifi_list();
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
    ESP_LOGI(TAG, "Tasks are reported as blocked ('B'), ready ('R'),\
 deleted ('D') or suspended ('S').");
    printf("taskName  state  (unknow)  block  priority  coreId \n");
    printf("%s\n", taskList);
    free(taskList);
}
static void CheckTaskStackHighWaterMark(void *arg)
{
    UBaseType_t waterMark;
#if (LED_DRIVER_VERSION_MAJOR == LED_TASK_VERSION)
    waterMark = uxTaskGetStackHighWaterMark(User_GetTaskHandle(LED_TASK_NAME));
    ESP_LOGI(TAG, "LED task stack high water mark: %u", waterMark);
#endif
#if (BELL_DRIVER_VERSION_MAJOR == BELL_TASK_VERSION)
    waterMark = uxTaskGetStackHighWaterMark(User_GetTaskHandle(BELL_TASK_NAME));
    ESP_LOGI(TAG, "Bell task stack high water mark: %u", waterMark);
#endif
    waterMark = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(TAG, "this task stack high water mark: %u", waterMark);
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
    short year, month, day, hour, minute, second;
    uint8_t P_hour, P_minute, P_second;
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

    if (Read_PowerTime(&year, &month, &day, &hour, &minute, &second) != ESP_OK)
    {
        ESP_LOGW(TAG, "Read_PowerTime failed");
    }
    else
    {
        ESP_LOGI(TAG, "power time: %d-%d-%d %d:%d:%d",
                 year, month, day, hour, minute, second);
    }

    CheckStopwatch();
    Show_Wifi_Info();

    ReadPeriodicCloseLedTargetTime(&P_hour, &P_minute, &P_second);
    ESP_LOGI(TAG, "set periodic close LED target time: %u:%u:%u",
             P_hour, P_minute, P_second);
    ESP_LOGI(__func__, "%p", CheckStatus);
}
static void CheckPeriodicCloseLed(void *arg)
{
    uint8_t hour, minute, second;
    ReadPeriodicCloseLedTargetTime(&hour, &minute, &second);
    ESP_LOGI(TAG, "set periodic close LED target time: %u:%u:%u", hour, minute, second);
}
static void CheckPowerTime(void *arg)
{
    My_tm time;
    short year, month, day, hour, minute, second;
    get_time_from_timer_v2(&time);
    ESP_LOGI(TAG, "local time: %d-%d-%d %d:%d:%d",
             time.year, time.month, time.day,
             time.hour, time.minute, time.second);

    if (Read_PowerTime(&year, &month, &day, &hour, &minute, &second) == ESP_OK)
        ESP_LOGI(TAG, "power time: %d-%d-%d %d:%d:%d",
                 year, month, day, hour, minute, second);
    else
        ESP_LOGW(TAG, "Read_PowerTime failed");
}
static void CheckBellDays(void *arg)
{
    uint8_t days = 0;

    Read_BellDays(&days);
    ESP_LOGI(TAG, "Bell days: %u", days);
    ESP_LOGI(TAG, "Sun:%u, Mon:%u, Tue:%u, Wed:%u, Thu:%u, Fri:%u, Sat:%u",
             CHECK_BIT(days, 0), CHECK_BIT(days, 1), CHECK_BIT(days, 2),
             CHECK_BIT(days, 3), CHECK_BIT(days, 4), CHECK_BIT(days, 5),
             CHECK_BIT(days, 6));
}
#endif
#if ENABLE_FUNCTION_SET
static void SetClock(void *arg)
{
    int hour, minute, second;
    const userString *data = (userString *)arg;
    uint64_t time;

    TimeParse(data->strHead, &hour, &minute, &second);
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
    const userString *data = (userString *)arg;
    bool state;

    if (data == NULL)
    {
        ESP_LOGW(TAG, "<%s> data is NULL", __func__);
        return;
    }

    state = BoolStateParse(data);
#if (LED_DRIVER_VERSION_MAJOR == LED_TASK_VERSION)
    if (User_GetTaskHandle(LED_TASK_NAME) == NULL)
        return;
    (state) 
    ? vTaskResume(User_GetTaskHandle(LED_TASK_NAME)) 
    : vTaskSuspend(User_GetTaskHandle(LED_TASK_NAME));
#else
    (state) ? esp_register_freertos_tick_hook_for_cpu(GetLedDriverTask(1), DEFAULT_LED_TASK_CORE)
            : esp_deregister_freertos_tick_hook_for_cpu(GetLedDriverTask(1), DEFAULT_LED_TASK_CORE);
#endif
    User_LEDall_off();
    ESP_LOGI(TAG, "LED state: %d", state);
}
static void SetPeriodicCloseLed(void *arg)
{
    const userString *data = (userString *)arg;
    int hour, minute, second;
    esp_err_t ret;

    ret = TimeParse(data->strHead, &hour, &minute, &second);
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
    const userString *data = (userString *)arg;
    bool state;
    if (data == NULL)
    {
        ESP_LOGW(TAG, "<%s> data is NULL", __func__);
        return;
    }

    state = BoolStateParse(data);
#if (BELL_DRIVER_VERSION_MAJOR == BELL_TASK_VERSION)
    if (User_GetTaskHandle(BELL_TASK_NAME) == NULL)
        return;
    (state)
        ? vTaskResume(User_GetTaskHandle(BELL_TASK_NAME))
        : vTaskSuspend(User_GetTaskHandle(BELL_TASK_NAME));
    set_user_bell(false);
#else
    // BellControlOFFON() 只有两种状态, 每使用一次, 状态翻转
    if (state != BellControlOFFON())
        BellControlOFFON();
#endif
    ESP_LOGI(TAG, "bell task state: %d", state);
}
static void SetWifi(void *arg)
{
    const userString *pos = (userString *)arg;
    char ssid[CONNECT_SSID_LEN] = {0};
    char passwd[CONNECT_PASSWD_LEN] = {0};
    int auth = 0;

    if (pos == NULL)
    {
        ESP_LOGW(__func__, "has no parameter");
        return;
    }

    if ((strcmp((char *)(pos->strHead), "disable") == 0) &&
        (NodeGetUserParamsCnt() == 1)) // 命令参数中仅有 "disable"
    {
        ESP_LOGI(TAG, "close station %s", (User_WIFI_disable_sta()) ? "failed" : "success");
    }
    else
    {
        WifiStrParse(pos, ssid, passwd, &auth);
        ESP_LOGI(TAG, "ssid: %s, passwd: %s, auth: %d", ssid, passwd, auth);
        (passwd[0] != '\0')
            ? SetConnectionInfo(ssid, passwd, auth)
            : SetConnectionInfo(ssid, NULL, auth);
        User_WIFI_changeConnection_sta();
    }
}
static void SetWifiAp(void *arg)
{
    const userString *pos = (userString *)arg;
    char ssid[CONNECT_SSID_LEN] = {0};
    char passwd[CONNECT_PASSWD_LEN] = {0};
    int auth = 0;
    esp_err_t err = ESP_OK;

    if (pos == NULL)
    {
        ESP_LOGW(__func__, "has no parameter");
        goto NO_PARAMS;
    }

    if (strcmp((char *)(pos->strHead), "disable") == 0) // 命令参数中仅有 "disable"
    {
        ESP_LOGI(TAG, "close AP %s", (User_WIFI_disable_AP()) ? "failed" : "success");
    }
    else
    {
        err = WifiStrParse(pos, ssid, passwd, &auth);
    NO_PARAMS:

        if (!NodeGetUserParamsCnt()) // 没有设定参数, 使用默认值
        {
            err = User_WIFI_enable_AP(NULL, NULL, 0);
        }
        else
        {
            // 密码为空, 传入NULL
            err = (passwd[0] != '\0')
                      ? User_WIFI_enable_AP(ssid, passwd, strlen(ssid))
                      : User_WIFI_enable_AP(ssid, NULL, strlen(ssid));
        }

        ESP_LOGI(TAG, "open AP %s", (err) ? "failed" : "success");
    }
    Show_Wifi_Info();
}
static void SetCountDown(void *arg)
{
    My_tm time;
    char *pos = (char *)arg;
    if (pos == NULL)
    {
        ESP_LOGW(__func__, "has no parameter");
        return;
    }
    CountDownParse(pos, SetCountdown());
    startCountdownTimer(GetCountdown());
    get_time_from_timer_v2(&time);
    uint64_t temp = GetCountdown() / 1000000ULL;
    ESP_LOGI(TAG, "countdown:%lld(h)%lld(m)%lld(s), NOW: %d:%d:%d",
             temp / 3600ULL, (temp % 3600ULL) / 60ULL, temp % 60ULL,
             time.hour, time.minute, time.second);
}
static bool httpServerTest = false;
static void SetHttpServer(void *arg)
{
    const userString *data = (userString *)arg;
    int ret = GetCurrentWifiMode();
    if (data == NULL)
        return;

    if (httpServerTest == true)
        goto WIFI_MODE_CHECK;
    if (ret < WIFI_MODE_STA)
    {
        ESP_LOGW(TAG, "WiFi is in an illegal state...");
        return;
    }
WIFI_MODE_CHECK:
    (BoolStateParse(data))
        ? WebServerConnect_handler()
        : WebServerDisconnect_handler();

    return;
}
static void setBellDays(void *arg)
{
    const userString *data = (userString *)arg;
    unsigned long long days = 0;
    if (data == NULL)
    {
        ESP_LOGW(TAG, "<%s>has no parameter", __func__);
        return;
    }

    days = strtoull(data->strHead, NULL, 2);
    ESP_LOGI(TAG, "set bell days: %llu, RET = %d", days, Write_BellDays((uint8_t)days));
}
#endif
#if ENABLE_FUNCTION_UPDATA
static void UpdataNtp(void *arg)
{
    uint32_t task_mark = SNTP_SYNC;
    if (GetMainEventQueueHandle() != NULL)
        xQueueSend(GetMainEventQueueHandle(), &task_mark, 0);
    else
        User_SNTP_update_time();
}
#endif
#if ENABLE_FUNCTION_MQTT
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
#endif
#if ENABLE_FUNCTION_MODE
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
#endif
#if ENABLE_FUNCTION_TEST
static void Test1(void *arg)
{
    httpServerTest = !httpServerTest;
    ESP_LOGI(TAG, "HTTP server test status: %s", (httpServerTest) ? "ON" : "OFF");
}
#endif
#if ENABLE_FUNCTION_HELP
/// @brief 命令行帮助信息
/// @param
static void HelpDescription(void *arg)
{
#define PRINT_SPACES_COMMAND(COMMAND) \
    printf("command:\n        %s\n( %s )parameter:\n", COMMAND, COMMAND)

#define PRINT_CHECK_PARAMETER(PARAMETER, EXPLANATION) \
    printf("        %s    %s\n", PARAMETER, EXPLANATION)

#define PRINT_SET_PARAMETER(PARAMETER, FORMAT_DATA, EXPLANATION) \
    printf("        %s%c%s    %s\n", PARAMETER, COMMAND_SPACE, FORMAT_DATA, EXPLANATION)

#define PRINT_CUT_UP_LINE(NUM)       \
    for (size_t i = 0; i < NUM; i++) \
        putchar('-');                \
    putchar('\n')

    putchar('\n');
    PRINT_CUT_UP_LINE(18);
    PRINT_SPACES_COMMAND(CHECK_COMMAND);
    PRINT_CHECK_PARAMETER(STR_TIME, CheckTimeDescription);
    PRINT_CHECK_PARAMETER(STR_CLOCK, CheckClockDescription);
    PRINT_CHECK_PARAMETER(STR_HEAP, CheckHeapDescription);
    PRINT_CHECK_PARAMETER(STR_RESTART, CheckRestartDescription);
    PRINT_CHECK_PARAMETER(STR_CHIP, CheckChipDescription);
    PRINT_CHECK_PARAMETER(STR_WIFI, CheckWifiDescription);
    PRINT_CHECK_PARAMETER(STR_WIFISCAN, CheckWifiScanDescription);
    PRINT_CUT_UP_LINE(18);

    PRINT_SPACES_COMMAND(SET_COMMAND);
    PRINT_SET_PARAMETER(STR_CLOCK, "hh:mm:ss", SetClockDescription);
    PRINT_SET_PARAMETER(STR_LED, "NUM ( 0:off, other:on )", SetLedDescription);
    PRINT_SET_PARAMETER(STR_BELL, "NUM ( 0:off, other:on )", SetBellDescription);
    PRINT_SET_PARAMETER(STR_WIFIAP,
                        WIFI_STRING_FROMAT_DESCRIPTION,
                        SetWifiApDescription1);
    PRINT_SET_PARAMETER(STR_WIFIAP,
                        "disable",
                        SetWifiApDescription2);
    PRINT_SET_PARAMETER(STR_WIFI,
                        WIFI_STRING_FROMAT_DESCRIPTION,
                        SetWifiDescription);
    printf("AUTH_TYPE_NUM:\n");
    printf("        0 = WIFI_AUTH_OPEN\n");
    printf("        1 = WIFI_AUTH_WEP\n");
    printf("        2 = WIFI_AUTH_WPA_PSK\n");
    printf("        3 = WIFI_AUTH_WPA2_PSK\n");
    // printf("        6 = WIFI_AUTH_WPA3_PSK\n");
    PRINT_CUT_UP_LINE(18);

    PRINT_SPACES_COMMAND(UPDATA_COMMAND);
    PRINT_CHECK_PARAMETER(STR_NTP, UpdataNtpDescription);
    PRINT_CUT_UP_LINE(18);

    PRINT_SPACES_COMMAND(ABORT_COMMAND);
    printf("        ( The parameters here are user-defined strings. )\n");
    PRINT_CUT_UP_LINE(18);
}

static void HelpCmdList(void *arg)
{
    showList();
}
#endif

static void read_command(void *arg)
{
    esp_err_t ret = 0;
    char command[COMMAND_LENGTH] = {0};

    while (1)
    {
        memset(command, 0, COMMAND_LENGTH);
        ret = uart_read_bytes(DEFAULT_UART_CH,
                              (void *)command,
                              COMMAND_LENGTH,
                              200 / portTICK_PERIOD_MS);
#if 0
        if (ret)
        {
            record_len += ret;
            memcpy(command + record_len - ret, tempBuffer, ret);
            // 找到command中的'\n'
            ch = memchr(command, '\n', COMMAND_LENGTH);
            if (ch)
            {
                for (int i = 0; i < record_len; i++)
                {
                    printf("%d ", command[i]);
                }
                record_len = 0;
                *(char *)ch = '\0';     // 清空了'\n'再解析
                *(char *)(--ch) = '\0'; // 清空了'\r'再解析
                CommandParse(command);
            }
        }
        else
            vTaskDelay(1);
#endif
#if 1
        (ret)
            ? CommandParse(command)
            : vTaskDelay(1);
#endif
    }
}

/// @brief 串口DEBUG初始化
/// @param
/// @return
esp_err_t uart_debug_init(void)
{
    esp_err_t err;
    command_node *node = NULL;
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_REF_TICK, // APB时钟源疑似唔稳定, 在安装RX后会出现乱码
    };
    // uart_driver_delete(DEFAULT_UART_CH);
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

    defaultRegCmd_init();

#if ENABLE_FUNCTION_MODE
    RegisterCommand(false, MODE_COMMAND);
    node = FindCommand(MODE_COMMAND, NULL);
    RegisterParameter(node, ModeClock, true, STR_CLOCK);
    RegisterParameter(node, ModeStopwatch, true, STR_STOPWATCH);
#endif
#if ENABLE_FUNCTION_HELP
    RegisterCommand(false, HELP_COMMAND);
    node = FindCommand(HELP_COMMAND, NULL);
    RegisterParameter(node, HelpDescription, false, STR_DESC);
    RegisterParameter(node, HelpCmdList, false, STR_CMDLIST);
#endif
#if ENABLE_FUNCTION_CHECK
    RegisterCommand(false, CHECK_COMMAND);
    node = FindCommand(CHECK_COMMAND, NULL);
    RegisterParameter(node, CheckTime, true, STR_TIME);
    RegisterParameter(node, CheckClock, true, STR_CLOCK);
    RegisterParameter(node, CheckHeap, true, STR_HEAP);
    RegisterParameter(node, CheckChip, true, STR_CHIP);
    RegisterParameter(node, CheckWifi, true, STR_WIFI);
    RegisterParameter(node, CheckWifisacn, true, STR_WIFISCAN);
    RegisterParameter(node, CheakRestartReason, true, STR_RESTART);
    RegisterParameter(node, CheckStopwatch_, true, STR_STOPWATCH);
    RegisterParameter(node, CheckStatus, true, STR_STATUS);
    RegisterParameter(node, CheckPowerTime, true, STR_POWERTIME);
    RegisterParameter(node, CheckTaskList, true, STR_TASKLIST);
    RegisterParameter(node, CheckPeriodicCloseLed, true, STR_LEDPERIOD);
    RegisterParameter(node, CheckLed, true, STR_LED);
    RegisterParameter(node, CheckTaskStackHighWaterMark, false, STR_WATER);
    RegisterParameter(node, CheckBellDays, false, STR_BELLDAYS);
#endif
#if ENABLE_FUNCTION_SET
    RegisterCommand(false, SET_COMMAND);
    node = FindCommand(SET_COMMAND, NULL);
    RegisterParameter(node, SetLed, false, STR_LED);
    RegisterParameter(node, SetClock, false, STR_CLOCK);
    RegisterParameter(node, SetBell, false, STR_BELL);
    RegisterParameter(node, SetWifi, false, STR_WIFI);
    RegisterParameter(node, SetWifiAp, false, STR_WIFIAP);
    RegisterParameter(node, SetCountDown, true, STR_COUNTDOWN);
    RegisterParameter(node, SetPeriodicCloseLed, false, STR_LEDPERIOD);
    RegisterParameter(node, SetHttpServer, false, STR_HTTPSERVER);
    RegisterParameter(node, setBellDays, false, STR_BELLDAYS);
#endif
#if ENABLE_FUNCTION_MQTT
    RegisterCommand(false, MQTT_COMMAND);
    node = FindCommand(MQTT_COMMAND, NULL);
    RegisterParameter(node, UserMqttInit, true, STR_INIT);
    RegisterParameter(node, UserMqttConnect, true, STR_CONNECT);
    RegisterParameter(node, UserMqttDisconnect, true, STR_DISCONNECT);
    RegisterParameter(node, UserMqttPublish, true, STR_PUBLISH);
    RegisterParameter(node, UserMqttSubscribe, true, STR_SUBSCRIBE);
    RegisterParameter(node, UserMqttUnsubscribe, true, STR_UNSUBSCRIBE);
    RegisterParameter(node, UserMqttDeinit, true, STR_DEINIT);
    RegisterParameter(node, UserMqttTest, true, STR_TEST);
#endif
#if ENABLE_FUNCTION_UPDATA
    RegisterCommand(false, UPDATA_COMMAND);
    node = FindCommand(UPDATA_COMMAND, NULL);
    RegisterParameter(node, UpdataNtp, true, STR_NTP);
#endif
#if ENABLE_FUNCTION_TEST
    RegisterCommand(false, TEST_COMMAND);
    node = FindCommand(TEST_COMMAND, NULL);
    RegisterParameter(node, Test1, true, STR_HTTPSERVER);
#endif
    xTaskCreatePinnedToCore(read_command,
                            DEBUG_TASK_NAME,
                            CMD_TASK_BUFFER_SIZE,
                            NULL,
                            6,
                            User_NewTask(),
                            1);
    return ESP_OK;

exit:
    uart_driver_delete(DEFAULT_UART_CH);
    return err;
}
