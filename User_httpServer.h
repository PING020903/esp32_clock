#ifndef _USER_HTTPSERVER_H_
#define _USER_HTTPSERVER_H_

#define HTTP_QUERY_KEY_MAX_LEN (64)
#define HTTPD_401 "401 UNAUTHORIZED" // 401错误响应
#define CONFIG_BASIC_AUTH_USERNAME "ESP32Clock_WebServer"
#define CONFIG_BASIC_AUTH_PASSWORD ""

#define CONFIG_BASIC_AUTH 0
#if CONFIG_BASIC_AUTH
/// @brief 基础认证信息
typedef struct
{
    char *username;
    char *password;
} basic_auth_info_t;
#endif

#ifndef MIN
#define MIN(x, y) ({    typeof(x) _x = (x); \
                        typeof(y) _y = (y); \
                        (void) (&_x == &_y); \
                        _x < _y ?_x : _y; })
#endif
#ifndef MAX
#define MAX(x, y) ({    typeof(x) _x = (x); \
                        typeof(y) _y = (y); \
                        (void) (&_x == &_y); \
                        _x > _y ?_x : _y; })
#endif


#ifndef WEB_CJSON_STRUCT_DEFINE
#define WEB_CHIP_MODEL "chip_model"
#define WEB_FLASH_SIZE "flash_size"
#define WEB_CORE_FREQ "frequency"
#define WEB_CHIP_WIFI "wifi"
#define WEB_CHIP_BT "bluetooth"

#define WEB_WIFI_MODE "wifi_mode"
#define WEB_WIFI_SSID "sta_ssid"
#define WEB_WIFI_IP "local_ip_address"
#define WEB_LOCAL_AP_SSID "ap_ssid"
#define WEB_LOCAL_AP_PASSWORD "ap_password"

#define WEB_TIMER_NUMBER "Timer number"
#define WEB_TARGET_TIME "time"
#define WEB_BELL_DAYS "days"
#define WEB_TIMER_MODE "mode"
#endif

void WebServerDisconnect_handler(void);
void WebServerConnect_handler(void);
#endif
