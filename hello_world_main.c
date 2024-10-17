/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_check.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_pm.h"

#include "User_NVSuse.h"
#include "User_uartDebug.h"
#include "User_IO_init.h"
#include "User_timer.h"
#include "User_WIFI.h"
#include "User_SNTP.h"
#if !CONFIG_ESPTOOLPY_FLASHSIZE_2MB
#include "User_bluetooth.h"
#endif // // - Part 'factory' 0/0 @ 0x10000 size 0x100000 (overflow 0x307f0)

#define NO_RESET 0
#define PRINT_INFO 0
TaskHandle_t LED_handle = NULL;
TaskHandle_t Bell_handle = NULL;
static const char *TAG = "USER";

static volatile long long int P_conut = 00;
static int bells_time = 0;
static int print_flag = true;

/// @brief bell_level   0:关 1:开
static uint8_t bell_level = 0;

static void stopwatchLogic(const uint32_t status)
{
    switch (status)
    {
    case MAIN_TASK:
    {
        vTaskResume(LED_handle);
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

static void clockLogic(const uint32_t status)
{
    My_tm time;
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
            uint8_t hour, minute, second;
            get_time_from_timer_v2(&time);
            ReadPeriodicCloseLedTargetTime(&hour, &minute, &second);
            while (print_flag < 4)
            {
                // print_flag = false;
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
                vTaskSuspend(LED_handle);
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
        get_time_from_timer_v2(&time);
        vTaskResume(LED_handle); // 恢复LED显示
        ESP_LOGI(TAG, "%d-%d-%d %d:%d:%d  %s CLOCK bells ! ! !",
                 time.year, time.month, time.day,
                 time.hour, time.minute, time.second, __func__);
        vTaskDelay(1000); // 延时1s后再重新启动闹钟, 不然可能会重复触发闹钟
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

    default:
        vTaskDelay(1); // RTOS ticks = 1000Hz, so delay 1ms
        break;
    }
}

/**
 * @brief  用户主逻辑
 *
 */
void User_main(void *arg)
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
    }
}

/**
 * @brief  LED显示任务
 *
 */
void showNum(void *arg)
{
    uint16_t dig_num = 1,
             delay = 0,
             stopwatchCnt = 0;
    My_tm time;
    get_time_from_timer_v2(&time);
    while (1)
    {
        switch ((int)GetClockMode())
        {
        case true:
        {
            int num1 = ((stopwatchCnt / 60) / 10) % 10,
                num2 = (stopwatchCnt / 60) % 10,
                num3 = (stopwatchCnt % 60) / 10,
                num4 = stopwatchCnt % 10;

            switch (dig_num)
            {
            case 1:
                User_LEDshow_num(num1, dig_num);
                break;
            case 2:
                User_LEDshow_num(num2, dig_num);
                break;
            case 3:
                User_LEDshow_num(num3, dig_num);
                break;
            case 4:
                User_LEDshow_num(num4, dig_num);
                break;
            case 5:
                User_LED_digital_on(1, 0);
                dig_num = 0;
                break;
            default:
                break;
            }
            dig_num++;
            stopwatchCnt = GetStopwatch_record();
            vTaskDelay(1);
        }
        break;
        case false:
        {
            while (UserNTP_ready() == false) // 等待SNTP获取时间
                vTaskDelay(1);

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
                User_LED_digital_on((int)two_point_state(), 0);
                // 中间两点的显示放于此处, 并没有导致显示有问题,
                // 但放于User_main()中会导致显示有问题
                dig_num = 0;
                break;
            default:
                break;
            }
            dig_num++;
            delay++;
            if (delay % 1000 == 0)
                get_time_from_timer_v2(&time);
            vTaskDelay(1);
        }
        break;
        default:
            ESP_LOGI(__func__, "Unknown clock mode");
            break;
        }
    }
}

/**
 * @brief  铃响任务
 *
 */
void clock_bells(void *arg)
{
    while (1)
    {
        set_user_bell(bell_level);
        vTaskDelay(8);
    }
}

void User_task_init(void)
{
    // 任务不宜过多, 否则任务执行需要更多的变量去调整
    // 低优先级数字表示低优先级任务。 空闲任务的优先级为零 (tskIDLE_PRIORITY)。
    xTaskCreatePinnedToCore(User_main, "main_task", 4096, NULL, 6, NULL, 1);
    xTaskCreatePinnedToCore(showNum, "LED_task", 1024, NULL, 4, &LED_handle, 1);
    xTaskCreatePinnedToCore(clock_bells, "bells_task", 1024, NULL, 2, &Bell_handle, 1);
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
    ESP_ERROR_CHECK(esp_pm_configure(&pm_config)); // 降低CPU运行频率

    User_NVS_Init();   // 初始化NVS
    User_gpio_init();  // 初始化IO
    User_LEDall_off(); // 关闭所有LED
#if 1
    set_user_bell(1);
    vTaskDelay(100);
    set_user_bell(0);
#endif // 表明上电, 时钟'哔'一下

    User_WIFI_Init();  // 初始化WIFI
    uart_debug_init(); // 初始化串口调试

#if 1
    User_SNTP_update_time(); // 先用SNTP获取时间
    User_Timer_Init();       // 初始化定时器
    User_task_init();        // 初始化主任务
#endif

#if !CONFIG_ESPTOOLPY_FLASHSIZE_2MB
    User_bluetooth_init();
#endif

    printf("Minimum free heap size: %" PRIu32 " bytes\n",
           esp_get_minimum_free_heap_size());
    esp_pm_get_configuration(&pm_config);
    ESP_LOGI(TAG, "MAX_freq %d MHz, MIN_freq %d MHz",
             pm_config.max_freq_mhz, pm_config.min_freq_mhz);

#if NO_RESET
    ESP_LOGI("RESTART", "user string...");
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
#endif
}
