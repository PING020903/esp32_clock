#include "esp_system.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "nvs_flash.h" // 从NVS中读取时间戳要用, 保存目标铃响时间要用
#include "esp_sntp.h"
#include "esp_err.h"
#include "keep_stamp.h"
#include "User_NVSuse.h"
#include "User_SNTP.h"

#define SNTP_INIT_PRINT 0
#define NTP_DEBUG_LOG 0
#define PRINT_RESTART_RESON 0

static const char *TAG = "User_SNTP";
static int err_temp[2] = {0};
static My_tm clock_tm = {0};

static uint8_t flag_show = true;
static int NtpReady = false;

static int isLeapYear(int year)
{
    if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)
    {
        return 1; // 是闰年
    }
    return 0; // 不是闰年
}

static void convertTimestamp(time_t timestamp, My_tm *my_tm)
{
    // 计算每个时间单位的秒数
    const long secondsPerMinute = 60;
    const long secondsPerHour = 3600;
    const long secondsPerDay = 86400;
    const long secondsPerYear = 31536000;

    // 调整为 UTC+8 时区
    timestamp += 8 * secondsPerHour;

    // 计算年份
    my_tm->year = 1970;
    while (timestamp >= secondsPerYear)
    {
        int daysInYear = isLeapYear(my_tm->year) ? 366 : 365;
        timestamp -= daysInYear * secondsPerDay;
        (my_tm->year)++;
    }

    // 计算月份和日期
    my_tm->month = 1;
    while (1)
    {
        int daysInMonth;
        switch (my_tm->month)
        {
        case 2: // 二月
            daysInMonth = isLeapYear(my_tm->year) ? 29 : 28;
            break;
        case 4:  // 四月
        case 6:  // 六月
        case 9:  // 九月
        case 11: // 十一月
            daysInMonth = 30;
            break;
        default: // 其他月份
            daysInMonth = 31;
            break;
        }

        if (timestamp < daysInMonth * secondsPerDay)
        {
            break;
        }

        timestamp -= (daysInMonth * secondsPerDay);
        (my_tm->month)++;
    }

    my_tm->day = timestamp / secondsPerDay + 1;
    timestamp %= secondsPerDay;

    // 计算小时
    my_tm->hour = timestamp / secondsPerHour;
    timestamp %= secondsPerHour;

    // 计算分钟
    my_tm->minute = timestamp / secondsPerMinute;
    timestamp %= secondsPerMinute;

    // 秒数
    my_tm->second = timestamp;
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

void UserConvertTimestamp(const int timezone, const int type,
                          time_t timestamp, My_tm *my_tm)
{
    // 计算每个时间单位的秒数
    const long secondsPerMinute = 60;
    const long secondsPerHour = 3600;
    const long secondsPerDay = 86400;
    const long secondsPerYear = 31536000;
    const long msPerSecond = 1000;

    // 提取毫秒部分
    my_tm->ms = (int)(timestamp % msPerSecond);
    timestamp /= msPerSecond;

    // 调整时区
    timestamp += timezone * secondsPerHour;

    // 计算年份
    my_tm->year = 1970;
    while (timestamp >= secondsPerYear)
    {
        int daysInYear = isLeapYear(my_tm->year) ? 366 : 365;
        timestamp -= daysInYear * secondsPerDay;
        (my_tm->year)++;
    }

    // 计算月份和日期
    my_tm->month = 1;
    while (1)
    {
        int daysInMonth;
        switch (my_tm->month)
        {
        case 2: // 二月
            daysInMonth = isLeapYear(my_tm->year) ? 29 : 28;
            break;
        case 4:  // 四月
        case 6:  // 六月
        case 9:  // 九月
        case 11: // 十一月
            daysInMonth = 30;
            break;
        default: // 其他月份
            daysInMonth = 31;
            break;
        }

        if (timestamp < daysInMonth * secondsPerDay)
        {
            break;
        }

        timestamp -= (daysInMonth * secondsPerDay);
        (my_tm->month)++;
    }

    my_tm->day = timestamp / secondsPerDay + 1;
    timestamp %= secondsPerDay;

    // 计算小时
    my_tm->hour = timestamp / secondsPerHour;
    timestamp %= secondsPerHour;

    // 计算分钟
    my_tm->minute = timestamp / secondsPerMinute;
    timestamp %= secondsPerMinute;

    // 计算秒数
    my_tm->second = timestamp;
}

/// @brief 获取本地时间戳
/// @param
/// @return
time_t GetTimestamp(void)
{
    return get_User_timestamp2();
}

/**
 * @brief  获取本地timer时间戳并转换年月日
 * @note    timestamp from GP-tiemr
 * @return time_t   User时间戳
 *
 */
void get_time_from_timer_v2(My_tm *my_tm)
{
    time_t now = get_User_timestamp2();
    convertTimestamp(now, my_tm);
}

/**
 * @brief  更新时钟tm值
 *
 */
void update_clock_timer_tm(void)
{
#if KEEP_TIMESTAMP_SOFTWARE_VERSION
#endif
#if KEEP_TIMESTAMP_HARDWARE_VERSION
    time_t timestamp_temp = get_User_timestamp2();
#endif
    convertTimestamp(timestamp_temp, &clock_tm);
}

/**
 * @brief  从NVS获取时间戳并转换年月日
 * @note    NVS时间有时候不太准确
 *
 */
void get_time_from_nvs(My_tm *my_tm)
{
    time_t timestamp;
    time(&timestamp); // 获取本地时间戳
    convertTimestamp(timestamp, my_tm);
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

    update_clock_timer_tm();
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

    const int retry_count = 120;
    while (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(500)) != ESP_OK &&
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
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        err_temp[1] = 1;
        goto exit_1;
    }

    // write
    err = nvs_set_i64(my_handle, "time_stamp", now); // 写入时间戳
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
    err = nvs_get_i64(my_handle, "time_stamp", &time_stamp); // 获取时间戳
    if (err == ESP_ERR_NVS_NOT_FOUND)                        // 这个选项一般情况是不会进入的
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
        err = nvs_get_i64(my_handle, "time_stamp", &time_stamp); // 读取时间戳in nvs
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
#if KEEP_TIMESTAMP_SOFTWARE_VERSION
#endif
#if KEEP_TIMESTAMP_HARDWARE_VERSION
        *Set_User_timestamp() = time_stamp;
        keep_userTimestamp_timer_ISR();
#endif

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

/// @brief NTP 是否就绪
/// @param
/// @return OK:true 1; FAIL:false 0;
int UserNTP_ready(void)
{
    return NtpReady;
}
