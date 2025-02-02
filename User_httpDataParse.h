#pragma once
#ifndef _USER_HTTPDATAPARSE_H_
#define _USER_HTTPDATAPARSE_H_

#ifndef ENABLE
#define ENABLE 1
#endif // !ENABLE

#ifndef DISABLE
#define DISABLE 0
#endif // !DISABLE

#define HTTP_PARSE_DEBUG DISABLE

#ifndef HTTP_RECV_BUF_SIZE
#define HTTP_RECV_BUF_SIZE 512U
#endif // !HTTP_RECV_BUF_SIZE


#ifndef ENABLE_CLOCK
#define ENABLE_CLOCK ENABLE
#endif // !ENABLE_CLOCK

#ifndef ENABLE_WIFI
#define ENABLE_WIFI ENABLE
#endif // !ENABLE_WIFI

#define HTTPDATA_PARSE_OK 0
#define HTTPDATA_PARSE_FAIL (HTTPDATA_PARSE_OK - 1)
#define HTTPDATA_PARSE_ARG_ERR (HTTPDATA_PARSE_OK - 2)
#define HTTPDATA_PARSE_ALLOC_ERR (HTTPDATA_PARSE_OK - 3)
#define HTTPDATA_PARSE_LAST_MEM_NOT_FREE (HTTPDATA_PARSE_OK - 4)

typedef struct {
    char* label;
    char* data;
}HttpData;

#ifndef DATA_NAME

#if ENABLE_CLOCK
#define BELL_DAYS "bell_days"   // 每周需要铃响的日子
#define BELLS_TIME "bells_time" // 铃响时间
#define BELLS_INTERVALS "intervals_between_bells" // 循环铃响还是只铃响一次
typedef struct 
{
    unsigned char hour;
    unsigned char min;
    unsigned char bellDays; // |0000 0000| |null Sat Fri Thu Wed Tue Mon Sun |
    unsigned char intervals;
}BellInfo;
#endif

#if ENABLE_WIFI
#define WIFI_AP_STATUS "AP_status" // AP 状态
#define STA2AP_SSID "connectSSID" // 要连接的 SSID
#define STA2SP_PASSWD "connectPassword" // 要连接的 password
#define WIFI_AP_SSID "AP_SSID" // 设定 AP 的 SSID
#define WIFI_AP_PASSWD "AP_password" // 设定 AP 的 password
#define WIFI_AP_AUTH "AP_auth" // 设定 AP 的连接验证方式
#endif

#endif // !DATA_NAME

int GetLastParseError();

int HttpDataParse_clock(const char* data);

int HttpDataParse_wifiConnect(const char *data);

#if HTTP_PARSE_DEBUG

int HttpTest(const char* data);
#endif

BellInfo GetBellInfo();

char *GetWifiConnetInfo(const int type);

void HttpDataBufFree();

#endif