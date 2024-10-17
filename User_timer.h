#ifndef __USER_TIMER_H__
#define __USER_TIMER_H__

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
    NOTHING_TASK,
};

QueueSetHandle_t GetMainEventQueueHandle(void);

uint8_t GetBellFlag(void);

void SetBellFlag(void);

void ResetBellFlag(void);

void User_Timer_Init(void);

esp_err_t restartOfRunning_UserClockTimer(void);

void start_User_clock_timer(void);

void stop_MainTask_timer(void);

void start_MainTask_timer(void);

uint8_t two_point_state(void);

uint8_t ClockResetFlag(void);

void SetClockResetFlag(void);

esp_err_t startCountdownTimer(const uint64_t us);

esp_err_t stopwatch_Start(void);

esp_err_t stopwatch_Stop(void);

uint16_t GetStopwatch_record(void);

void SetStopwatch_record(void);

bool GetClockMode(void);

bool SetClockMode(void);

bool CheckStopwatch(void);

#endif