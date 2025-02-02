#pragma once
#ifndef __USER_TIMER_H__
#define __USER_TIMER_H__

#define KEEP_STAMP true

#ifndef DAY_OF_HOUR
#define DAY_OF_HOUR (24)
#endif
#ifndef MINUTE_OF_HOUR
#define MINUTE_OF_HOUR (60)
#endif
#ifndef SECOND_OF_MINUTE
#define SECOND_OF_MINUTE (60)
#endif

#define ONE_MS 1000ULL
#define MAIN_TASK_PERIOD 10000ULL
#define ONE_HOUR 3600000000ULL
#define ONE_MINUTE 60000000ULL
#define ONE_SECOND 1000000ULL
#define HALF_A_DAY 43200000000ULL
#define ONE_DAY 86400000000ULL

enum
{
    MAIN_TASK,
    SNTP_SYNC,
    USER_CLOCK,
    COUNTDOWN,
    LED_SHOW,
    NOTHING_TASK,
};

#if KEEP_STAMP
time_t GetUserTimestampISR(void);

time_t GetUserTimestamp(void);

void Set_User_timestamp(const time_t timeStamp);

#endif

QueueSetHandle_t GetMainEventQueueHandle(void);

uint8_t GetBellFlag(void);

void SetBellFlag(void);

void ResetBellFlag(void);

void User_Timer_Init(void);

void start_User_clock_timer(void);

esp_err_t restartOfRunning_UserClockTimer(void);

void stop_MainTask_timer(void);

void start_MainTask_timer(void);

#if (LED_DRIVER_VERSION_MAJOR != 0)
uint32_t two_point_state(void);
#else
bool two_point_state(void);
#endif


uint8_t ClockResetFlag(void);

void SetClockResetFlag(void);

esp_err_t startCountdownTimer(const uint64_t us);

esp_err_t stopwatch_Start(void);

esp_err_t stopwatch_Stop(void);

#if (LED_DRIVER_VERSION_MAJOR != 0)
/// @brief 获取秒表记录
/// @param
/// @return
uint32_t GetStopwatch_record(void);
#else
uint16_t GetStopwatch_record(void);
#endif


void SetStopwatch_record(void);

#if (LED_DRIVER_VERSION_MAJOR != 0)
/// @brief 获取时钟模式
/// @param
/// @return true: 秒表模式, false: 时钟模式
uint32_t GetClockMode(void);

/// @brief 设置时钟模式
/// @param
/// @return true: 秒表模式, false: 时钟模式
uint32_t SetClockMode(void);
#else
/// @brief 获取时钟模式
/// @param
/// @return true: 秒表模式, false: 时钟模式
bool GetClockMode(void);

bool SetClockMode(void);
#endif



bool CheckStopwatch(void);

esp_err_t target_time_timeout_close_led(void);

esp_err_t check_closeLedTimer_status(void);

esp_err_t delete_closeLedTimer(void);

#endif