#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "User_timer.h"
#include "User_SNTP.h"

static const char *TAG = "User_Timer";
static bool two_point = 0;

// task_queue
static QueueSetHandle_t task_evt_queue = NULL;
static esp_timer_handle_t main_timer;
static esp_timer_handle_t SNTP_timer;
static esp_timer_handle_t Clock_timer;
static esp_timer_handle_t Two_point_timer;
static esp_timer_handle_t Countdown_timer;
static esp_timer_handle_t Stopwatch_timer;
static uint8_t clock_reset_flag = false;
static uint16_t stopwatch_record = 0;
static bool clockMode = false; // false: 闹钟模式, true: 计时模式


/// @brief bell_flag    铃响标记( 是否允许铃响 )
static uint8_t bell_flag = false;

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
static void Two_point_Timer_callback(void *arg)
{
    two_point = !two_point;
}
static void countdown_callback(void *arg)
{
    uint32_t task_mark = COUNTDOWN;
    xQueueSend(task_evt_queue, &task_mark, 0);
}
static void stopwatch_callback(void *arg)
{
    stopwatch_record++;
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

    const esp_timer_create_args_t two_point_timer_args = {
        .callback = &Two_point_Timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "Two_point_Timer",
        .skip_unhandled_events = false,
    };

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

    ESP_LOGI(TAG, "User_timer_init");

    ESP_ERROR_CHECK(esp_timer_create(&main_timer_args, &main_timer));
    ESP_ERROR_CHECK(esp_timer_create(&sntp_timer_args, &SNTP_timer));
    ESP_ERROR_CHECK(esp_timer_create(&clock_timer_agrs, &Clock_timer));
    ESP_ERROR_CHECK(esp_timer_create(&two_point_timer_args, &Two_point_timer));
    ESP_ERROR_CHECK(esp_timer_create(&countdown_timer_args, &Countdown_timer));
    ESP_ERROR_CHECK(esp_timer_create(&stopwatch_timer_args, &Stopwatch_timer));

    ESP_ERROR_CHECK(esp_timer_start_periodic(main_timer, MAIN_TASK_PERIOD)); // 10ms
    ESP_ERROR_CHECK(esp_timer_start_periodic(SNTP_timer, ONE_HOUR * 8));     // 8h
    ESP_ERROR_CHECK(esp_timer_start_periodic(Two_point_timer, ONE_MS * 500));
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

/// @brief 重启闹钟定时器
/// @param
/// @return OK: ESP_OK; ERROR: ESP_FAIL, ESP_ERR_INVALID_STATE, ESP_ERR_INVALID_ARG
/// @note 用于更改了目标铃响时间
esp_err_t restartOfRunning_UserClockTimer(void)
{
    esp_err_t err;
    if (!esp_timer_is_active(Clock_timer))
    {
        ESP_LOGI(TAG, "Timer is not running, return");
        return ESP_FAIL;
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

uint8_t two_point_state(void)
{
    return two_point;
}

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

/// @brief 获取秒表记录
/// @param
/// @return
uint16_t GetStopwatch_record(void)
{
    return stopwatch_record;
}
/// @brief 重置秒表记录
/// @param
void SetStopwatch_record(void)
{
    stopwatch_record = 0;
}

/// @brief 获取时钟模式
/// @param
/// @return true: 秒表模式, false: 时钟模式
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

/// @brief 检查秒表状态
/// @param  
/// @return 
bool CheckStopwatch(void)
{
    bool status = esp_timer_is_active(Stopwatch_timer);
    ESP_LOGI(TAG, "Stopwatch is %s, record: %u(s)",
             (status) ? "running" : "stopped",
             stopwatch_record);
    return status;
}
