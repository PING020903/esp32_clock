#ifndef __USER_SNTP_H__
#define __USER_SNTP_H__

#define GET_TIME_FROM_NVS 0
#define GET_TIME_FROM_SNTP 1

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 48
#endif

#ifndef DAY_HOUR
#define DAY_HOUR (24)
#endif
#ifndef MINUTE_OF_HOUR
#define MINUTE_OF_HOUR (60)
#endif
#ifndef SECOND_OF_MINUTE
#define SECOND_OF_MINUTE (60)
#endif

typedef struct
{
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int ms;
} My_tm;

int GetDayOfWeek(int year, int month, int day);

time_t GetTimestamp(void);

void get_time_from_timer_v2(My_tm *my_tm);

uint64_t User_clock(void);

esp_err_t fetch_and_store_time_in_nvs(void *args);

esp_err_t update_time_from_sntp(void);

void User_SNTP_update_time(void);

/// @brief NTP 是否就绪
/// @param
/// @return OK:true 1; FAIL:false 0;
int UserNTP_ready(void);

cJSON *GetTimerInfo_JSON(void);

#endif
