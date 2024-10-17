#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "keep_stamp.h"
#include "User_SNTP.h"

static const char *TAG = "keep_stamp";



#if KEEP_TIMESTAMP_SOFTWARE_VERSION

#endif
#if KEEP_TIMESTAMP_HARDWARE_VERSION
static gptimer_handle_t clock_timer = NULL;
static uint8_t timer_create_flag2 = true;
static time_t IRAM_ATTR User_timestampISR = 0;
#endif


/**
 * @brief  获取User时间戳
 * @note    timestamp from GP-tiemr
 * @return time_t   User时间戳
 *
 */
time_t get_User_timestamp2(void)
{
    return User_timestampISR;
}

time_t* Set_User_timestamp(void)
{
    return &User_timestampISR;
}

#if KEEP_TIMESTAMP_HARDWARE_VERSION
/**
 * @brief  gptimer 中断回调函数
 * @note   触发警告值的时候会调用回调函数
 *
 */
static bool IRAM_ATTR gptimer_keeptimestamp_callback(gptimer_handle_t timer,
                                                     const gptimer_alarm_event_data_t *edata,
                                                     void *user_data)
{
    BaseType_t high_task_awoken = pdFALSE;
    User_timestampISR += 1;
    return (high_task_awoken == pdTRUE);
}
#endif

#if KEEP_TIMESTAMP_SOFTWARE_VERSION
#endif

#if KEEP_TIMESTAMP_SOFTWARE_VERSION
#endif

#if KEEP_TIMESTAMP_HARDWARE_VERSION 
/**
 * @brief  时间戳定时器ISR版本
 * @note   无论 softwareTimer 还是 hardwareTimer 都会有精度损失, 但硬件版本会更精准
 *
 */
void keep_userTimestamp_timer_ISR(void)
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