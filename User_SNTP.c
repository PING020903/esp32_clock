#include "stdio.h"
#include "string.h"
#include "time.h"
#include "esp_system.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "nvs_flash.h" // 从NVS中读取时间戳要用, 保存目标铃响时间要用
#include "esp_sntp.h"
#include "esp_err.h"

#include "cJSON.h"
#include "User_timer.h"
#include "User_WIFI.h"
#include "User_NVSuse.h"
#include "User_httpServer.h"
#include "User_main.h"
#include "User_SNTP.h"

#define DEFAULT_NAMESPACE "storage"
#define STAMP_KEY "time_stamp"

#define SNTP_INIT_PRINT 0
#define NTP_DEBUG_LOG 0
#define PRINT_RESTART_RESON 0

static const char *TAG = "User_SNTP";
static int err_temp[2] = {0};
static My_tm clock_tm = {0};

static uint8_t flag_show = true;
#if (LED_DRIVER_VERSION_MAJOR != 0)
ISR_SAFE
#endif
static int NtpReady = false;
static uint8_t PowerOnce = true;

IRAM_ATTR static void convertTimestamp(time_t timestamp, My_tm *my_tm)
{
#define DATE_DEBUG 0
    /*
        1970 年 1 月 1 日 00:00:00
        1972 年系闰年
    */
#define secondsPerMinute 60LL               // 一分钟的秒数
#define secondsPerHour 3600LL               // 一小时的秒数
#define secondsPerDay 86400LL               // 一天的秒数
#define YearDays 365LL                      // 平年天数
#define LeapYearDays (YearDays + 1LL)       // 闰年天数
#define fourYearDays (YearDays * 4LL + 1LL) // 四年天数
#define LeapMonthDays 29LL                  // 闰月天数
#define FebMonthDays 28LL                   // 一般二月月天数
#define BigMonthDays 31LL                   // 大月天数
#define SmallMonthDays 30LL                 // 小月天数

    // 调整为 UTC+8 时区
    timestamp += 8 * secondsPerHour;

    long long temp = timestamp / secondsPerDay;        // 总天数
    long long temp_second = timestamp % secondsPerDay; // 当天的时间秒数
#if DATE_DEBUG
    long long cnt = 0;
    printf("&& days = %lld\n", temp);
#endif
    my_tm->hour = temp_second / secondsPerHour;
    my_tm->minute = temp_second % secondsPerHour / secondsPerMinute;
    my_tm->second = temp_second % secondsPerHour % secondsPerMinute;
#if DATE_DEBUG
    printf("&& hour = %d, minute = %d, second = %d\n", my_tm->hour, my_tm->minute, my_tm->second);
#endif
    if (timestamp == 0)
    {
        my_tm->year = 1970;
        my_tm->month = 1;
        my_tm->day = 1;
        return;
    }

    // 总天数减去2年天数再开始算
    temp = temp - (YearDays * 2); // 除去1970 1971这两个年份, 从1972年开始算
    long long a = temp / fourYearDays;
#if DATE_DEBUG
    printf("&& years = %lld, +%lld days\n", (a * 4) + 1970 + 2, temp % fourYearDays);
#endif
    my_tm->year = (a * 4) + 1970 + 2;
    temp = temp % fourYearDays;

    my_tm->month = 1;
    my_tm->day = 1;
    if (temp < 366)
    {
        // 这里每四年, 以闰年为首, 然后到三年平年, 然后到闰年这样
        // 这里算闰年的月份
        // temp = temp % fourYearDays;
#if DATE_DEBUG
        printf("&& days = %lld\n", temp);
#endif
        while (1)
        {
            switch (my_tm->month)
            {
            case 4:
            case 6:
            case 9:
            case 11:
            {
                temp -= SmallMonthDays; // 小月
                if (temp < 0)
                {
#if DATE_DEBUG
                    printf("cnt = %lld, month = %d, |4 6 9 11|\n", ++cnt, my_tm->month);
#endif
                    temp += SmallMonthDays;
                    goto out1;
                }
                my_tm->month++;
            }
            break;
            case 2:
            {
                temp -= LeapMonthDays; // 二月
                if (temp < 0)
                {
#if DATE_DEBUG
                    printf("cnt = %lld, month = %d, |2|\n", ++cnt, my_tm->month);
#endif
                    temp += LeapMonthDays;
                    goto out1;
                }
                my_tm->month++;
            }
            break;
            default:
            {
                temp -= BigMonthDays; // 大月
                if (temp < 0)
                {
#if DATE_DEBUG
                    printf("cnt = %lld, month = %d, |1 3 5 7 8 10|\n", ++cnt, my_tm->month);
#endif
                    temp += BigMonthDays;
                    goto out1;
                }
                my_tm->month++;
            }
            break;
            }
        }
    out1:
        my_tm->day += temp;
    }
    else
    {
        long long tmp = temp / YearDays;
        my_tm->year += tmp;   // 其余情况加上年数
        temp -= LeapYearDays; // 先减去闰年
#if DATE_DEBUG
        printf("&& days = %lld, years = %d, tmp = %lld\n", temp, my_tm->year, tmp);
#endif
        temp -= ((tmp - 1) * YearDays); // 减去其余的平年
#if DATE_DEBUG
        printf("&& P_year days = %lld\n", temp);
#endif
        while (1)
        {
            switch (my_tm->month)
            {
            case 4:
            case 6:
            case 9:
            case 11:
            {
                temp -= SmallMonthDays; // 小月
                if (temp < 0)
                {
#if DATE_DEBUG
                    printf("cnt = %lld, month = %d, |4 6 9 11|\n", ++cnt, my_tm->month);
#endif
                    temp += SmallMonthDays;
                    goto out2;
                }
                my_tm->month++;
            }
            break;
            case 2:
            {
                temp -= FebMonthDays; // 二月
                if (temp < 0)
                {
#if DATE_DEBUG
                    printf("cnt = %lld, month = %d, |2|\n", ++cnt, my_tm->month);
#endif
                    temp += FebMonthDays;
                    goto out2;
                }
                my_tm->month++;
            }
            break;
            default:
            {
                temp -= BigMonthDays; // 大月
                if (temp < 0)
                {
#if DATE_DEBUG
                    printf("cnt = %lld, month = %d, |1 3 5 7 8 10|\n", ++cnt, my_tm->month);
#endif
                    temp += BigMonthDays;
                    goto out2;
                }
                my_tm->month++;
            }
            break;
            }
        }
    out2:
        my_tm->day += temp;
    }
#if DATE_DEBUG
    printf("&& month = %d, day = %d\n", my_tm->month, my_tm->day);
#endif
    return;
}

/// @brief 年月日转换星期
/// @param year 年
/// @param month 月
/// @param day 日
/// @return 0: sunday, 1: monday, 2: tuesday,
// 3: wednesday, 4: thursday, 5: friday, 6: saturday
int GetDayOfWeek(int year, int month, int day)
{
    // 基姆拉尔森计算公式
    int y = year;
    int m = month;
    int d = day;
    if (m == 1 || m == 2)
    {
        m += 12;
        y--;
    }
    int week = (d + 2 * m + 3 * (m + 1) / 5 + y + y / 4 - y / 100 + y / 400 + 1) % 7;
    return week;
}

/// @brief 获取本地时间戳
/// @param
/// @return
time_t GetTimestamp(void)
{
    return GetUserTimestamp();
}

/**
 * @brief  获取本地timer时间戳并转换年月日
 * @note    timestamp from GP-tiemr
 * @return time_t   User时间戳
 *
 */
ISR_SAFE void get_time_from_timer_v2(My_tm *my_tm)
{
    time_t now = GetUserTimestampISR();
    convertTimestamp(now, my_tm);
}

/**
 * @brief  计算到下一个铃响时间的剩余时间(unit: us)
 * @note    用于设置下一次铃响定时器的值(不考虑闰秒, 不考虑超过24h的铃响)
 * @return uint64_t
 *
 */
uint64_t User_clock(void)
{
    int temp_hour, temp_minute, temp_second;
    int target_hour, target_minute, target_second;
    if (updateTargetClockTimeFromNVS()) // 获取铃响目标时间, 非零时, 获取失败, 获取默认值
    {
        SetDefaultTargetClockInfo();
    }

    convertTimestamp(GetUserTimestamp(), &clock_tm);
    target_hour = GetTargetTime(0);
    target_minute = GetTargetTime(1);
    target_second = GetTargetTime(2);
    if (clock_tm.hour > target_hour)
    {
        temp_hour = (DAY_HOUR - 0) - (clock_tm.hour - target_hour); //   24-(now-target), 没有满整24h的可能
    }
    else if (clock_tm.hour <= target_hour)
    {
        temp_hour = target_hour - clock_tm.hour; //   target-now
    }

    if (clock_tm.minute > target_minute)
    {
        temp_minute = (MINUTE_OF_HOUR - 0) - (clock_tm.minute - target_minute); // 没有满整60min的可能
        temp_hour--;
    }
    else if (clock_tm.minute <= target_minute)
    {
        temp_minute = target_minute - clock_tm.minute;
    }

    if (clock_tm.second > target_second)
    {
        temp_second = (SECOND_OF_MINUTE - 0) - (clock_tm.second - target_second);
        temp_minute--;
    }
    else if (clock_tm.second <= target_second)
    {
        temp_second = target_second - clock_tm.second;
    }

    if (temp_minute < 0) // 出现了负数, 补偿修正真实时间
    {
        // 如果唔修正, 过了1min后这个bug才好(因为此时temp_min不为负数)
        temp_minute = MINUTE_OF_HOUR - 1;
        temp_hour = DAY_HOUR - 1;
    }
    if (temp_hour < 0)
        temp_hour = DAY_HOUR - 1;

    if (flag_show)
    {
        ESP_LOGW(TAG, "remaining_hours: %d", temp_hour);
        ESP_LOGW(TAG, "remaining_min: %d", temp_minute);
        ESP_LOGW(TAG, "remaining_sec: %d", temp_second);
        flag_show = false;
    }

    uint64_t ret = (temp_hour * MINUTE_OF_HOUR * SECOND_OF_MINUTE) +
                   (temp_minute * SECOND_OF_MINUTE) +
                   temp_second;
    ret *= 1000000ULL;
    return ret;
}

#if SNTP_INIT_PRINT
/**
 * @brief  打印SNTP服务器列表
 * @note
 * @return
 *
 */
static void print_servers(void)
{

    ESP_LOGI(TAG, "List of configured NTP servers:");

    for (uint8_t i = 0; i < SNTP_MAX_SERVERS; ++i)
    {
        if (esp_sntp_getservername(i))
        {
            ESP_LOGI(TAG, "server %d: %s", i, esp_sntp_getservername(i));
        }
        else
        {
            // we have either IPv4 or IPv6 address, let's print it
            char buff[INET6_ADDRSTRLEN];
            ip_addr_t const *ip = esp_sntp_getserver(i);
            if (ipaddr_ntoa_r(ip, buff, INET6_ADDRSTRLEN) != NULL)
                ESP_LOGI(TAG, "server %d: %s", i, buff);
        }
    }
}
#endif

/**
 * @brief  SNTP时间同步回调函数
 * @note
 * @return
 *
 */
static void time_sync_notification_cb(struct timeval *tv)
{
    NtpReady = true;
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

/**
 * @brief  初始化SNTP
 * @note
 * @return
 *
 */
static esp_err_t init_sntp()
{
#if SNTP_INIT_PRINT
    ESP_LOGI(TAG, "initializing SNTP");
#endif
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(4,
                                                                      ESP_SNTP_SERVER_LIST("ntp1.aliyun.com",
                                                                                           "ntp.tencent.com",
                                                                                           "ntp.ntsc.ac.cn",
                                                                                           "cn.ntp.org.cn"));
    config.sync_cb = time_sync_notification_cb; // Note: This is only needed if we want
    esp_err_t err = esp_netif_sntp_init(&config);
    if (err)
        ESP_LOGW(TAG, "esp_netif_sntp_init failed");
#if SNTP_INIT_PRINT
    print_servers();
#endif
    return err;
}

/**
 * @brief  SNTP获取时间
 * @note
 * @return
 *
 */
static esp_err_t obtain_time(void)
{
    // wait for time to be set
    int retry = 0;
    int sync_ret;

    const int retry_count = 60;
    while (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(1000)) != ESP_OK &&
           retry++ < retry_count)
    {
        if (retry % 20 == 0)
        {
            sync_ret = sntp_get_sync_status();
            switch (sync_ret)
            {
            case SNTP_SYNC_STATUS_RESET:
                ESP_LOGW(TAG, "status: SNTP reset...");
                break;
            case SNTP_SYNC_STATUS_IN_PROGRESS:
                ESP_LOGW(TAG, "status: SNTP in progress...");
                break;
            case SNTP_SYNC_STATUS_COMPLETED:
                ESP_LOGW(TAG, "status: SNTP completed ! ! !");
                return ESP_OK;

            default:
                break;
            }
            ESP_LOGI(TAG, "time sync waitting...(%d (unit:second)/%d (unit:second))",
                     retry, retry_count);
        }
    }

    sync_ret = sntp_get_sync_status();
    return (sync_ret == SNTP_SYNC_STATUS_COMPLETED)
               ? ESP_OK
               : ESP_FAIL;
}

/**
 * @brief  获取时间并写入NVS
 * @note
 * @return esp_err_t
 *
 */
esp_err_t fetch_and_store_time_in_nvs(void *args)
{
    nvs_handle_t my_handle = 0;
    esp_err_t err;

    if (init_sntp())
        return ESP_FAIL;
    if (obtain_time() != ESP_OK)
    {
        err = ESP_FAIL;
        err_temp[1] = 4;
        goto exit_1;
    }

    time_t now;
    time(&now);

    // open
    err = nvs_open(DEFAULT_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        err_temp[1] = 1;
        goto exit_1;
    }

    // write
    err = nvs_set_i64(my_handle, STAMP_KEY, now); // 写入时间戳
    if (err != ESP_OK)
    {
        err_temp[1] = 2;
        goto exit_1;
    }

    err = nvs_commit(my_handle);
    if (err != ESP_OK)
    {
        err_temp[1] = 3;
        goto exit_1;
    }

exit_1:
    if (my_handle != 0)
        nvs_close(my_handle);

    esp_netif_sntp_deinit();

    if (err != ESP_OK)
    {
        /* 此处打印LOG不能带有参数, 一旦带参数就编译错误了 */
        ESP_LOGE(TAG, "error updating time in NVS");
        err_temp[0] = err;
    }
    else
        ESP_LOGI(TAG, "time updated in NVS");

    return err;
}

/*  */
/**
 * @brief  从sntp获取时间并更新NVS时间
 * @note
 * @return esp_err_t
 *
 */
esp_err_t update_time_from_sntp(void)
{
    nvs_handle_t my_handle = 0; // NVS句柄
    esp_err_t err;

    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "error opening NVS");
        goto exit_2;
    }

    int64_t time_stamp = 0;
#if GET_TIME_FROM_NVS
    err = nvs_get_i64(my_handle, STAMP_KEY, &time_stamp); // 获取时间戳
    if (err == ESP_ERR_NVS_NOT_FOUND)                     // 这个选项一般情况是不会进入的
    {
        ESP_LOGI(TAG, "time out found in NVS. syncing time from SNTP server.");
        if (fetch_and_store_time_in_nvs(NULL) == ESP_OK)
            err = ESP_FAIL;
        else
            err = ESP_OK;
    }
    else if (err == ESP_OK)
    {
        struct timeval get_nvs_time;
        get_nvs_time.tv_sec = time_stamp;
        settimeofday(&get_nvs_time, NULL);
        ESP_LOGI(TAG, "time: %lld", time_stamp);
    }
#endif
#if GET_TIME_FROM_SNTP
    err = (fetch_and_store_time_in_nvs(NULL) == ESP_OK)
              ? ESP_OK
              : ESP_FAIL;

    if (err)
    {
        ESP_LOGE(TAG, "error syncing time from SNTP server");
        goto exit_2;
    }
    else
    {
        err = nvs_get_i64(my_handle, STAMP_KEY, &time_stamp); // 读取时间戳in nvs
        if (err >= ESP_ERR_NVS_BASE)
        {
            ESP_LOGE(TAG, "error getting time from NVS");
            goto exit_2;
        }
        struct timeval get_nvs_time;
        get_nvs_time.tv_sec = time_stamp; // 单位: sec

        // 将时区设置为中国
        setenv("TZ", "CST-8", 1);
        tzset();

        settimeofday(&get_nvs_time, NULL); // 设置系统时间(超过35min的时候先会马上刷新时间)
        adjtime(&get_nvs_time, NULL);      // 平滑更新时间
        Set_User_timestamp(time_stamp);

#if NTP_DEBUG_LOG
        convertTimestamp(time_stamp, &clock_tm);
        ESP_LOGI(TAG, "time: %lld", time_stamp);
        ESP_LOGI(TAG, "%d-%d-%d  %d:%d:%d",
                 clock_tm.year, clock_tm.month, clock_tm.day,
                 clock_tm.hour, clock_tm.minute, clock_tm.second);
#endif

        time_t clock_stamp = User_clock() / 1000000;
        My_tm temp_tm;
        convertTimestamp(time_stamp + clock_stamp, &temp_tm);
        ESP_LOGI(TAG, "CLOCK will bells time: %d:%d:%d %d-%d-%d, day:%d",
                 temp_tm.hour, temp_tm.minute, temp_tm.second,
                 temp_tm.year, temp_tm.month, temp_tm.day,
                 GetDayOfWeek(temp_tm.year, temp_tm.month, temp_tm.day));

#if NTP_DEBUG_LOG
        get_time_from_nvs(&temp_tm);
        ESP_LOGI(TAG, "check local time of NVS %d-%d-%d %d:%d:%d, day:%d",
                 temp_tm.year, temp_tm.month, temp_tm.day,
                 temp_tm.hour, temp_tm.minute, temp_tm.second,
                 GetDayOfWeek(temp_tm.year, temp_tm.month, temp_tm.day));
#endif

        convertTimestamp(time_stamp, &temp_tm);
        ESP_LOGI(TAG, "check local time of timer %d-%d-%d %d:%d:%d, day:%d",
                 temp_tm.year, temp_tm.month, temp_tm.day,
                 temp_tm.hour, temp_tm.minute, temp_tm.second,
                 GetDayOfWeek(temp_tm.year, temp_tm.month, temp_tm.day));

        while (PowerOnce)
        {
            PowerOnce = false;
            err = Write_PowerTime((short)(temp_tm.year), (short)(temp_tm.month),
                                  (short)(temp_tm.day), (short)(temp_tm.hour),
                                  (short)(temp_tm.minute), (short)(temp_tm.second));
        }
    }
#endif

exit_2:
    if (my_handle != 0)
        nvs_close(my_handle);

    return err;
}

/**
 * @brief  进行SNTP时间更新
 * @note   只有最近复位原因为ESP_RST_POWERON才会进行更新
 *
 */
void User_SNTP_update_time(void)
{
    esp_err_t ret;
    unsigned char retry_conut = 0,
                  retry_conut_max = 3,
                  cnt = 0;
    esp_reset_reason_t reset_reason = esp_reset_reason();

    if (updateTargetClockTimeFromNVS()) // 当NVS未写入时, 则写入闹钟铃响默认值
        SetDefaultTargetClockInfo();
#if PRINT_RESTART_RESON
    switch (reset_reason)
    {
    case ESP_RST_UNKNOWN:
        ESP_LOGE(TAG, "reset reason: unknow...");
        ESP_LOGW(TAG, "chip will restart ! ! !");
        esp_restart();
        break;
    case ESP_RST_POWERON:
        ESP_LOGW(TAG, "Reset due to power-on event...");
        break;
    case ESP_RST_EXT:
        ESP_LOGW(TAG, "Reset by external pin (not applicable for ESP32)...");
        break;
    case ESP_RST_SW:
        ESP_LOGW(TAG, "Software reset via esp_restart...");
        break;
    case ESP_RST_PANIC:
        ESP_LOGW(TAG, "Software reset due to exception/panic...");
        break;
    case ESP_RST_INT_WDT:
        ESP_LOGW(TAG, "Reset (software or hardware) due to interrupt watchdog...");
        break;
    case ESP_RST_TASK_WDT:
        ESP_LOGW(TAG, "Reset due to task watchdog...");
        break;
    case ESP_RST_WDT:
        ESP_LOGW(TAG, "Reset due to other watchdogs...");
        break;
    case ESP_RST_DEEPSLEEP:
        ESP_LOGW(TAG, "Reset after exiting deep sleep mode...");
        break;
    case ESP_RST_BROWNOUT:
        ESP_LOGW(TAG, "Brownout reset (software or hardware)...");
        break;
    case ESP_RST_SDIO:
        ESP_LOGW(TAG, "Reset over SDIO...");
        break;
    default:
        ESP_LOGE(TAG, "reset reason is not in list...");
        ESP_LOGW(TAG, "chip will restart ! ! !");
        esp_restart();
        break;
    }
#endif
err_sntp:
    if (reset_reason && retry_conut < retry_conut_max)
    {
        if (cnt > 2)
            esp_restart();

        // WiFi 未连接
        while (GetWifiConnectStatus() & WIFI_FAIL_BIT)
        {
            vTaskDelay(1);
        }

        retry_conut++;
        memset(err_temp, 0, sizeof(err_temp));
        ESP_LOGI(TAG, "updating time from NVS, %d", retry_conut);
        ret = update_time_from_sntp();
        ESP_GOTO_ON_ERROR(ret,
                          err_sntp,
                          TAG,
                          "update_time_from_nvs failed(err[0]:%d, err[1]:%d])",
                          err_temp[0], err_temp[1]);
    }
    else
    {
        esp_netif_sntp_deinit();
        retry_conut = 0;
        cnt++;
        goto err_sntp;
    }

    return;
}

#if (LED_DRIVER_VERSION_MAJOR != 0)
ISR_SAFE
#endif
int UserNTP_ready(void)
{
    return NtpReady;
}

/// @brief 获取定时器信息(JSON)
/// @param
/// @return
cJSON *GetTimerInfo_JSON(void)
{
    char *timerStr = "timer0";
    char temp[18] = {0};
    uint8_t days = 0;
    cJSON *root = cJSON_CreateObject();
    cJSON *timerInfo = cJSON_CreateObject();
    cJSON_AddItemToObject(root, timerStr, timerInfo);

    cJSON_AddNumberToObject(timerInfo, WEB_TIMER_NUMBER, timerStr[5] - '0');

    memset(temp, 0, sizeof(temp));
    sprintf(temp, "%d:%d:%d", GetTargetTime(0), GetTargetTime(1), GetTargetTime(2));
    cJSON_AddStringToObject(timerInfo, WEB_TARGET_TIME, temp);

    Read_BellDays(&days);
    cJSON_AddNumberToObject(timerInfo, WEB_BELL_DAYS, days);

    cJSON_AddStringToObject(timerInfo, WEB_TIMER_MODE, "periodic");

    return root;
}