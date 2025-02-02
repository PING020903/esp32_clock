#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "User_taskManagement.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "esp_log.h"
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "protocol_examples_utils.h"
#include "esp_tls_crypto.h"
#include <esp_http_server.h>
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"

#include <esp_wifi.h>
#include <esp_system.h>
#include "nvs_flash.h"
#include "User_NVSuse.h"
#include "esp_eth.h"
#include "dns_server.h"
#include "cJSON.h"
#include "User_WIFI.h"
#include "User_SNTP.h"
#include "User_timer.h"
#include "User_main.h"
#include "User_httpDataParse.h"
#include "OrderReady.h"
#include "CommandParse.h"
#include "User_httpServer.h"

#define HTTP_TEST 0
#define PARSE_REQUEST_HEAD 0
#define GET_INFO_DEBUG 0
#define RECE_DATA_DEBUG 0

#define CLOCK_OK "clock_OK"

#define HTTPSET_COMMAND "httpset"
#define HTTPGET_COMMAND "httpget"

#define STR_CLOCK "clock"
#define STR_INFODATA "infodata"
#define STR_WIFI_CONNECT "wifiConnect"

static const char *TAG = "HTTP_server";

// #define HTTP_END(REQ) httpd_resp_send_chunk(REQ, NULL, 0)
#define HTTP_END(REQ) httpd_resp_send(REQ, NULL, 0)
//#define POST_END(REQ) httpd_send(REQ, "" CLOCK_OK "", strlen(CLOCK_OK))
#define POST_END(REQ) httpd_resp_send(REQ, "" CLOCK_OK "", strlen(CLOCK_OK))
static httpd_handle_t server = NULL; // web服务句柄

extern char httpFavicon_start[] asm("_binary_favicon_ico_start");
extern char httpSelectHtml_start[] asm("_binary_select_html_start");
extern char httpServerHTML_start[] asm("_binary_server_html_start");
extern char httpClockHtml_start[] asm("_binary_clock_html_start");
extern char httpShowInfoHtml_start[] asm("_binary_showInfo_html_start");
extern char httpWIFI_connetHtml_start[] asm("_binary_WIFI_connect_html_start");

#if 0
const static char http_index_hml[] = "<!DOCTYPE html>"
                                     "<html>\n"
                                     "<head>\n"
                                     "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
                                     "  <style type=\"text/css\">\n"
                                     "    html, body, iframe { margin: 0; padding: 0; height: 100%; }\n"
                                     "    iframe { display: block; width: 100%; border: none; }\n"
                                     "  </style>\n"
                                     "<title>HELLO ESP32</title>\n"
                                     "</head>\n"
                                     "<body>\n"
                                     "<h1>Hello World, from ESP32!</h1>\n"
                                     "</body>\n"
                                     "</html>\n";
#endif

#if CONFIG_BASIC_AUTH
static char *http_auth_basic(const char *username, const char *password)
{
    size_t out;
    char *user_info = NULL;
    char *digest = NULL;
    size_t n = 0;
    int rc = asprintf(&user_info, "%s:%s", username, password);
    if (rc < 0)
    {
        ESP_LOGE(TAG, "asprintf() returned: %d", rc);
        return NULL;
    }

    if (!user_info)
    {
        ESP_LOGE(TAG, "No enough memory for user information");
        return NULL;
    }
    esp_crypto_base64_encode(NULL,
                             0,
                             &n,
                             (const unsigned char *)user_info,
                             strlen(user_info));

    /* 6: The length of the "Basic " string
     * n: Number of bytes for a base64 encode format
     * 1: Number of bytes for a reserved which be used to fill zero
     */
    digest = calloc(1, 6 + n + 1);
    if (digest)
    {
        strcpy(digest, "Basic ");
        esp_crypto_base64_encode((unsigned char *)digest + 6,
                                 n,
                                 &out,
                                 (const unsigned char *)user_info,
                                 strlen(user_info));
    }
    free(user_info);
    return digest;
}

/* An HTTP GET handler */
static esp_err_t basic_auth_get_handler(httpd_req_t *req)
{
    char *buf = NULL;
    size_t buf_len = 0;
    basic_auth_info_t *basic_auth_info = req->user_ctx;

    buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;
    if (buf_len > 1)
    {
        buf = calloc(1, buf_len);
        if (!buf)
        {
            ESP_LOGE(TAG, "No enough memory for basic authorization");
            return ESP_ERR_NO_MEM;
        }

        if (httpd_req_get_hdr_value_str(req, "Authorization", buf, buf_len) == ESP_OK)
        {
            ESP_LOGI(TAG, "Found header => Authorization: %s", buf);
        }
        else
        {
            ESP_LOGE(TAG, "No auth value received");
        }

        char *auth_credentials = http_auth_basic(basic_auth_info->username,
                                                 basic_auth_info->password);
        if (!auth_credentials)
        {
            ESP_LOGE(TAG, "No enough memory for basic authorization credentials");
            free(buf);
            return ESP_ERR_NO_MEM;
        }

        if (strncmp(auth_credentials, buf, buf_len))
        {
            ESP_LOGE(TAG, "Not authenticated");
            httpd_resp_set_status(req, HTTPD_401);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "keep-alive");
            httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Hello\"");
            httpd_resp_send(req, NULL, 0);
        }
        else
        {
            ESP_LOGI(TAG, "Authenticated!");
            char *basic_auth_resp = NULL;
            httpd_resp_set_status(req, HTTPD_200);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "keep-alive");
            int rc = asprintf(&basic_auth_resp,
                              "{\"authenticated\": true,\"user\": \"%s\"}",
                              basic_auth_info->username);
            if (rc < 0)
            {
                ESP_LOGE(TAG, "asprintf() returned: %d", rc);
                free(auth_credentials);
                return ESP_FAIL;
            }
            if (!basic_auth_resp)
            {
                ESP_LOGE(TAG, "No enough memory for basic authorization response");
                free(auth_credentials);
                free(buf);
                return ESP_ERR_NO_MEM;
            }
            httpd_resp_send(req, basic_auth_resp, strlen(basic_auth_resp));
            free(basic_auth_resp);
        }
        free(auth_credentials);
        free(buf);
    }
    else
    {
        ESP_LOGE(TAG, "No auth header received");
        httpd_resp_set_status(req, HTTPD_401);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Connection", "keep-alive");
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Hello\"");
        httpd_resp_send(req, NULL, 0);
    }

    return ESP_OK;
}
#endif

typedef esp_err_t (*httpd_send_func)(httpd_req_t *r, const char *buf, size_t len);
typedef esp_err_t (*httpd_data_handle)(httpd_req_t *r, const char *data, const char* funcDesc, int R_W);

static unsigned char OrderStatus = ORDER_EXIT;
static unsigned char OrderDataStatus = ORDER_DATA_INVALID;
static char FuncDescStr[64] = {0};

/// @brief HTTP_用户数据处理结构体
typedef struct
{
    httpd_req_t *req;
    const char *data;
}HttpUserDataStruct;


/**
 * @brief 解析指令
 * @param str
 * @return
 * @note POST的返回值应当在POST_HANDLE中返回
 */
static int OrderParse(httpd_req_t *r, const char *str, httpd_data_handle handle)
{
    char *pos = NULL;
    if (str == NULL || handle == NULL)
        return Order_ERR_INVALID_ARG;

    if (OrderStatus == ORDER_READY)
        goto HANDLE_FUNC;

    pos = strstr(str, READY_STR);
    if (pos != str) // 若正常找到 Ready, 那么 pos==str
    {
        // OrderStatus = ORDER_EXIT;
        printf("order is not right\"%s\"\n", str);
        return Order_ERR_PARSE;
    }
    OrderStatus = ORDER_READY;

    pos = strstr(str, RECV_STR);
    if (pos) // 设置接收模式
    {
        OrderDataStatus = ORDER_DATA_RECV;
        pos += strlen(RECV_STR) + 1;
        memcpy(FuncDescStr, pos, strlen(pos)); // 保存指令功能描述
        return Order_OK;
    }

    pos = strstr(str, SEND_STR);
    if (pos) // 设置发送模式
    {
        OrderDataStatus = ORDER_DATA_SEND;
        pos += strlen(SEND_STR) + 1;
        memcpy(FuncDescStr, pos, strlen(pos)); // 保存指令功能描述
        return Order_OK;
    }

    OrderDataStatus = ORDER_DATA_INVALID; // 不符合接收或发送指令
    printf("order not find R/W\n");
    return Order_ERR_PARSE;

HANDLE_FUNC:
    if (OrderDataStatus == ORDER_DATA_INVALID)
        return Order_ERR_FAIL;

    if (strcmp(str, EXIT_STR) == 0) // 检测退出指令
    {
        OrderStatus = ORDER_EXIT;
        OrderDataStatus = ORDER_DATA_INVALID;
        memset(FuncDescStr, 0, sizeof(FuncDescStr));    // 清除指令功能描述
        return Order_OK;
    }

    handle(r, str, FuncDescStr, OrderDataStatus); // 处理
    return Order_OK;
}

/* An HTTP GET handler */
static esp_err_t MainHtmlHandle(httpd_req_t *req)
{
    esp_err_t err;

#if HTTP_TEST
    httpd_resp_send(req, httpServerHTML_start, HTTPD_RESP_USE_STRLEN);
#else
    // httpd_resp_send(req, httpClockHtml_start, HTTPD_RESP_USE_STRLEN);'
    err = httpd_resp_send(req, httpSelectHtml_start, HTTPD_RESP_USE_STRLEN);
#endif
    ESP_LOGI(TAG, "httpd_resp_send %s", (err) ? "!= ESP_OK" : "== ESP_OK");

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0)
    {
        ESP_LOGI(TAG, "Request headers lost");
    }
    err = HTTP_END(req);
    ESP_LOGI(__func__, "HTTP_END ret:%s", esp_err_to_name(err));
    return err;
}

static esp_err_t clock_html(httpd_req_t *req)
{
    esp_err_t err = ESP_OK;
    httpd_resp_send(req, httpClockHtml_start, HTTPD_RESP_USE_STRLEN);
    err = HTTP_END(req);
    ESP_LOGI(__func__, "HTTP_END ret:%s", esp_err_to_name(err));
    return err;
}


static void httpset_clock(void* arg)
{
    HttpUserDataStruct *data = (HttpUserDataStruct *)arg;
    if(data == NULL)
    {
        ESP_LOGW(TAG, "%s data is NULL", __func__);
        return;
    }

    HttpDataParse_clock(data->data); // 解析数据
    if (HTTPDATA_PARSE_OK == GetLastParseError())
    {
        // 解析数据成功则打印
        ESP_LOGI(TAG, "bell days:%d, intervals:%d, time:%d:%d",
                 GetBellInfo().bellDays, GetBellInfo().intervals,
                 GetBellInfo().hour, GetBellInfo().min);
        Write_BellDays(GetBellInfo().bellDays);
        SetTargetTime(GetBellInfo().hour, GetBellInfo().min, 0);
        restartOfRunning_UserClockTimer();
    }
    else
        ESP_LOGI(TAG, "HttpDataParse_clock != OK (VALUE=%d)", GetLastParseError());
}
static void httpset_wificonnect(void* arg)
{
    HttpUserDataStruct *data = (HttpUserDataStruct *)arg;
    if(data == NULL)
    {
        ESP_LOGW(TAG, "%s data is NULL", __func__);
        return;
    }

    HttpDataParse_wifiConnect(data->data);
    if (HTTPDATA_PARSE_OK == GetLastParseError())
    {
        ESP_LOGI(TAG, "set wifi %s", (SetConnectionInfo(GetWifiConnetInfo(0), GetWifiConnetInfo(1), 3)) ? "fail" : "success");
        vTaskResume(User_GetTaskHandle(HTTP_WIFI_CONNECT_TASK_NAME));
    }
    else
        ESP_LOGI(TAG, "HttpDataParse_wifiConnect != OK (VALUE=%d)", GetLastParseError());
}
static void httpget_infodata(void *arg)
{
    HttpUserDataStruct *data = (HttpUserDataStruct *)arg;
    char *pos = NULL;
    cJSON *obj = NULL;
    cJSON *baseInfo = NULL, *wifiInfo = NULL, *timerInfo = NULL;
    if(data == NULL)
    {
        ESP_LOGW(TAG, "%s data is NULL", __func__);
        return;
    }

    baseInfo = GetBoardInfo_JSON();
    wifiInfo = GetWifiInfo_JSON();
    timerInfo = GetTimerInfo_JSON();

    obj = cJSON_CreateObject();
    cJSON_AddItemToObject(obj, "base_info_table", baseInfo);
    cJSON_AddItemToObject(obj, "wifi_info_table", wifiInfo);
    cJSON_AddItemToObject(obj, "timer_info_table", timerInfo);

    pos = cJSON_Print(obj);
#if GET_INFO_DEBUG
    printf("%s\n", pos);
#endif
    // httpd_resp_send_chunk(data->req, pos, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send(data->req, pos, HTTPD_RESP_USE_STRLEN);
    cJSON_Delete(obj);
    free(pos);
}
static esp_err_t UserDataHandle(httpd_req_t *req, const char* data, const char* funcDesc, int R_W)
{
    const HttpUserDataStruct handle = {
        .req = req,
        .data = data};
    ;
    ESP_LOGI(TAG, "%s %s", __func__, (CommandParse_JSON_data(funcDesc, (User_httpd_data*)&handle)) ? "FAIL" : "OK");
    return ESP_OK;
}
static esp_err_t SetBell_handler(httpd_req_t *req)
{
    char buf[HTTP_RECV_BUF_SIZE] = {0};
    int ret, remaining = req->content_len;

    while (remaining > 0)
    {
        /* Read the data for the request */
        ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));
        if (ret < 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL; // 因接收超时导致错误, 一般情况无需理会, 返回FAIL后socket会自动关闭
        }
#if RECE_DATA_DEBUG
        /* Log data received */
        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
        ESP_LOGI(TAG, "url: %s", req->uri);
        ESP_LOGI(TAG, "%.*s", ret, buf);
        ESP_LOGI(TAG, "====================================");
#endif
        OrderParse(req, buf, UserDataHandle);
        remaining -= ret;
    }
    
    return (POST_END(req)) ? ESP_FAIL : ESP_OK;
}

static esp_err_t WIFI_connect_handler(httpd_req_t *req)
{

    httpd_resp_send(req, httpWIFI_connetHtml_start, HTTPD_RESP_USE_STRLEN);
    return HTTP_END(req);
}
static esp_err_t ShowBoardInfo_handler(httpd_req_t *req)
{
    httpd_resp_send(req, httpShowInfoHtml_start, HTTPD_RESP_USE_STRLEN);
    return HTTP_END(req);
}
static esp_err_t favicon_handler(httpd_req_t *req)
{
    // esp_err_t err = httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "'favicon.ico' has no sources");
    esp_err_t err = HTTP_END(req);
    ESP_LOGI(__func__, "httpd_resp_send_err ret: %s", esp_err_to_name(err));
    return err;
    // httpd_resp_send(req, httpFavicon_start, HTTPD_RESP_USE_STRLEN);
    // return httpd_resp_send_404(req);
    // return HTTP_END(req);
}

static const httpd_uri_t mainHtml = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = MainHtmlHandle,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx = "Hello World!"};
static const httpd_uri_t setBellHtml = {
    .uri = "/setBell",
    .method = HTTP_GET,
    .handler = clock_html,
    .user_ctx = "NULL"};
static const httpd_uri_t setBellEcho = {
    .uri = "/setBellEcho",
    .method = HTTP_POST,
    .handler = SetBell_handler,
    .user_ctx = "NULL"};
static const httpd_uri_t WIFI_connect = {
    .uri = "/wifiConnect",
    .method = HTTP_GET,
    .handler = WIFI_connect_handler,
    .user_ctx = "NULL"};
static const httpd_uri_t favicon = {
    .uri = "/favicon.ico",
    .method = HTTP_GET,
    .handler = favicon_handler,
    .user_ctx = "NULL"};
static const httpd_uri_t ShowBoardInfo = {
    .uri = "/showInfo",
    .method = HTTP_GET,
    .handler = ShowBoardInfo_handler,
    .user_ctx = "NULL"};


#if CONFIG_BASIC_AUTH
static httpd_uri_t basic_auth = {
    .uri = "/basic_auth",
    .method = HTTP_GET,
    .handler = basic_auth_get_handler,
};

static void httpd_register_basic_auth(httpd_handle_t server)
{
    basic_auth_info_t *basic_auth_info = calloc(1, sizeof(basic_auth_info_t));
    if (basic_auth_info)
    {
        basic_auth_info->username = CONFIG_BASIC_AUTH_USERNAME;
        basic_auth_info->password = CONFIG_BASIC_AUTH_PASSWORD;

        basic_auth.user_ctx = basic_auth_info;
        httpd_register_uri_handler(server, &basic_auth);
    }
}
#endif

static esp_err_t My_error_hanlder(httpd_req_t *req, httpd_err_code_t error)
{
    esp_err_t err = ESP_OK;
    ESP_LOGW(TAG, "func:%s receive error code %d", __func__, error);
    switch (error)
    {
    case HTTPD_431_REQ_HDR_FIELDS_TOO_LARGE:
        printf("%s\n", req->uri);
        break;
    case HTTPD_404_NOT_FOUND:
    {
        printf("%s\n", req->uri);
    }
    break;

    default:
        break;
    }
    return err;
}

/// @brief 开始web服务
/// @param
/// @return
static httpd_handle_t start_webserver(void)
{
    server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    command_node *node = NULL;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Registering the ws handler
        ESP_LOGI(TAG, "Registering URI handlers");

        httpd_register_uri_handler(server, &mainHtml);
        ESP_LOGI(TAG, "Registering %s", mainHtml.uri);

        httpd_register_uri_handler(server, &WIFI_connect);
        ESP_LOGI(TAG, "Registering %s", WIFI_connect.uri);

        httpd_register_uri_handler(server, &setBellHtml);
        ESP_LOGI(TAG, "Registering %s", setBellHtml.uri);

        httpd_register_uri_handler(server, &ShowBoardInfo);
        ESP_LOGI(TAG, "Registering %s", ShowBoardInfo.uri);

        httpd_register_uri_handler(server, &setBellEcho);
        ESP_LOGI(TAG, "Registering %s", setBellEcho.uri);

        httpd_register_uri_handler(server, &favicon);
        ESP_LOGI(TAG, "Registering %s", favicon.uri);

        /*将默认错误处理重定向*/
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, My_error_hanlder);
        httpd_register_err_handler(server, HTTPD_431_REQ_HDR_FIELDS_TOO_LARGE,
                                   My_error_hanlder);
#if CONFIG_BASIC_AUTH
        httpd_register_basic_auth(server);
#endif

        RegisterCommand(false, HTTPSET_COMMAND);
        node = FindCommand(HTTPSET_COMMAND, NULL);
        RegisterParameter(node, httpset_clock, false, STR_CLOCK);
        RegisterParameter(node, httpset_wificonnect, false, STR_WIFI_CONNECT);

        RegisterCommand(false, HTTPGET_COMMAND);
        node = FindCommand(HTTPGET_COMMAND, NULL);
        RegisterParameter(node, httpget_infodata, false, STR_INFODATA);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

/// @brief 停止web服务
/// @param server
/// @return
static esp_err_t stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    esp_err_t err = httpd_stop(server);
    switch (err)
    {
    case ESP_OK:
        unRegisterCommand(HTTPGET_COMMAND, NULL);
        unRegisterCommand(HTTPSET_COMMAND, NULL);
        return ESP_OK;
    case ESP_ERR_INVALID_ARG:
        ESP_LOGE(TAG, "%s INVALID_ARG", __func__);
        return ESP_ERR_INVALID_ARG;
    case ESP_FAIL:
        ESP_LOGE(TAG, "%s shut down error", __func__);
        return ESP_FAIL;
    default:
        ESP_LOGE(TAG, "%s unknow error", __func__);
        return err;
    }
    return err;
}

/// @brief 断开连接处理
/// @param
void WebServerDisconnect_handler(void)
{

    if (server)
    {
        if (stop_webserver(server) == ESP_OK)
        {
            server = NULL;
            // stop_dns_server();
            ESP_LOGI(TAG, "Stop https server success");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to stop https server");
        }
    }
    else
        ESP_LOGW(TAG, "server is NULL");
}

/// @brief 连接处理
/// @param
void WebServerConnect_handler(void)
{
    if (server == NULL)
    {
        server = start_webserver();
        // start_dns_server();
    }
    else
        ESP_LOGW(TAG, "server is on");
}