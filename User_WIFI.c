#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"  // wifi相关头文件
#include "esp_event.h" // wifi事件头文件
#include "esp_eth.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_check.h"
#include "esp_netif.h"
#include "esp_tls_crypto.h"
//#include "esp_https_server.h"

#include "nvs_flash.h" // nvs相关头文件
#include "User_NVSuse.h"
#include "User_sntp.h"
// #include "User_http.h"
#include "User_MQTT.h"
#include "User_WIFI.h"

static const char *TAG = "User_WIFI";
static EventGroupHandle_t wifi_event_group; // wifi事件组
static int s_retry_num = 0;                 // 重试次数
static wifi_ap_record_t ap_info;            // AP信息
static esp_netif_t *esp_netif_sta;          // WIFI_station句柄
#if WIFI_AP_ENABLE
static esp_netif_t *esp_netif_ap; // WIFI_AP句柄
#endif

static esp_event_handler_instance_t instance_any_id; // 事件处理句柄
static esp_event_handler_instance_t instance_got_ip;
static esp_event_handler_instance_t instance_control_clock;

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

    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
        {
            ESP_ERROR_CHECK(esp_wifi_connect());
        }
        break;
        case WIFI_EVENT_STA_CONNECTED:
        {
            ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED ! ! !");
        }
        break;
        case WIFI_EVENT_STA_DISCONNECTED:
        {

            if (s_retry_num++ < ESP_MAXIMUM_RETRY) // 尝试重连
            {
                ESP_LOGW(TAG, "retry(%d) to connect to the AP", s_retry_num);
                esp_err_t ret = esp_wifi_connect();
                switch (ret)
                {
                case ESP_OK:
                    return;

                case ESP_ERR_WIFI_NOT_INIT:
                    ESP_LOGW(TAG, "WIFI NOT INIT...");
                    break;

                case ESP_ERR_WIFI_NOT_STARTED:
                    ESP_LOGW(TAG, "WIFI NOT STARTED...");
                    break;

                case ESP_ERR_WIFI_CONN:
                    ESP_LOGW(TAG, "WIFI CONN ERROR...");
                    break;

                case ESP_ERR_WIFI_SSID:
                    ESP_LOGW(TAG, "WIFI SSID ERROR...");
                    break;

                default:
                    break;
                }
            }
            else
            {
                xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT); // 连接失败
                ESP_LOGW(TAG, "connect to the AP fail");
            }
            // s_retry_num++;
        }
        break;
        case WIFI_EVENT_SCAN_DONE:
        {
            ESP_LOGI(TAG, "wifi scan done");
        }
        break;
        case WIFI_EVENT_STA_BEACON_TIMEOUT:
        {
            ESP_LOGE(TAG, "wifi timeout");
        }
        break;
        case WIFI_EVENT_STA_AUTHMODE_CHANGE:
        {
            ESP_LOGI(TAG, "wifi auth mode change");
        }
        break;
        case WIFI_EVENT_AP_START:
        {
            ESP_LOGI(TAG, "Soft-AP start, start web server..");
        }
        break;
        case WIFI_EVENT_AP_STOP:
        {
            ESP_LOGI(TAG, "Soft-AP stop, stop web server..");
        }
        break;
        case WIFI_EVENT_AP_STACONNECTED:
        {
            ESP_LOGI(TAG, "a station connected to Soft-AP");
            printf("Minimum free heap size: %" PRIu32 " bytes\n",
                   esp_get_minimum_free_heap_size());
            // connect_handler();
        }
        break;
        case WIFI_EVENT_AP_STADISCONNECTED:
        {
            ESP_LOGI(TAG, "a station disconnected from Soft-AP");
            printf("Minimum free heap size: %" PRIu32 " bytes\n",
                   esp_get_minimum_free_heap_size());
            // disconnect_handler();
        }
        break;
        case WIFI_EVENT_AP_PROBEREQRECVED:
        {
            ESP_LOGI(TAG, "Receive probe request packet in soft-AP interface");
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
            ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT); // 连接成功
            s_retry_num = 0;
        }
        break;
        case IP_EVENT_STA_LOST_IP:
        {
            ESP_LOGI(TAG, "wifi event: sta lost IP...");
        }
        break;
        case IP_EVENT_ETH_GOT_IP:
        {
            ESP_LOGI(TAG, "ETH got ip");
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

/// @brief 显示WIFI连接信息
/// @param
/// @return OK: ESP_OK, ERROR: ESP_FAIL
esp_err_t Show_Wifi_connectionInfo_sta(void)
{
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);
    uint8_t protocol_bitmap_temp;
    if (bits & WIFI_CONNECTED_BIT)
    {
        /* 当支持802.11n时候, 才支持40MHz频段带宽 */
        ESP_ERROR_CHECK(esp_wifi_get_protocol(WIFI_IF_STA, &protocol_bitmap_temp));

        esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
        switch (err)
        {
        case ESP_OK:
            break;
        case ESP_ERR_WIFI_NOT_CONNECT:
            ESP_LOGW(TAG, "The station is in disconnect status");
            break;
        case ESP_ERR_WIFI_CONN:
            ESP_LOGW(TAG, "The station interface don't initialized");
            break;
        default:
            ESP_LOGE(TAG, "Unknown error %d", err);
            break;
        }

        ESP_LOGI(TAG, "connected to ap SSID:%s, AP_MAC:" MACSTR ", bitmap:0x%x",
                 ap_info.ssid, MAC2STR(ap_info.bssid), protocol_bitmap_temp);
        return ESP_OK;
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 GetConnectWIFI_SSID(), GetConnectWIFI_PASSWD());
        return ESP_FAIL;
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT ! ! !");
    }

    return ESP_FAIL;
}

/**
 * @brief  用户WIFI初始化
 * @note
 * @return
 *
 */
void User_WIFI_Init(void)
{
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
        ESP_LOGE(TAG, "unknown error create event loop! ! !\n  deinit NVS");
        ESP_ERROR_CHECK(nvs_flash_deinit());
        return;

    default:
        ESP_LOGE(TAG, "unknown error create event loop! ! !\n  deinit NVS");
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

    Show_Wifi_connectionInfo_sta();

    /* Set sta as the default interface */
    ESP_ERROR_CHECK(esp_netif_set_default_netif(esp_netif_sta));

#if WIFI_AP_ENABLE
    /* Enable napt on the AP netif */
    if (esp_netif_napt_enable(esp_netif_ap) != ESP_OK)
    {
        ESP_LOGE(TAG, "NAPT not enabled on the netif: %p", esp_netif_ap);
    }
#endif
    return;
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
    strcpy((char *)wifi_config.sta.ssid, GetConnectWIFI_SSID());
    strcpy((char *)wifi_config.sta.password, GetConnectWIFI_PASSWD());

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));               // 设置模式为 station
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config)); // 设置配置
    err = esp_wifi_connect();
    // err = esp_wifi_start();
    switch (err)
    {
    case ESP_OK:
        Write_WIFIconnectionInfo_sta();
        break;
    default:
        ESP_LOGE(TAG, "wifi start ERROR( %s )", esp_err_to_name(err));
        break;
    }

    vTaskDelay(2000 / portTICK_PERIOD_MS); // 扫描与连接需要时间
    Show_Wifi_connectionInfo_sta();
    if (MQTT_FLAG == true)
        User_mqtt_client_connect();

    return ESP_OK;
}

/// @brief 获取连接的AP信息
/// @param
/// @return
const char *GetConnect_AP_info(void)
{
    return (char *)ap_info.ssid;
}
