/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_freertos_hooks.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_pm.h"

#include "cJSON.h"
#include "User_NVSuse.h"
#include "User_uartDebug.h"
#include "User_IO_init.h"
#include "User_timer.h"
#include "User_WIFI.h"
#include "User_SNTP.h"
#include "User_httpServer.h"
#if !CONFIG_ESPTOOLPY_FLASHSIZE_2MB
#include "User_bluetooth.h"
#endif // // - Part 'factory' 0/0 @ 0x10000 size 0x100000 (overflow 0x307f0)
#include "User_main.h"
#include "User_taskManagement.h"
#include "sdkconfig.h"

#define NO_RESET 0
#define PRINT_INFO 0
#define POWER_BELL 0
#define WIFI_TEST 0
#define WAIT_SNTP 1

static const char *TAG = "USER";

static volatile long long int P_conut = 0LL;
static int bells_time = 0;
static int print_flag = true, run_once = true;

/// @brief bell_level   0:关 1:开
static uint32_t bell_level = 0;

#if (LED_DRIVER_VERSION_MAJOR != 0)
ISR_SAFE static uint32_t dig_num = 1,
                         stopwatchCnt = 0;
ISR_SAFE static My_tm time;
#else
static uint16_t dig_num = 1,
                stopwatchCnt = 0;
static My_tm time;
#endif
/// @brief 共阳数码管数字显示
/// @param
/// @return
#if (LED_DRIVER_VERSION_MAJOR == LED_TICK_VERSION)
ISR_SAFE
#endif
static void showNum(void)
{
#if (LED_DRIVER_VERSION_MAJOR == LED_TASK_VERSION)
    while (1)
    {
#endif
#if (BELL_DRIVER_VERSION_MAJOR == BELL_TICK_VERSION)
        set_user_bell(bell_level);
#endif
        switch ((int)GetClockMode())
        {
        case true:
        {
            switch (dig_num)
            {
            case 1:
                User_LEDshow_num(((stopwatchCnt / 60) / 10) % 10, dig_num);
                break;
            case 2:
                User_LEDshow_num((stopwatchCnt / 60) % 10, dig_num);
                break;
            case 3:
                User_LEDshow_num((stopwatchCnt % 60) / 10, dig_num);
                break;
            case 4:
                User_LEDshow_num(stopwatchCnt % 10, dig_num);
                break;
            case 5:
                dig_num = 0;
                User_LEDshow_num(1, dig_num);
                break;
            default:
                break;
            }
            dig_num++;
            stopwatchCnt = GetStopwatch_record();
        }
        break;
        case false:
        {
#if WAIT_SNTP
            while (UserNTP_ready() == false) // 等待SNTP获取时间
#if (LED_DRIVER_VERSION_MAJOR != LED_TASK_VERSION)
                return;
#else
                vTaskDelay(1);
#endif

#endif

            get_time_from_timer_v2(&time);
            switch (dig_num)
            {
            case 1:
                User_LEDshow_num((time.hour / 10), dig_num);
                break;
            case 2:
                User_LEDshow_num((time.hour % 10), dig_num);
                break;
            case 3:
                User_LEDshow_num((time.minute / 10), dig_num);
                break;
            case 4:
                User_LEDshow_num((time.minute % 10), dig_num);
                break;
            case 5:
                dig_num = 0;
                User_LEDshow_num(two_point_state(), dig_num);
                break;
            default:
                break;
            }
            dig_num++;
        }
        break;
        default:
            ESP_LOGI(__func__, "Unknown clock mode");
            break;
        }
#if (LED_DRIVER_VERSION_MAJOR == 0)
        vTaskDelay(1);
    }
#endif
}
#if (BELL_DRIVER_VERSION_MAJOR == BELL_TASK_VERSION)
/// @brief 铃响任务
/// @param arg
static void BellTask(void *arg)
{
    while (1)
    {
        set_user_bell(bell_level);
        vTaskDelay(1);
    }
}
#endif

static void stopwatchLogic(const uint32_t status)
{
    switch (status)
    {
    case MAIN_TASK:
    {
#if (LED_DRIVER_VERSION_MAJOR == 0)
        vTaskResume(LED_handle);
#endif
        (!read_user_gpio())
            ? stopwatch_Stop()
            : stopwatch_Start();
    }
    break;
    default:
        vTaskDelay(1);
        break;
    }
}

/// @brief 检查本周的某一天是否需要铃响
/// @param today
/// @return 1: 需要铃响, 0: 不需要铃响
static int isBellCreateTimer(const int today)
{
    uint8_t bellDays = 0;
    Read_BellDays(&bellDays);
    return (bellDays & (1 << today)) ? 1 : 0;
}

static void clockLogic(const uint32_t status)
{
    My_tm time;
    esp_err_t closeLedTimer_status;
    uint8_t hour, minute, second;
    switch (status)
    {
    case MAIN_TASK: // periodic 10ms
    {
        P_conut++;
        if (P_conut % 20 == 0 && read_user_gpio() && GetBellFlag()) // 200ms
        {
            // 发出铃响
            bell_level = !bell_level; // 0:关 1:开
            bells_time++;             // 铃响计时

            if (bells_time >= ((5 * 60) * 3)) // 3min
            {
                // 闹钟结束
                ResetBellFlag();    // 复位定时器倒计时标志
                bells_time = 0;     // 铃响计时复位
                bell_level = false; // 铃响电平拉低
            }
        }
        else if (P_conut % 2 == 0 && !read_user_gpio() && GetBellFlag()) // 20ms
        {
            // 手动关闭闹钟
            bells_time = 0;
            bell_level = false;
            ResetBellFlag();
        }
        else if (P_conut % 100 == 0) // 1s
        {
            get_time_from_timer_v2(&time);
            ReadPeriodicCloseLedTargetTime(&hour, &minute, &second);
            while (print_flag < 4)
            {
                while (run_once)
                {
                    run_once = false;
                    // 在预设关闭LED的时间后, 尚未到铃响时间掉电
                    if (time.hour > (int)hour ||
                        (time.hour == (int)hour && time.minute >= (int)minute) ||
                        time.hour <= GetTargetTime(0))
                    {
                        ESP_LOGW(TAG, "CLOSE LED %s",
                                 (target_time_timeout_close_led())
                                     ? "failed"
                                     : "success");
                    }
                    closeLedTimer_status = check_closeLedTimer_status();
                    ESP_LOGI(TAG, "run once, closeLed timer is %s",
                             (closeLedTimer_status) ? "ON" : "OFF");
                    if (closeLedTimer_status == false)
                        delete_closeLedTimer();
                }

                ESP_LOGI(TAG, "func:%s , %d-%d-%d %d:%d:%d,\n \
        (bell ring)target time: %d:%d:%d\n \
        (close led)periodic time: %u:%u:%u",
                         __func__, time.year, time.month, time.day,
                         time.hour, time.minute, time.second,
                         GetTargetTime(0), GetTargetTime(1), GetTargetTime(2),
                         hour, minute, second);
                print_flag++;
                break;
            }

            if (time.hour == (int)hour &&
                time.minute == (int)minute &&
                time.second == (int)second) // 关闭LED
            {
#if (LED_DRIVER_VERSION_MAJOR == LED_TASK_VERSION)
                vTaskSuspend(LED_handle);
#else
                esp_deregister_freertos_tick_hook_for_cpu(showNum, DEFAULT_LED_TASK_CORE);
#endif
                User_LEDall_off();
            }
        }
        else
        {
            // 人为重新设定了铃响时间时
            if (ClockResetFlag())
            {
                bell_level = false;
                SetClockResetFlag();
            }
        }
    }
    break;
    case SNTP_SYNC:
        bell_level = false;      // 先暂停闹钟
        stop_MainTask_timer();   // 防止主任务定时器超时导致OS重启
        User_SNTP_update_time(); // 该函数会阻塞
        start_MainTask_timer();  // 重新启动主任务定时器
        break;
    case USER_CLOCK:
    {
        int tomorrow = 0;
        get_time_from_timer_v2(&time);
        tomorrow = GetDayOfWeek(time.year, time.month, time.day) + 1;
        if (tomorrow == 7) // 周日至周六
            tomorrow = 0;
#if (LED_DRIVER_VERSION_MAJOR == LED_TASK_VERSION)
        vTaskResume(LED_handle); // 恢复LED显示
#else
        esp_register_freertos_tick_hook_for_cpu(showNum, DEFAULT_LED_TASK_CORE); // 恢复LED显示
#endif
        ESP_LOGI(TAG, "%d-%d-%d %d:%d:%d  %s CLOCK belling ! ! !",
                 time.year, time.month, time.day,
                 time.hour, time.minute, time.second, __func__);
        vTaskDelay(1000); // 延时1s后再重新启动闹钟, 不然可能会重复触发闹钟
        if (isBellCreateTimer(tomorrow))
            start_User_clock_timer();
        SetBellFlag(); // 定时器倒计时结束
    }
    break;
    case COUNTDOWN:
    {
        get_time_from_timer_v2(&time);
        uint64_t temp = GetCountdown() / 1000000ULL;
        ESP_LOGI(TAG, "NOW:%d-%d-%d %d:%d:%d====countdown:%lld(h)%lld(m)%lld(s)",
                 time.year, time.month, time.day,
                 time.hour, time.minute, time.second,
                 temp / 3600ULL, (temp % 3600ULL) / 60ULL, temp % 60ULL);
        SetBellFlag(); // 定时器倒计时结束
    }
    break;
#if 1
    case LED_SHOW:
    {
    }
    break;
#endif
    default:
        vTaskDelay(1); // RTOS ticks = 1000Hz, so delay 1ms
        break;
    }
}

/**
 * @brief  用户主逻辑
 *
 */
static void User_main(void *arg)
{
    while (1)
    {
        uint32_t task_mark = NOTHING_TASK;
        // UBaseType_t MsgRet = uxQueueMessagesWaiting(GetMainEventQueueHandle()); // 先查询队列
        xQueueReceive(GetMainEventQueueHandle(), &task_mark, 0);
        switch ((int)GetClockMode())
        {
        case true:
            stopwatchLogic(task_mark);
            break;
        case false:
            clockLogic(task_mark);
            break;
        default:
            break;
        }
        vTaskDelay(1);
    }
}

static void User_task_init(void)
{
    // 任务不宜过多, 否则任务执行需要更多的变量去调整
    // 低优先级数字表示低优先级任务。 空闲任务的优先级为零 (tskIDLE_PRIORITY)。
    xTaskCreatePinnedToCore(User_main,
                            MAIN_TASK_NAME,
                            MAIN_TASK_STACK_SIZE,
                            NULL,
                            MAIN_TASK_PRIORITY,
                            User_NewTask(),
                            1);
#if (LED_DRIVER_VERSION_MAJOR == LED_TASK_VERSION)
    xTaskCreatePinnedToCore(showNum,
                            LED_TASK_NAME,
                            LED_TASK_STACK_SIZE,
                            NULL,
                            LED_TASK_PRIORITY,
                            User_NewTask(),
                            1);
#endif
#if (BELL_DRIVER_VERSION_MAJOR == BELL_TASK_VERSION)
    xTaskCreatePinnedToCore(BellTask,
                            BELL_TASK_NAME,
                            BELL_TASK_STACK_SIZE,
                            NULL,
                            BELL_TASK_PRIORITY,
                            User_NewTask(),
                            1);
#endif
}

#if (LED_DRIVER_VERSION_MAJOR == LED_TICK_VERSION)
esp_freertos_tick_cb_t GetLedDriverTask(const int type)
{
    return (type) ? showNum : User_LEDall_off;
}
#endif

/// @brief 获取板子信息
/// @param
/// @return
cJSON *GetBoardInfo_JSON(void)
{
    esp_chip_info_t chip_info;
    esp_pm_config_t pm_config;
    uint32_t flash_size;
    cJSON *boardInfo = cJSON_CreateObject();
    char temp[32] = {0};

    esp_chip_info(&chip_info);
    esp_pm_get_configuration(&pm_config);
    cJSON_AddStringToObject(boardInfo, WEB_CHIP_MODEL, CONFIG_IDF_TARGET);

    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK)
    {
        printf("Get flash size failed");
        temp[0] = '0';
        temp[1] = ' ';
        temp[2] = 'M';
        temp[3] = 'B';
        temp[4] = '\0';
        goto ZERO_MB;
    }
    memset(temp, 0, sizeof(temp));
    sprintf(temp, "%" PRIu32 " MB", flash_size / (uint32_t)(1024 * 1024));
ZERO_MB:
    cJSON_AddStringToObject(boardInfo, WEB_FLASH_SIZE, temp);

    memset(temp, 0, sizeof(temp));
    sprintf(temp, "%d MHz", pm_config.max_freq_mhz);
    cJSON_AddStringToObject(boardInfo, WEB_CORE_FREQ, temp);

    cJSON_AddStringToObject(boardInfo, WEB_CHIP_WIFI,
                            (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "b/g/n/" : "");

    memset(temp, 0, sizeof(temp));
    sprintf(temp, "%s%s", (chip_info.features & CHIP_FEATURE_BT) ? "BT/" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "BLE/" : "");
    cJSON_AddStringToObject(boardInfo, WEB_CHIP_BT, temp);

    return boardInfo;
}

void app_main(void)
{
#if PRINT_INFO
    printf("Hello world!\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT/" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE/" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK)
    {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n",
           esp_get_minimum_free_heap_size());
#endif

    esp_pm_config_t pm_config = {
        .max_freq_mhz = 80,
        .min_freq_mhz = 80,
        .light_sleep_enable = false,
    };

    vTaskDelay(100);   // 等待board打印信息, 防止未打印完就打印新信息
    uart_debug_init(); // 初始化串口调试
    User_NVS_Init();   // 初始化NVS
    User_gpio_init();  // 初始化IO
    User_LEDall_off(); // 关闭所有LED
#if POWER_BELL
    set_user_bell(1);
    vTaskDelay(100);
    set_user_bell(0);
#endif // 表明上电, 时钟'哔'一下

    User_WIFI_Init(); // 初始化WIFI

#if !WIFI_TEST
#if WAIT_SNTP
    User_SNTP_update_time(); // 先用SNTP获取时间
#endif

    User_Timer_Init(); // 初始化定时器
    User_task_init();  // 初始化主任务
#endif

#if !CONFIG_ESPTOOLPY_FLASHSIZE_2MB
    User_bluetooth_init();
#endif

    printf("Minimum free heap size: %" PRIu32 " bytes\n",
           esp_get_minimum_free_heap_size());
#if CONFIG_PM_ENABLE
    ESP_ERROR_CHECK(esp_pm_configure(&pm_config)); // 降低CPU运行频率
#endif
    esp_pm_get_configuration(&pm_config);
    ESP_LOGI(TAG, "MAX_freq %d MHz, MIN_freq %d MHz",
             pm_config.max_freq_mhz, pm_config.min_freq_mhz);

#if (LED_DRIVER_VERSION_MAJOR == LED_TICK_VERSION)
    esp_err_t ret = esp_register_freertos_tick_hook_for_cpu(showNum, DEFAULT_LED_TASK_CORE);
    ESP_LOGI(TAG, "esp_register_freertos_tick_hook_for_cpu %d", ret);
#endif
#if NO_RESET
    ESP_LOGI("RESTART", "user string...");
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
#endif
}
