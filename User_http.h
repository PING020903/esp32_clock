#ifndef _USER_HTTP_H_
#define _USER_HTTP_H_

#define HTTP_QUERY_KEY_MAX_LEN (64)
#define HTTPD_401 "401 UNAUTHORIZED" // 401错误响应
#define CONFIG_BASIC_AUTH_USERNAME "ESP32Clock_WebServer"
#define CONFIG_BASIC_AUTH_PASSWORD ""

#define CONFIG_BASIC_AUTH 1
#if CONFIG_BASIC_AUTH
/// @brief 基础认证信息
typedef struct
{
    char *username;
    char *password;
} basic_auth_info_t;
#endif

#define MIN(x, y) ({ typeof(x) _x = (x); \
typeof(y) _y = (y); \
(void) (&_x == &_y); \
_x < _y ?_x : _y; })
#define MAX(x, y) ({ typeof(x) _x = (x); \
typeof(y) _y = (y); \
(void) (&_x == &_y); \
_x > _y ?_x : _y; })

void disconnect_handler(void);
void connect_handler(void);
#endif