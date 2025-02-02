#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "User_taskManagement.h"

#include "esp_system.h"
#include "esp_wifi.h"  // wifi相关头文件
#include "esp_event.h" // wifi事件头文件
#include "esp_eth.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_check.h"
#include "esp_netif.h"
#include "esp_tls_crypto.h"
// #include "esp_https_server.h"

#include "cJSON.h"
#include "nvs_flash.h" // nvs相关头文件
#include "User_NVSuse.h"
#include "User_sntp.h"
#include "User_httpServer.h"
#include "User_MQTT.h"
#include "User_WIFI.h"

#define ENABLE_AP_DEBUG 0
#define MY_NAPT 1
#define CHANNEL_MAX 13

#define WIFI_MODE_CHENGE_WAIT() vTaskDelay(200 / portTICK_PERIOD_MS)
#define WIFIERRORPARSE(err) UserWifiErrorParse(err, __FILE__, __LINE__)
#define USER_WIFI_ERR_CHECK(err) \
    if (WIFIERRORPARSE(err))     \
    return err

static const char *TAG = "User_WIFI";
static const char *wifiModeStrMap[] = {
    "wifi mode is NULL",
    "wifi mode is station",
    "wifi mode is softAP",
    "wifi mode is station and softAP",
    "wifi mode is NAN",
    "unknown wifi mode"};

static wifi_mode_t record_mode = WIFI_MODE_NULL;
static EventGroupHandle_t wifi_event_group; // wifi事件组
static int s_retry_num = 0;                 // 重试次数
static esp_netif_t *esp_netif_sta = NULL;   // WIFI_station句柄
static esp_netif_t *esp_netif_ap = NULL;    // WIFI_AP句柄

static esp_event_handler_instance_t instance_any_id; // 事件处理句柄
static esp_event_handler_instance_t instance_got_ip;
// static esp_event_handler_instance_t instance_control_clock;

static esp_err_t UserWifiErrorParse(const esp_err_t err, const char *file, const int line)
{
    switch (err)
    {
    case ESP_OK:
        return err;
    case ESP_ERR_WIFI_NOT_INIT:
        ESP_LOGE(TAG, "<%s><line:%d> wifi is not initialized...", file, line);
        break;
    case ESP_ERR_WIFI_NOT_STARTED:
        ESP_LOGW(TAG, "<%s><line:%d> wifi is not started...", file, line);
        break;
    case ESP_ERR_WIFI_IF:
        ESP_LOGE(TAG, "<%s><line:%d>\
 wifi invalid interface or error occured ...",
                 file, line);
        break;
    case ESP_ERR_WIFI_STATE:
        ESP_LOGE(TAG, "<%s><line:%d>\
 wifi still connecting when invoke esp_wifi_scan_start...",
                 file, line);
        break;
    case ESP_ERR_WIFI_CONN:
        ESP_LOGW(TAG, "<%s><line:%d>\
 WiFi internal error, station or soft-AP control block wrong",
                 file, line);
        break;
    case ESP_ERR_WIFI_NVS:
        ESP_LOGW(TAG, "<%s><line:%d>\
 WiFi internal NVS error",
                 file, line);
        break;
    case ESP_ERR_WIFI_SSID:
        ESP_LOGW(TAG, "<%s><line:%d>\
 SSID of AP which station connects is invalid...",
                 file, line);
        break;
    case ESP_ERR_WIFI_PASSWORD:
        ESP_LOGW(TAG, "<%s><line:%d>\
  invalid password...",
                 file, line);
        break;
    case ESP_ERR_WIFI_TIMEOUT:
        ESP_LOGE(TAG, "<%s><line:%d>\
 blocking scan is timeout...",
                 file, line);
        break;
    case ESP_ERR_WIFI_NOT_CONNECT:
        ESP_LOGW(TAG, "<%s><line:%d>\
 The station is in disconnect status",
                 file, line);
        break;
    default:
        ESP_LOGE(TAG, "<%s><line:%d>\
 unknown error...(%s)err=0x%x",
                 file, line,
                 esp_err_to_name(err), err);
        break;
    }
    return err;
}

/// @brief 获取wifi模式字符串
/// @param mode
/// @return
static const char *GetWifiModeStr(const wifi_mode_t mode)
{
    switch (mode)
    {
    case WIFI_MODE_NULL:
    case WIFI_MODE_STA:
    case WIFI_MODE_AP:
    case WIFI_MODE_APSTA:
    case WIFI_MODE_NAN:
        return wifiModeStrMap[mode];
    default:
        return wifiModeStrMap[5];
    }
    return NULL;
}

/**
 * @brief  wifi事件处理函数
 * @param   *arg        传入的参数
 * @param   event_base  基础事件类型
 * @param   event_id    事件ID
 * @param   event_data  事件数据
 * @note
 * @return
 *
 */
static void UserWIFI_event_handler(void *arg,
                                   esp_event_base_t event_base,
                                   int32_t event_id,
                                   void *event_data)
{
    static const char *EVENT_TAG = "User_WIFI_event";
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
        {
            ESP_LOGI(EVENT_TAG, "STA START...ssid:%s, passwrod:%s",
                     GetConnectWIFI_SSID(), GetConnectWIFI_PASSWD());
            WIFIERRORPARSE(esp_wifi_connect());
        }
        break;
        case WIFI_EVENT_STA_CONNECTED:
        {
            ESP_LOGI(EVENT_TAG, "WIFI_EVENT_STA_CONNECTED ! ! !");
        }
        break;
        case WIFI_EVENT_STA_DISCONNECTED:
        {

            if (s_retry_num++ < ESP_MAXIMUM_RETRY) // 尝试重连
            {
                vTaskDelay(8); // 模式已变换, 传到状态给API需要时间, 等待一下, 最少约8ms
                WIFIERRORPARSE(esp_wifi_get_mode(&record_mode));
                if (record_mode == WIFI_MODE_AP ||
                    record_mode == WIFI_MODE_NULL) // station is disabled
                    goto __EXIT_RETRY_CONNECT;
                ESP_LOGW(EVENT_TAG, "retry(%d) to connect to the AP, mode:%s",
                         s_retry_num, GetWifiModeStr(record_mode));
                ESP_LOGI(EVENT_TAG, "STA RETRY...ssid:%s, passwrod:%s",
                         GetConnectWIFI_SSID(), GetConnectWIFI_PASSWD());
                WIFIERRORPARSE(esp_wifi_connect());
            }
            else
            {
                xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT); // 连接失败
                User_WIFI_enable_AP(NULL, NULL, 0);
                ESP_LOGW(EVENT_TAG, "connect to the AP fail, open AP");
            }
        __EXIT_RETRY_CONNECT:
        }
        break;
        case WIFI_EVENT_SCAN_DONE:
        {
            ESP_LOGI(EVENT_TAG, "wifi scan done");
        }
        break;
        case WIFI_EVENT_STA_BEACON_TIMEOUT:
        {
            ESP_LOGE(EVENT_TAG, "wifi timeout");
        }
        break;
        case WIFI_EVENT_STA_AUTHMODE_CHANGE:
        {
            ESP_LOGI(EVENT_TAG, "wifi auth mode change");
        }
        break;
        case WIFI_EVENT_AP_START:
        {
            ESP_LOGI(EVENT_TAG, "Soft-AP start ! ! !");
        }
        break;
        case WIFI_EVENT_AP_STOP:
        {
            ESP_LOGI(EVENT_TAG, "Soft-AP stop ! ! !");
        }
        break;
        case WIFI_EVENT_AP_STACONNECTED:
        {
            ESP_LOGI(EVENT_TAG, "a station connected to Soft-AP");
            printf("Minimum free heap size: %" PRIu32 " bytes\n",
                   esp_get_minimum_free_heap_size());
            // connect_handler();
        }
        break;
        case WIFI_EVENT_AP_STADISCONNECTED:
        {
            ESP_LOGI(EVENT_TAG, "a station disconnected from Soft-AP");
            printf("Minimum free heap size: %" PRIu32 " bytes\n",
                   esp_get_minimum_free_heap_size());
            // disconnect_handler();
        }
        break;
        case WIFI_EVENT_AP_PROBEREQRECVED:
        {
            ESP_LOGI(EVENT_TAG, "Receive probe request packet in soft-AP interface");
        }
        break;
        default:
            break;
        }
    }
    else if (event_base == IP_EVENT)
    {
        switch (event_id)
        {
        case IP_EVENT_STA_GOT_IP:
        {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(EVENT_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT); // 连接成功
            s_retry_num = 0;
        }
        break;
        case IP_EVENT_STA_LOST_IP:
        {
            ESP_LOGI(EVENT_TAG, "wifi event: sta lost IP...");
        }
        break;
        case IP_EVENT_ETH_GOT_IP:
        {
            ESP_LOGI(EVENT_TAG, "ETH got ip");
        }
        break;

        default:
            break;
        }
    }
#if 0
    else if (event_base == ESP_HTTP_SERVER_EVENT)
    {
        switch (event_id)
        {
        case HTTP_SERVER_EVENT_ERROR:
        {
            esp_tls_error_handle_t last_error = (esp_tls_last_error_t *)event_data;
            ESP_LOGE(TAG, "Error event triggered: last_error = %s, last_tls_err = %d, tls_flag = %d",
                     esp_err_to_name(last_error->last_error),
                     last_error->esp_tls_error_code,
                     last_error->esp_tls_flags);
        }
        break;
        case HTTP_SERVER_EVENT_START:
        {
            ESP_LOGI(TAG, "http server start");
        }
        break;
        case HTTP_SERVER_EVENT_ON_CONNECTED:
        {
            // ESP_LOGW(TAG, "Once the HTTP Server has been connected to the client");
        }
        break;
        case HTTP_SERVER_EVENT_DISCONNECTED:
        {
            // ESP_LOGW(TAG, "The connection has been disconnected");
        }
        break;
        }
    }
#endif
    else if (event_base == ETH_EVENT)
    {
        switch (event_id)
        {
        case ETHERNET_EVENT_START:
            break;
        case ETHERNET_EVENT_STOP:
            break;
        case ETHERNET_EVENT_CONNECTED:
            break;
        case ETHERNET_EVENT_DISCONNECTED:
            break;

        default:
            break;
        }
    }
    return;
}

static esp_err_t Show_Wifi_station_info(void)
{
    const char *tag = "WiFi_sta";
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);
    uint8_t protocol_bitmap_temp;
    wifi_bandwidth_t current_bandwidth = WIFI_BW_HT20;
    esp_netif_ip_info_t ipInfo = {0};
    wifi_ap_record_t ap_info = {0}; // 连接的AP信息
    esp_err_t err = ESP_OK;
    const char *bandwithStr[] = {
        "unknown MHz",
        "20MHz",
        "40MHz"};

    if (bits & WIFI_CONNECTED_BIT)
    {
        /* 当支持802.11n时候, 才支持40MHz频段带宽 */
        err = esp_wifi_get_protocol(WIFI_IF_STA, &protocol_bitmap_temp);
        USER_WIFI_ERR_CHECK(err);

        err = esp_wifi_sta_get_ap_info(&ap_info);
        USER_WIFI_ERR_CHECK(err);

        err = esp_wifi_get_bandwidth(WIFI_IF_STA, &current_bandwidth);
        USER_WIFI_ERR_CHECK(err);

        err = esp_netif_get_ip_info(esp_netif_sta, &ipInfo);
        USER_WIFI_ERR_CHECK(err);

        ESP_LOGI(tag, "connected to ap SSID:%s, AP_MAC:" MACSTR ", bitmap:0x%x, bandwidth:%s ",
                 ap_info.ssid, MAC2STR(ap_info.bssid),
                 protocol_bitmap_temp, bandwithStr[current_bandwidth]);
        ESP_LOGI(tag, "local_IP:" IPSTR ", AP_GW:" IPSTR ", AP_NETMASK:" IPSTR "",
                 IP2STR(&ipInfo.ip), IP2STR(&ipInfo.gw), IP2STR(&ipInfo.netmask));

        s_retry_num = 0; // 正常连接, 重置重连次数
        return err;
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(tag, "Failed to connect to SSID:%s, password:%s",
                 GetConnectWIFI_SSID(), GetConnectWIFI_PASSWD());
        return ESP_FAIL;
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT ! ! !");
    }
    return ESP_FAIL;
}

static esp_err_t Show_Wifi_SoftAp_info(void)
{
    const char *tag = "Wifi_SoftAp";
    esp_err_t err = ESP_OK;
    esp_netif_ip_info_t ipInfo = {0};
    wifi_config_t softAp_info; // 本地软AP信息

    err = esp_wifi_get_config(WIFI_IF_AP, &softAp_info);
    USER_WIFI_ERR_CHECK(err);

    err = esp_netif_get_ip_info(esp_netif_ap, &ipInfo);
    USER_WIFI_ERR_CHECK(err);

    ESP_LOGI(tag, "softAP SSID:%s, password:%s, channel:%d, auth:%d, \
ip:" IPSTR ", gw:" IPSTR "",
             softAp_info.ap.ssid, softAp_info.ap.password,
             softAp_info.ap.channel, softAp_info.ap.authmode,
             IP2STR(&ipInfo.ip), IP2STR(&ipInfo.gw));
    return err;
}

/// @brief 显示WIFI连接信息
/// @param
/// @return OK: ESP_OK, ERROR: ESP_FAIL
esp_err_t Show_Wifi_Info(void)
{

    esp_err_t err = esp_wifi_get_mode(&record_mode);
    ESP_LOGI(__func__, "WIFI mode: %s", GetWifiModeStr(record_mode));
    switch (err)
    {
    case ESP_OK:
    {
        switch (record_mode)
        {
        case WIFI_MODE_NULL:
        {
            ESP_LOGW(TAG, "%s, no need to show wifi info", GetWifiModeStr(record_mode));
        }
        break;
        case WIFI_MODE_STA:
        {
            err = Show_Wifi_station_info();
            ESP_LOGW(TAG, "%s, no need to show softAP info", GetWifiModeStr(record_mode));
        }
        break;
        case WIFI_MODE_AP:
        {
            err = Show_Wifi_SoftAp_info();
            ESP_LOGW(TAG, "%s, no need to show station info", GetWifiModeStr(record_mode));
        }
        break;
        case WIFI_MODE_APSTA:
        {
            err = Show_Wifi_station_info();
            err = Show_Wifi_SoftAp_info();
        }
        break;
        case WIFI_MODE_NAN:
        {
            ESP_LOGW(TAG, "%s,\
current mode status display is not supported for the time being",
                     GetWifiModeStr(record_mode));
        }
        break;
        default:
            ESP_LOGW(__func__, "unknown wifi mode");
            break;
        }
    }
    break;
    case ESP_ERR_INVALID_ARG:
        ESP_LOGE(__func__, "esp_wifi_get_mode: Invalid argument");
        break;
    default:
        ESP_LOGW(__func__, "wifi is not initialized...");
        break;
    }

    return err;
}

static void httpWifiConnect(void *arg)
{
    while (1)
    {
        vTaskDelay(1000);
        User_WIFI_changeConnection_sta(); // 更改连接
        vTaskSuspend(NULL);               // 随后挂起
    }
    vTaskDelete(NULL);
}

static void UserEnableAP(void *arg)
{
    while (1)
    {
        vTaskDelay(1000);
        User_WIFI_enable_AP(NULL, NULL, 0);
        vTaskSuspend(NULL);
    }
    vTaskDelete(NULL);
}

/**
 * @brief  用户WIFI初始化
 * @note
 * @return
 *
 */
void User_WIFI_Init(void)
{
    const char *deInitStr = "no memory create event loop! ! !\n  deinit NVS";
    wifi_event_group = xEventGroupCreate(); // 创建事件组

    if (Read_WIFIconnectionInfo_sta())
    {
        SetDefaultConnetionInfo();
        esp_err_t err = Write_WIFIconnectionInfo_sta();
        if (err)
        {
            ESP_LOGW(TAG, "Write_WIFIconnectionInfo_sta failed %s", esp_err_to_name(err));
            EraseWIFIconnetionInfoFromNVS();
        }
    }
    else
        ESP_LOGI(TAG, "read sta info from NVS success");

    // 初始化网络层, 首次初始化成功后不可 deinit
    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t event_ret = esp_event_loop_create_default(); // 创建默认事件循环
event_loop_check:
    switch (event_ret)
    {
    case ESP_OK:
        break;

    case ESP_ERR_INVALID_STATE:
        ESP_LOGW(TAG, "Default event loop has already been created, will recreate event loop");
        ESP_ERROR_CHECK(esp_event_loop_delete_default());
        event_ret = esp_event_loop_create_default();
        goto event_loop_check;

    case ESP_ERR_NO_MEM:
        ESP_LOGE(TAG, "no memory create event loop! ! !\n  deinit NVS");
        ESP_ERROR_CHECK(nvs_flash_deinit());
        return;

    case ESP_FAIL:
        ESP_LOGE(TAG, "%s", deInitStr);
        ESP_ERROR_CHECK(nvs_flash_deinit());
        return;

    default:
        ESP_LOGE(TAG, "%s", deInitStr);
        ESP_ERROR_CHECK(nvs_flash_deinit());
        return;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); // 初始化配置, 使用宏将WiFi硬件配置初始化
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));                // 初始化 wifi
#if WIFI_AP_ENABLE && WIFI_STA_ENABLE
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
#endif
#if WIFI_STA_ENABLE
    esp_netif_sta = esp_netif_create_default_wifi_sta(); // 创建 wifi STA 默认的网络接口

    wifi_config_t wifi_config = {
        .sta = {
            /*
如果密码与 WPA2 标准匹配，则 Authmode 阈值将重置为 WPA2 默认值 （pasword len => 8）。
* 如果要将设备连接到已弃用的 WEP/WPA 网络，请设置阈值
* 设置为 WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK，并将长度和格式匹配的密码设置为
* WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK标准。
*/
            .threshold.authmode = GetConnectWIFI_AuthType(),
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        },
    };
    strcpy((char *)wifi_config.sta.ssid, GetConnectWIFI_SSID());
    strcpy((char *)wifi_config.sta.password, GetConnectWIFI_PASSWD());

#if !WIFI_AP_ENABLE && WIFI_STA_ENABLE
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // 设置模式为 station
    ESP_ERROR_CHECK(esp_wifi_get_mode(&record_mode));
#endif
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config)); // 设置配置
#endif
#if WIFI_AP_ENABLE
    esp_netif_ap = esp_netif_create_default_wifi_ap(); // 创建 wifi AP 默认的网络接口

    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = ESP_WIFI_AP_SSID,
            .ssid_len = strlen(ESP_WIFI_AP_SSID),
            .channel = ESP_WIFI_CHANNEL,
            .password = ESP_WIFI_AP_PASSWD,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    if (strlen(ESP_WIFI_AP_PASSWD) == 0)
    {
        wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }
#if WIFI_AP_ENABLE && !WIFI_STA_ENABLE
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP)); // 设置模式为 AP
#endif                                                // WIFI_AP_ENABLE && !WIFI_STA_ENABLE
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));
#endif // WIFI_AP_ENABLE

    // 注册事件处理实例
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &UserWIFI_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &UserWIFI_event_handler,
                                                        NULL,
                                                        &instance_got_ip));
#if 0
    ESP_ERROR_CHECK(esp_event_handler_instance_register(ESP_HTTP_SERVER_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &UserWIFI_event_handler,
                                                        NULL,
                                                        &instance_control_clock));
#endif

    ESP_ERROR_CHECK(esp_wifi_start()); // 启动 wifi

    Show_Wifi_Info();
    xTaskCreatePinnedToCore(UserEnableAP,
                            AP_CONTROL_TASK_NAME,
                            AP_CONTROL_TASK_STACK_SIZE,
                            NULL,
                            AP_CONTROL_TASK_PRIORITY,
                            User_NewTask(),
                            0);
    vTaskSuspend(User_GetTaskHandle(AP_CONTROL_TASK_NAME));

    /* Set sta as the default interface */
    ESP_ERROR_CHECK(esp_netif_set_default_netif(esp_netif_sta));

#if WIFI_AP_ENABLE
    /* Enable napt on the AP netif */
    if (esp_netif_napt_enable(esp_netif_ap) != ESP_OK)
    {
        ESP_LOGE(TAG, "NAPT not enabled on the netif: %p", esp_netif_ap);
    }
#endif
#if WIFI_CONNETED_AUTO_ENABLE_HTTP_SERVER
    WebServerConnect_handler();
    xTaskCreatePinnedToCore(httpWifiConnect,
                            HTTP_WIFI_CONNECT_TASK_NAME,
                            HTTP_WIFI_CONNECT_TASK_STACK_SIZE,
                            NULL,
                            HTTP_WIFI_CONNECT_TASK_PRIORITY,
                            User_NewTask(),
                            0);
    vTaskSuspend(User_GetTaskHandle(HTTP_WIFI_CONNECT_TASK_NAME));
#endif
    return;
}

esp_err_t User_WIFI_disable_sta(void)
{
    esp_err_t err = ESP_OK;
    err = esp_wifi_get_mode(&record_mode);
    USER_WIFI_ERR_CHECK(err);

    switch (record_mode)
    {
    case WIFI_MODE_NULL:
    case WIFI_MODE_NAN:
    case WIFI_MODE_AP:
        ESP_LOGW(TAG, "wifi station is not enable");
        return err;
    case WIFI_MODE_STA:
    {
        err = esp_wifi_disconnect();
        ESP_LOGW(TAG, "esp_wifi_disconnect %s", (err) ? "fail" : "ok");
        USER_WIFI_ERR_CHECK(err);

        err = esp_wifi_set_mode(WIFI_MODE_NULL);
        USER_WIFI_ERR_CHECK(err);

        err = esp_wifi_get_mode(&record_mode);
        USER_WIFI_ERR_CHECK(err);
    }
    break;
    case WIFI_MODE_APSTA:
    {
        err = esp_wifi_disconnect();
        ESP_LOGW(TAG, "esp_wifi_disconnect %s", (err) ? "fail" : "ok");
        USER_WIFI_ERR_CHECK(err);

        err = esp_wifi_set_mode(WIFI_MODE_AP);
        USER_WIFI_ERR_CHECK(err);

        err = esp_wifi_get_mode(&record_mode);
        USER_WIFI_ERR_CHECK(err);
    }
    break;
    default:
        break;
    }

    return err;
}

/// @brief WIFI修改连接ap
/// @param
/// @return
esp_err_t User_WIFI_changeConnection_sta(void)
{
    esp_err_t err = ESP_OK;
    uint8_t MQTT_FLAG = false;
    if (GetMQTTinitReady() == true)
    {
        User_mqtt_client_disconnect();
        MQTT_FLAG = true;
    }

    err = esp_wifi_disconnect();
#if 0
    err = esp_wifi_stop();
    ESP_LOGW(TAG, "esp_wifi_stop %s", (err) ? "fail" : "ok");

    err = esp_wifi_deinit();
    ESP_LOGW(TAG, "esp_wifi_deinit %s", (err) ? "fail" : "ok");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); // 初始化配置, 使用宏将WiFi硬件配置初始化
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));                // 初始化 wifi
#endif // 重新初始化WiFi stack可以马上切换到新的配置
       // 否则需要等待到下一次本地网络请求才生效

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = GetConnectWIFI_AuthType(),
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        },
    };

    memcpy((char *)wifi_config.sta.ssid, GetConnectWIFI_SSID(), CONNECT_SSID_LEN);
    memcpy((char *)wifi_config.sta.password, GetConnectWIFI_PASSWD(), sizeof(wifi_config.sta.password));

    err = esp_wifi_get_mode(&record_mode);
    USER_WIFI_ERR_CHECK(err);

    switch (record_mode)
    {
    case WIFI_MODE_NULL:
        err = esp_wifi_set_mode(WIFI_MODE_STA); // 设置模式为 station
        break;
    case WIFI_MODE_AP:
        err = esp_wifi_set_mode(WIFI_MODE_APSTA); // 设置模式为 station
        break;
    default:
        ESP_LOGW(TAG, "wifi mode is not need set");
        break;
    }

    USER_WIFI_ERR_CHECK(err);

    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config); // 设置配置
    USER_WIFI_ERR_CHECK(err);

    err = esp_wifi_get_mode(&record_mode);
    USER_WIFI_ERR_CHECK(err);

    err = esp_wifi_connect();
    // err = esp_wifi_start();
    switch (err)
    {
    case ESP_OK:
        break;
    default:
        ESP_LOGE(TAG, "wifi start ERROR( %s )", esp_err_to_name(err));
        break;
    }

    vTaskDelay(3000 / portTICK_PERIOD_MS); // 扫描与连接需要时间
    if (Show_Wifi_Info() == ESP_OK)
    {
        err = Write_WIFIconnectionInfo_sta();
        if(err == ESP_OK)
            err = User_WIFI_disable_AP();
    }
        
    if (MQTT_FLAG == true)
        User_mqtt_client_connect();

    return err;
}

/// @brief 最优信道选择
/// @param channel channel_map(array)
/// @param channel_max 最多有多少频道
/// @return OK:channel_number, ERROR:-1
static int channel_select(unsigned int *channel, const unsigned int channel_max)
{
#define INTERFERENCE_CONSTANT 7

    int best_channel = 0;
    int channel_map[2][CHANNEL_MAX] = {0};
    for (size_t i = 0; i < channel_max; i++)
    {
        channel_map[0][i] = *(channel + i);
    }

    // 为每个频道标记出左右各两信道中的干扰
    for (size_t i = 2; i < channel_max - 2; i++)
    {
        channel_map[1][i] = *channel + *(channel + i - 1) +
                            *(channel + i + 1) + *(channel + i + 2);
    }
    channel_map[1][0] = *(channel + 1) + *(channel + 2);
    channel_map[1][1] = *(channel) + *(channel + 2) + *(channel + 3);

    channel_map[1][channel_max - 1] = *(channel + channel_max - 2) +
                                      *(channel + channel_max - 3);
    channel_map[1][channel_max - 2] = *(channel + channel_max - 3) +
                                      *(channel + channel_max - 4) +
                                      *(channel + channel_max - 1);
#if 0
    for (size_t i = 0; i < channel_max; i++)
    {
        printf("%d,", channel_map[0][i]);
    }
    putchar('\n');
    for (size_t i = 0; i < channel_max; i++)
    {
        printf("%d,", channel_map[1][i]);
    }
    putchar('\n');
#endif
    for (; best_channel < channel_max; best_channel++)
    {
        // 当前信道的接入数, 左右相邻信道的接入数
        if (channel_map[0][best_channel] < INTERFERENCE_CONSTANT &&
            channel_map[1][best_channel] < INTERFERENCE_CONSTANT)
            return best_channel + 1;
    }

    return -1;
}

/// @brief WIFI开启AP
/// @param ssid
/// @param passwd
/// @param ssid_len
/// @return OK:ESP_OK, FAIL:others
esp_err_t User_WIFI_enable_AP(const char *ssid, const char *passwd,
                              size_t ssid_len)
{
    uint8_t status_null = false, status_ap = false;
    uint8_t best_channel;
    esp_err_t err = ESP_OK;
    uint16_t ap_num = 0;
    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = {0},
            .ssid_len = ssid_len,
            .max_connection = MAX_STA_CONN,
            .password = {0},
            .authmode = WIFI_AUTH_OPEN,
            .ssid_hidden = 0,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    unsigned int channel_ap_num[CHANNEL_MAX] = {0};
    wifi_ap_record_t *scanAP_info = NULL;

    err = esp_wifi_get_mode(&record_mode);
    USER_WIFI_ERR_CHECK(err);

    switch (record_mode) /*先判断目前WiFi状态*/
    {
    case WIFI_MODE_NULL:
    {
        status_null = true;
        err = esp_wifi_set_mode(WIFI_MODE_STA); /*因为要先扫描, 判断信道情况*/
        USER_WIFI_ERR_CHECK(err);
        WIFI_MODE_CHENGE_WAIT(); // 等待station启动
    }
    break;
    case WIFI_MODE_AP:
    case WIFI_MODE_APSTA:
    {
        ESP_LOGI(TAG, "wifi soft-ap is running. mode: %s", GetWifiModeStr(record_mode));
        if (record_mode == WIFI_MODE_AP)
        {
            status_ap = true;
            err = esp_wifi_set_mode(WIFI_MODE_APSTA); /*因为要先扫描, 判断信道情况*/
            USER_WIFI_ERR_CHECK(err);
            WIFI_MODE_CHENGE_WAIT(); // 等待station启动
        }
    }
    break;
    case WIFI_MODE_NAN:
        ESP_LOGW(TAG, "%s", GetWifiModeStr(record_mode));
        return err;
    default:
        break;
    }

    err = esp_wifi_scan_start(NULL, true);
    USER_WIFI_ERR_CHECK(err);

    err = esp_wifi_scan_get_ap_num(&ap_num);
    USER_WIFI_ERR_CHECK(err);

    scanAP_info = (wifi_ap_record_t *)malloc(ap_num * sizeof(wifi_ap_record_t));
    err = esp_wifi_scan_get_ap_records(&ap_num, scanAP_info);
    if (WIFIERRORPARSE(err))
    {
        ESP_LOGW(__func__, "parma: %d, %p", ap_num, scanAP_info);
        esp_wifi_clear_ap_list();
        free(scanAP_info);
        return err;
    }

    if (status_null || status_ap)
    {
        err = esp_wifi_disconnect();
        USER_WIFI_ERR_CHECK(err);

        if (status_ap)
            err = esp_wifi_set_mode(WIFI_MODE_AP);

        if (status_null)
            err = esp_wifi_set_mode(WIFI_MODE_NULL);
        USER_WIFI_ERR_CHECK(err);
    }

    for (size_t i = 0; i < ap_num; i++)
    {
        channel_ap_num[(scanAP_info[i].primary) - 1]++;
    }
    best_channel = (uint8_t)channel_select(channel_ap_num, CHANNEL_MAX);
    if (best_channel == (uint8_t)-1)
        best_channel = ESP_WIFI_CHANNEL;
    if (best_channel > 11) // esp预置世界安全模式, 可用信道为1~11
        best_channel = 11;
    wifi_ap_config.ap.channel = best_channel;
    esp_wifi_clear_ap_list();
    free(scanAP_info);

    if (passwd == NULL) // 设置默认WiFi_ap密码
    {
        ESP_LOGI(TAG, "set default AP password");
        wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
        memset((void *)wifi_ap_config.ap.password, 0, sizeof(wifi_ap_config.ap.password));
    }
    else
    {
        memset((void *)wifi_ap_config.ap.password, 0, sizeof(wifi_ap_config.ap.password));
        strcpy((char *)wifi_ap_config.ap.password, passwd);
        wifi_ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    }

    if (ssid == NULL) // 设置默认WiFi_ap名称
    {
        ESP_LOGI(TAG, "set default AP name, use default password");

        memset((void *)wifi_ap_config.ap.ssid, 0, sizeof(wifi_ap_config.ap.ssid));
        strcpy((char *)wifi_ap_config.ap.ssid, ESP_WIFI_AP_SSID);
        wifi_ap_config.ap.ssid_len = strlen(ESP_WIFI_AP_SSID);

        // 没有名称默认为ESP32_clock, 密码也清空
        memset((void *)wifi_ap_config.ap.password, 0, sizeof(wifi_ap_config.ap.password));
        wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    else
    {
        memset((void *)wifi_ap_config.ap.ssid, 0, sizeof(wifi_ap_config.ap.ssid));
        strcpy((char *)wifi_ap_config.ap.ssid, ssid);
    }
#if PRINT_AP_ENABLE_DISABLE_RETURN_VALUE
    ESP_LOGI(__func__, "AP INFO <ssid: %s> <password: %s> <auth: %d>",
             wifi_ap_config.ap.ssid, wifi_ap_config.ap.password, wifi_ap_config.ap.authmode);
#endif
    err = esp_wifi_get_mode(&record_mode);
    USER_WIFI_ERR_CHECK(err);
    switch (record_mode)
    {
    case WIFI_MODE_AP:
    {
        err = esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config); // 设置AP配置
        if (WIFIERRORPARSE(err))
        {
#if ENABLE_AP_DEBUG
            ESP_LOGE(TAG, "ap netif:%p, ap config: ssid[%s] ssidLen[%d]\
 password[%s] auth[%d] channel[%d]",
                     esp_netif_ap,
                     wifi_ap_config.ap.ssid,
                     wifi_ap_config.ap.ssid_len,
                     wifi_ap_config.ap.password, wifi_ap_config.ap.authmode,
                     wifi_ap_config.ap.channel);
#endif
            return err;
        }
    }
    break;
    case WIFI_MODE_APSTA:
    {
        err = esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config); // 设置AP配置
        if (WIFIERRORPARSE(err))
        {
#if ENABLE_AP_DEBUG
            ESP_LOGE(TAG, "ap netif:%p, ap config: ssid[%s] ssidLen[%d]\
 password[%s] auth[%d] channel[%d]",
                     esp_netif_ap,
                     wifi_ap_config.ap.ssid,
                     wifi_ap_config.ap.ssid_len,
                     wifi_ap_config.ap.password, wifi_ap_config.ap.authmode,
                     wifi_ap_config.ap.channel);
#endif
            return err;
        }
    }
    break;
    case WIFI_MODE_NULL:
    case WIFI_MODE_STA:
    {
        esp_netif_ap = esp_netif_create_default_wifi_ap(); // 创建 wifi AP 默认的网络接口

        err = esp_wifi_set_mode(WIFI_MODE_APSTA);
        if (WIFIERRORPARSE(err))
        {
            esp_netif_destroy_default_wifi((void *)esp_netif_ap);
            esp_netif_ap = NULL;
            return err;
        }

        err = esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config); // 设置AP配置
        if (WIFIERRORPARSE(err))
        {
#if ENABLE_AP_DEBUG
            ESP_LOGE(TAG, "ap netif:%p, ap config: ssid[%s] ssidLen[%d]\
 password[%s] auth[%d] channel[%d]",
                     esp_netif_ap,
                     wifi_ap_config.ap.ssid,
                     wifi_ap_config.ap.ssid_len,
                     wifi_ap_config.ap.password, wifi_ap_config.ap.authmode,
                     wifi_ap_config.ap.channel);
#endif
            esp_netif_destroy_default_wifi((void *)esp_netif_ap);
            esp_netif_ap = NULL;
            return err;
        }
#if MY_NAPT
        /* Enable napt on the AP netif */
        if (esp_netif_napt_enable(esp_netif_ap) != ESP_OK)
        {
            ESP_LOGW(TAG, "NAPT not enabled on the netif: %p", esp_netif_ap);
        }
#endif
    }
    break;
    case WIFI_MODE_NAN:
        ESP_LOGW(TAG, "WIFI_MODE_NAN");
        break;
    default:
        ESP_LOGE(TAG, "WIFI MODE UNKNOW");
        return ESP_FAIL;
    }

    USER_WIFI_ERR_CHECK(esp_wifi_get_mode(&record_mode));
    ESP_LOGI(TAG, "start wifi_ap success");
    return err;
}

/// @brief WIFI关闭AP
/// @param
/// @return OK:ESP_OK, FAIL:others
esp_err_t User_WIFI_disable_AP(void)
{
    esp_err_t err = ESP_OK;
    err = esp_wifi_get_mode(&record_mode);
    USER_WIFI_ERR_CHECK(err);
    switch (record_mode)
    {
    case WIFI_MODE_AP:
    {
        err = esp_wifi_set_mode(WIFI_MODE_NULL);
#if PRINT_AP_ENABLE_DISABLE_RETURN_VALUE
        ESP_LOGW(__func__, "%s err = 0x%x", "WIFI_MODE_AP", err);
#endif
        USER_WIFI_ERR_CHECK(err);
        err = esp_wifi_get_mode(&record_mode);
        WIFIERRORPARSE(err);
    }
    break;
    case WIFI_MODE_APSTA:
    {
        err = esp_wifi_set_mode(WIFI_MODE_STA);
#if PRINT_AP_ENABLE_DISABLE_RETURN_VALUE
        ESP_LOGW(__func__, "%s err = 0x%x", "WIFI_MODE_APSTA", err);
#endif
        USER_WIFI_ERR_CHECK(err);
        err = esp_wifi_get_mode(&record_mode);
        WIFIERRORPARSE(err);
    }
    break;
    case WIFI_MODE_NULL:
    case WIFI_MODE_STA:
        ESP_LOGI(TAG, "It was not enable WiFi-ap before...");
        return err;
    case WIFI_MODE_NAN:
        ESP_LOGW(__func__, "WIFI_MODE_NAN");
        return ESP_FAIL;
    default:
        ESP_LOGE(TAG, "WIFI MODE UNKNOW");
        return ESP_FAIL;
    }

    esp_netif_destroy_default_wifi((void *)esp_netif_ap);
    esp_netif_ap = NULL;
    return err;
}

#if 0
static int findArray_min(int *channel, const unsigned int channel_num)
{
    int posA = 0, posB = 1;
    for (; posB < channel_num; posB++)
    {
        if (*(channel + posA) > *(channel + posB))
        {
            posA = posB;
        }
    }
    return *(channel + posA);
}
#endif

esp_err_t show_wifi_list(void)
{
    esp_err_t err = ESP_OK;
    uint16_t ap_num = 0;
    wifi_ap_record_t *scanAP_info;
    int lastStatus = WIFI_MODE_NULL;
    unsigned int channel_ap_num[CHANNEL_MAX] = {0};
    ESP_LOGI(TAG, "Scan start\n wait %.2f(s) ~ %.2f(s)",
             (11 * 120) / (float)1000, (11 * 360) / (float)1000);

    err = esp_wifi_get_mode(&record_mode);
    USER_WIFI_ERR_CHECK(err);
    switch (record_mode) // 记录模式
    {
    case WIFI_MODE_NULL:
    {
        lastStatus = record_mode;
        err = esp_wifi_set_mode(WIFI_MODE_STA);
        USER_WIFI_ERR_CHECK(err);
        WIFI_MODE_CHENGE_WAIT();
    }
    break;
    case WIFI_MODE_AP:
    {
        lastStatus = record_mode;
        err = esp_wifi_set_mode(WIFI_MODE_APSTA);
        USER_WIFI_ERR_CHECK(err);
        WIFI_MODE_CHENGE_WAIT();
    }
    break;
    case WIFI_MODE_STA:
    case WIFI_MODE_APSTA:
        lastStatus = record_mode;
        break;
    case WIFI_MODE_NAN:
        ESP_LOGE(__func__, "WIFI_MODE_NAN");
        return ESP_FAIL;
    default:
        ESP_LOGE(__func__, "WIFI_MODE_UNKNOWN");
        return ESP_FAIL;
    }

    err = esp_wifi_scan_start(NULL, true);
    USER_WIFI_ERR_CHECK(err);

    err = esp_wifi_scan_get_ap_num(&ap_num);
    ESP_LOGI(TAG, "ap_num = %" PRIu16 "", ap_num);
    USER_WIFI_ERR_CHECK(err);

    scanAP_info = (wifi_ap_record_t *)malloc(ap_num * sizeof(wifi_ap_record_t));
    err = esp_wifi_scan_get_ap_records(&ap_num, scanAP_info);
    if (WIFIERRORPARSE(err))
    {
        free(scanAP_info);
        return err;
    }
    for (size_t i = 0; i < ap_num; i++)
    {
        ESP_LOGI(TAG, "ssid:%s, rssi:%d, channel:%d, authmode:%d",
                 scanAP_info[i].ssid, scanAP_info[i].rssi,
                 scanAP_info[i].primary, scanAP_info[i].authmode);
        channel_ap_num[(scanAP_info[i].primary) - 1]++;
    }

    ESP_LOGI(TAG, "best channel is %d", channel_select(channel_ap_num, CHANNEL_MAX));
    esp_wifi_clear_ap_list();
    free(scanAP_info);

    switch (lastStatus) // 恢复模式
    {
    case WIFI_MODE_NULL:
    {
        err = esp_wifi_set_mode(WIFI_MODE_NULL);
        USER_WIFI_ERR_CHECK(err);
    }
    break;
    case WIFI_MODE_AP:
    {
        err = esp_wifi_set_mode(WIFI_MODE_AP);
        USER_WIFI_ERR_CHECK(err);
    }
    break;
    case WIFI_MODE_STA:
    case WIFI_MODE_APSTA:
        break;
    case WIFI_MODE_NAN:
        ESP_LOGE(__func__, "WIFI_MODE_NAN");
        return ESP_FAIL;
    default:
        ESP_LOGE(__func__, "WIFI_MODE_UNKNOWN");
        return ESP_FAIL;
    }
    return err;
}

/// @brief 获取连接的AP-SSID信息
/// @param
/// @return
const char *GetConnect_AP_info(void)
{
    wifi_ap_record_t ap_info = {0}; // 连接的AP信息
    esp_wifi_sta_get_ap_info(&ap_info);
    return (char *)(ap_info.ssid);
}

/// @brief 获取当前WIFI模式
/// @param
/// @return OK: wifi_mode_t
/// @return ERROR: ESP_FIAL
int GetCurrentWifiMode(void)
{
    esp_err_t err = ESP_OK;
    err = esp_wifi_get_mode(&record_mode);
    if (err != ESP_OK)
        return ESP_FAIL;
    return record_mode;
}

/// @brief 获取WIFI信息
/// @param
/// @return cJSON*
cJSON *GetWifiInfo_JSON(void)
{
    cJSON *wifi_info = cJSON_CreateObject();
    const char *const wifiModeStr[] = {
        "Null",
        "Station",
        "SoftAP",
        "SoftAP+Station",
        "NAN"};
    char IP_addr[16] = {0};
    esp_netif_ip_info_t ipInfo = {0};
    wifi_ap_record_t ap_info = {0}; // 连接的AP信息

    esp_wifi_sta_get_ap_info(&ap_info);
    esp_netif_get_ip_info(esp_netif_sta, &ipInfo);
    sprintf(IP_addr, "" IPSTR "", IP2STR(&ipInfo.ip));

    cJSON_AddStringToObject(wifi_info, WEB_WIFI_MODE, wifiModeStr[GetCurrentWifiMode()]);
    cJSON_AddStringToObject(wifi_info, WEB_WIFI_SSID, (char *)(ap_info.ssid));
    cJSON_AddStringToObject(wifi_info, WEB_WIFI_IP, IP_addr);
    cJSON_AddStringToObject(wifi_info, WEB_LOCAL_AP_SSID, ESP_WIFI_AP_SSID);
    cJSON_AddStringToObject(wifi_info, WEB_LOCAL_AP_PASSWORD, "");

    return wifi_info;
}

uint16_t GetWifiConnectStatus(void)
{
    return xEventGroupWaitBits(wifi_event_group,
                               WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                               pdFALSE,
                               pdFALSE,
                               0);
}
