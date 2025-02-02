#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "User_IO_init.h"
#include "User_SNTP.h"
#include "User_main.h"
#include "User_taskManagement.h"
#include "User_timer.h"

static const char *TAG = "User_Timer";

// task_queue
static QueueSetHandle_t task_evt_queue = NULL;
static esp_timer_handle_t main_timer;
static esp_timer_handle_t SNTP_timer;
static esp_timer_handle_t Clock_timer;
#if (LED_DRIVER_VERSION_MAJOR == 0)
static esp_timer_handle_t Two_point_timer;
#endif
static esp_timer_handle_t Countdown_timer;
static esp_timer_handle_t Stopwatch_timer;
static esp_timer_handle_t CloseLed_timer;
static uint8_t clock_reset_flag = false;

#if (LED_DRIVER_VERSION_MAJOR != 0)
ISR_SAFE static uint32_t clockMode = false; // false: 闹钟模式, true: 计时模式
ISR_SAFE static uint32_t stopwatch_record = 0;
ISR_SAFE static uint32_t two_point = 0;
#else
static bool clockMode = false; // false: 闹钟模式, true: 计时模式
static uint16_t stopwatch_record = 0;
static bool two_point = 0;
#endif

/// @brief bell_flag    铃响标记( 是否允许铃响 )
static uint8_t bell_flag = false;

#if KEEP_STAMP
static gptimer_handle_t clock_timer = NULL;
static bool timer_create_flag2 = true;
ISR_SAFE static time_t User_timestampISR = 0;

/// @brief 获取User时间戳(ISR)
/// @param
/// @return
ISR_SAFE time_t GetUserTimestampISR(void)
{
    return User_timestampISR;
}

/// @brief 获取User时间戳
/// @param
/// @return
time_t GetUserTimestamp(void)
{
    return User_timestampISR;
}

/// @brief 设置User时间戳
/// @param timeStamp
void Set_User_timestamp(const time_t timeStamp)
{
    User_timestampISR = timeStamp;
    return;
}

/**
 * @brief  gptimer 中断回调函数
 * @note   触发警告值的时候会调用回调函数
 *
 */
ISR_SAFE static bool gptimer_keeptimestamp_callback(gptimer_handle_t timer,
                                                    const gptimer_alarm_event_data_t *edata,
                                                    void *user_data)
{
    BaseType_t high_task_awoken = pdFALSE;
    ++User_timestampISR;    // 时间戳
    two_point = !two_point; // 中间两点
    return (high_task_awoken == pdTRUE);
}

/**
 * @brief  时间戳定时器ISR版本
 * @note   无论 softwareTimer 还是 hardwareTimer 都会有精度损失, 但硬件版本会更精准
 *
 */
static void keep_userTimestamp_timer_ISR(void)
{
    if (timer_create_flag2)
    {
        gptimer_config_t timer_config = {
            .clk_src = GPTIMER_CLK_SRC_DEFAULT, // 时钟源
            .direction = GPTIMER_COUNT_UP,      // 计数方向(+)
            .intr_priority = 0,                 // 优先级
            .resolution_hz = 1000000,           // 1MHz, 1 tick=1us(不能<=1Hz, 会断言失败)
        };

        esp_err_t ret = gptimer_new_timer(&timer_config, &clock_timer);
        switch (ret)
        {
        case ESP_OK:
            ESP_LOGI(TAG, "GPtimer create successfully...");
            break;
        case ESP_ERR_INVALID_ARG:
            ESP_LOGE(TAG, "Create GPTimer failed because of invalid argument ! ! !");
            return;
        case ESP_ERR_NO_MEM:
            ESP_LOGE(TAG, "Create GPTimer failed because out of memory ! ! !");
            return;
        case ESP_ERR_NOT_FOUND:
            ESP_LOGE(TAG, "Create GPTimer failed because all hardware timers are used up and no more free one");
            return;

        default:
            ESP_LOGE(TAG, "Create GPTimer failed because of other error...");
            break;
        }

        gptimer_event_callbacks_t callback_func = {
            .on_alarm = gptimer_keeptimestamp_callback,
        };
        ESP_ERROR_CHECK(gptimer_register_event_callbacks(clock_timer,
                                                         &callback_func,
                                                         NULL));
        ESP_ERROR_CHECK(gptimer_enable(clock_timer));

        gptimer_alarm_config_t alarm_config = {
            .alarm_count = 1000000,             // 目标值, 计数这么多次以后就是1s
            .reload_count = 0,                  // 重载值
            .flags.auto_reload_on_alarm = true, // 是否开启重载
        };
        ESP_ERROR_CHECK(gptimer_set_alarm_action(clock_timer, &alarm_config));
        ESP_ERROR_CHECK(gptimer_start(clock_timer));
    }
    else
        ESP_LOGW(TAG, "GPtimer created...");

    timer_create_flag2 = false;
}
#endif

QueueSetHandle_t GetMainEventQueueHandle(void)
{
    return task_evt_queue;
}
static void Main_Timer_callback(void *arg)
{
    uint32_t task_mark = MAIN_TASK;
    xQueueSend(task_evt_queue, &task_mark, 0);
}
static void SNTP_Timer_callback(void *arg)
{
    uint32_t task_mark = SNTP_SYNC;
    xQueueSend(task_evt_queue, &task_mark, 0);
}
static void UserClock_Timer_callback(void *arg)
{
    uint32_t task_mark = USER_CLOCK;
    xQueueSend(task_evt_queue, &task_mark, 0);
}
#if (LED_DRIVER_VERSION_MAJOR == 0)
static void Two_point_Timer_callback(void *arg)
{
    two_point = !two_point;
}
#endif
static void countdown_callback(void *arg)
{
    uint32_t task_mark = COUNTDOWN;
    xQueueSend(task_evt_queue, &task_mark, 0);
}
static void stopwatch_callback(void *arg)
{
    stopwatch_record++;
}
static void closeLed_callback(void *arg)
{
    My_tm time;
    get_time_from_timer_v2(&time);
#if (LED_DRIVER_VERSION_MAJOR == LED_TASK_VERSION)
    vTaskSuspend(User_GetTaskHandle(LED_TASK_NAME));
#else
    esp_deregister_freertos_tick_hook_for_cpu(GetLedDriverTask(1), DEFAULT_LED_TASK_CORE);
#endif
    User_LEDall_off();
    ESP_LOGI(TAG, "turn off led, %d:%d:%d", time.hour, time.minute, time.second);
}


/// @brief 获取铃响状态
/// @param
/// @return OK:ture(1), ERR:false(0)
uint8_t GetBellFlag(void)
{
    return bell_flag;
}

/// @brief 设置铃响状态
/// @param
void SetBellFlag(void)
{
    bell_flag = true;
}

/// @brief 重置铃响状态
/// @param
void ResetBellFlag(void)
{
    bell_flag = false;
}

void User_Timer_Init(void)
{
    task_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    const esp_timer_create_args_t main_timer_args = {
        .callback = &Main_Timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "Main_Timer",
        .skip_unhandled_events = false,
    };

    const esp_timer_create_args_t clock_timer_agrs = {
        .callback = &UserClock_Timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "Clock_Timer",
        .skip_unhandled_events = false,
    };

    const esp_timer_create_args_t sntp_timer_args = {
        .callback = &SNTP_Timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "SNTP_Timer",
        .skip_unhandled_events = false,
    };
#if (LED_DRIVER_VERSION_MAJOR == 0)
    const esp_timer_create_args_t two_point_timer_args = {
        .callback = &Two_point_Timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "Two_point_Timer",
        .skip_unhandled_events = false,
    };
#endif
    const esp_timer_create_args_t countdown_timer_args = {
        .callback = &countdown_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "Countdown_Timer",
        .skip_unhandled_events = false,
    };

    const esp_timer_create_args_t stopwatch_timer_args = {
        .callback = &stopwatch_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "Stopwatch_Timer",
        .skip_unhandled_events = false,
    };

    const esp_timer_create_args_t closeLed_timer_args = {
        .callback = &closeLed_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "closeLed_Timer",
        .skip_unhandled_events = false,
    };

    ESP_LOGI(TAG, "stamp timer init");
    keep_userTimestamp_timer_ISR();

    ESP_LOGI(TAG, "User_timer_init");

    ESP_ERROR_CHECK(esp_timer_create(&main_timer_args, &main_timer));
    ESP_ERROR_CHECK(esp_timer_create(&sntp_timer_args, &SNTP_timer));
    ESP_ERROR_CHECK(esp_timer_create(&clock_timer_agrs, &Clock_timer));
#if (LED_DRIVER_VERSION_MAJOR == 0)
    ESP_ERROR_CHECK(esp_timer_create(&two_point_timer_args, &Two_point_timer));
#endif
    ESP_ERROR_CHECK(esp_timer_create(&countdown_timer_args, &Countdown_timer));
    ESP_ERROR_CHECK(esp_timer_create(&stopwatch_timer_args, &Stopwatch_timer));
    ESP_ERROR_CHECK(esp_timer_create(&closeLed_timer_args, &CloseLed_timer)); // 要修改该逻辑

    ESP_ERROR_CHECK(esp_timer_start_periodic(main_timer, MAIN_TASK_PERIOD)); // 10ms
    ESP_ERROR_CHECK(esp_timer_start_periodic(SNTP_timer, ONE_HOUR * 8));     // 8h
#if (LED_DRIVER_VERSION_MAJOR == 0)
    ESP_ERROR_CHECK(esp_timer_start_periodic(Two_point_timer, ONE_MS * 500));
#endif

    uint64_t ret = User_clock();
    if (!ret) // ret value is 0
    {
        ESP_LOGI(TAG, "SNTP return fial time, OS will restart...");
        esp_restart();
    }
    ESP_LOGI(TAG, "%llu us will clock bells...", ret);
    ESP_ERROR_CHECK(esp_timer_start_once(Clock_timer, ret));

    ESP_LOGI(TAG, "Timer start ...");
}

/// @brief 启动闹钟定时器
/// @param
/// @note 用于闹钟定时器结束以后再次启动
void start_User_clock_timer(void)
{

    uint64_t clock_ret;
    if (esp_timer_is_active(Clock_timer))
    {
        ESP_LOGW(TAG, "timer is running ! ! ! will stop first");
        esp_err_t err = esp_timer_stop(Clock_timer);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "timer is not running ! ! !");
            return;
        }
    }

    clock_ret = User_clock();
    ESP_LOGW(TAG, "1--RET=%llu", clock_ret);

    // last timer is not destroy in here, so need to wait
    vTaskDelay(500 / portTICK_PERIOD_MS);
    if (clock_ret) // clock_ret value is not 0
    {
        ESP_ERROR_CHECK(esp_timer_start_once(Clock_timer, clock_ret));
    }
    else // 返回值为零
    {
        clock_ret = DAY_OF_HOUR * MINUTE_OF_HOUR * SECOND_OF_MINUTE;
        clock_ret *= 1000000ULL;
        ESP_LOGW(TAG, "2--RET=%llu", clock_ret);
        ESP_ERROR_CHECK(esp_timer_start_once(Clock_timer, clock_ret));
    }
}

/// @brief 重启闹钟定时器
/// @param
/// @return OK: ESP_OK; ERROR: ESP_FAIL, ESP_ERR_INVALID_STATE, ESP_ERR_INVALID_ARG
/// @note 用于更改了目标铃响时间
esp_err_t restartOfRunning_UserClockTimer(void)
{
    esp_err_t err;
    if (!esp_timer_is_active(Clock_timer))
    {
        // 如果没有启动, 则直接启动
        ESP_LOGI(TAG, "Timer is not running, start it");
        start_User_clock_timer();
        return ESP_OK;
    }

    ResetBellFlag(); // 若正在响, 则停止
    err = esp_timer_restart(Clock_timer, User_clock());
    if (err)
    {
        switch (err)
        {
        case ESP_ERR_INVALID_ARG:
            ESP_LOGE(TAG, "<esp_timer_restart> the handle is invalid");
            return ESP_ERR_INVALID_ARG;
        case ESP_ERR_INVALID_STATE:
            ESP_LOGE(TAG, "<esp_timer_restart>  the timer is not running");
            return ESP_ERR_INVALID_STATE;
        default:
            ESP_LOGE(TAG, "unknow reason, esp_timer_restart failed ! ! !");
            return ESP_FAIL;
        }
    }
    clock_reset_flag = true;
    return err;
}

// 停止MainTask timer
void stop_MainTask_timer(void)
{
    esp_err_t err = esp_timer_stop(main_timer);
    const char *str[] = {
        "Main timer is not running ! ! !",
        "Main timer was stopped...."};
    ESP_LOGI(TAG, "%s", (err) ? str[0] : str[1]);
}

// 启动MainTask timer
void start_MainTask_timer(void)
{
    esp_err_t err = esp_timer_start_periodic(main_timer, MAIN_TASK_PERIOD);
    switch (err)
    {
    case ESP_OK:
        ESP_LOGI(TAG, "Main timer has resumed....");
        return;
    case ESP_ERR_INVALID_STATE:
        ESP_LOGW(TAG, "Main timer is running ! ! !");
        return;
    case ESP_ERR_INVALID_ARG:
        ESP_LOGW(TAG, "Main timer period is invalid ! ! !\n will be restart");
        esp_restart();
        break;

    default:
        ESP_LOGW(TAG, "Main timer start unknow error ! ! !");
        break;
    }
}

#if (LED_DRIVER_VERSION_MAJOR != 0)
IRAM_ATTR uint32_t two_point_state(void)
{
    return two_point;
}
#else
bool two_point_state(void)
{
    return two_point;
}
#endif

/// @brief 获取闹钟重置标志
/// @param
/// @return
uint8_t ClockResetFlag(void)
{
    return clock_reset_flag;
}

/// @brief 清除闹钟重置标志
/// @param
void SetClockResetFlag(void)
{
    clock_reset_flag = false;
    return;
}

/// @brief 启动倒计时定时器
/// @param us
/// @return OK:ESP_OK; ERROR:ESP_ERR_INVALID_STATE;
esp_err_t startCountdownTimer(const uint64_t us)
{
    return (esp_timer_is_active(Countdown_timer))
               ? esp_timer_restart(Countdown_timer, us)
               : esp_timer_start_once(Countdown_timer, us);
}

/// @brief 启动秒表定时器
/// @param
/// @return
esp_err_t stopwatch_Start(void)
{
    return (esp_timer_is_active(Stopwatch_timer))
               ? ESP_FAIL
               : esp_timer_start_periodic(Stopwatch_timer, ONE_MS * 1000);
}

/// @brief 停止秒表定时器
/// @param
/// @return
esp_err_t stopwatch_Stop(void)
{
    return (esp_timer_is_active(Stopwatch_timer))
               ? esp_timer_stop(Stopwatch_timer)
               : ESP_FAIL;
}

#if (LED_DRIVER_VERSION_MAJOR != 0)
ISR_SAFE uint32_t GetStopwatch_record(void)
{
    return stopwatch_record;
}
#else
uint16_t GetStopwatch_record(void)
{
    return stopwatch_record;
}
#endif

/// @brief 重置秒表记录
/// @param
void SetStopwatch_record(void)
{
    stopwatch_record = 0;
}

#if (LED_DRIVER_VERSION_MAJOR != 0)
IRAM_ATTR uint32_t GetClockMode(void)
{
    return clockMode;
}
IRAM_ATTR uint32_t SetClockMode(void)
{
    return clockMode = !clockMode;
}
#else
bool GetClockMode(void)
{
    return clockMode;
}

/// @brief 设置时钟模式
/// @param
/// @return true: 秒表模式, false: 时钟模式
bool SetClockMode(void)
{
    return clockMode = !clockMode;
}
#endif

/// @brief 检查秒表状态
/// @param
/// @return
bool CheckStopwatch(void)
{
    bool status = esp_timer_is_active(Stopwatch_timer);
#if (LED_DRIVER_VERSION_MAJOR != 0)
    ESP_LOGI(TAG, "Stopwatch is %s, record: %lu(s)",
             (status) ? "running" : "stopped",
             stopwatch_record);
#else
    ESP_LOGI(TAG, "Stopwatch is %s, record: %u(s)",
             (status) ? "running" : "stopped",
             stopwatch_record);
#endif

    return status;
}

/// @brief 目标时间超时关闭LED
/// @param
/// @return
esp_err_t target_time_timeout_close_led(void)
{
    esp_err_t err = ESP_OK;
    const int showLedTimes = 3; // minute
    uint64_t ret = User_clock();
    if (ret < (ONE_MINUTE * showLedTimes))
    {
        ESP_LOGI(TAG, "Less than %d minutes to bell target", showLedTimes);
        return ESP_FAIL;
    }
    err = esp_timer_start_once(CloseLed_timer, ONE_MINUTE * showLedTimes);
    return err;
}

esp_err_t check_closeLedTimer_status(void)
{
    return (esp_err_t)esp_timer_is_active(CloseLed_timer);
}

esp_err_t delete_closeLedTimer(void)
{
    return esp_timer_delete(CloseLed_timer);
}
