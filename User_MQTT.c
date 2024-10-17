#include <stdio.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "User_SNTP.h"
#include "User_WIFI.h"
#include "User_MQTT.h"

#define CONFIG_BROKER_URL_FROM_STDIN 0
#define MQTTS_PROTOCOL "mqtts://"
#define MQTT_PROTOCOL "mqtt://"
#define TIME_STR_MAX_LEN (32)

/* MQTT Base Topic (基础通信) */
#define DEVICE_NAME "WJP_esp32"
#define PRODUCT_KEY "k1mopgbwe56"
#define DEVICE_TITLE "/" PRODUCT_KEY "/" DEVICE_NAME "/thing/deviceinfo" // 设备标签信息

#define DEVICE_STATUS "/as/mqtt/status/" PRODUCT_KEY "/" DEVICE_NAME ""

/*subsrcibe or unsubsrcibe topic*/
#define UART_TFT_STRINGS_SUB "/" PRODUCT_KEY "/" DEVICE_NAME "/user/uartTFTstringsSUB"
#define TOPIC_PROPERTY_POST_REPLY "/sys/" PRODUCT_KEY "/" DEVICE_NAME "/" \
                                  "thing/event/property/post_reply"
#define TOPIC_CLOUDE_NTP_REPLY "/ext/ntp/" PRODUCT_KEY "/" DEVICE_NAME "/response"
#define TOPIC_TITLE_DELETE_REPLY "/sys" DEVICE_TITLE "/delete_reply" // "设备标签删除"订阅
#define TOPIC_TITLE_UPDATA_REPLY "/sys" DEVICE_TITLE "/update_reply" // "设备标签更新"订阅
#define TOPIC_TITLE_GET_REPLY "/sys" DEVICE_TITLE "/get_reply"       // "设备标签获取"订阅
/********************************/
/*publish*/
#define USER_UPDATE_PUB "/" PRODUCT_KEY "/" DEVICE_NAME "/user/update"
#define TOPIC_TITLE_UPDATA "/sys" DEVICE_TITLE "/update" // "设备标签更新"发布
#define TOPIC_TITLE_DELETE "" DEVICE_TITLE "/delete"     // "设备标签删除"发布
#define TOPIC_TITLE_GET "/sys" DEVICE_TITLE "/get"       // "设备标签查询"发布
#define TOPIC_PROPERTY_POST "/sys/" PRODUCT_KEY "/" DEVICE_NAME "/thing/event/property/post"
#define UART_TFT_STRINGS_PUB "/" PRODUCT_KEY "/" DEVICE_NAME "/user/uartTFTstringsPUB"
#define TOPIC_CLOUDE_NTP "/ext/ntp/" PRODUCT_KEY "/" DEVICE_NAME "/request"
/*********/

static const char *TAG = "User_MQTT";
static esp_mqtt_client_handle_t client;
static char *localIp = "120.230.164.69"; // 公网出口IP
static char *MQTT_topicRecive = NULL;    // DATA_EVENT topic接收
static char *MQTT_dataRecive = NULL;     // DATA_EVENT data接收
static uint8_t mqttInitReady = false;
static time_t DeviceSendTime = 0, DeviceRecvTime = 0;

/// @brief 自定义的字符串拼接函数
/// @param dest 目标字符串
/// @param src 源字符串
/// @param src_len 源字符串的长度
/// @param DestLenEnd 目标字符串的末端位置
/// @return
/// @note 应对字符串中含有'\0'但字符串仍未结束的情况
static char *
UserStrcat(char *dest,
           const char *src,
           unsigned int src_len,
           const size_t DestLenEnd)
{
    char *dest_end = dest;
    size_t dest_len = 0;
    if (src_len == 0)
        return dest;
    if (dest == NULL || src == NULL)
        return NULL;

    // 寻找 dest 的结束位置
    dest_end += DestLenEnd;

    // 将 src 的内容复制到 dest 的末尾, 确保连'\0'也一起复制过去
    while (dest_len < src_len)
    {
        *dest_end = *src;
        dest_end++;
        src++;
        dest_len++;
    }

    // 确保结果字符串以空字符终止
    *dest_end = '\0';

    return dest;
}

/**
 * 将整数转换为字符串，格式为“(整数值)”
 *
 * @param number 待转换的整数
 * @param str 用于存储转换结果的字符数组
 * @param size 字符数组 str 的大小
 *
 * 注意：调用者必须确保 str 的大小足够存储转换后的字符串，包括终止空字符
 */
void intToString(int number, char *str, size_t size)
{
    // 使用 sprintf 将整数转换为字符串
    // 注意：str 必须有足够的空间来存储转换后的字符串
    // %d 是整数的格式化标识符
    // 返回值是写入的字符数，不包括终止空字符
    snprintf(str, size, "%d", number);
}

/// @brief 获取本地时间戳将其转换成字符串
/// @param timezone_offset_hours 时区偏移量
/// @param TimeStr 时间字符串(字符串要接入的位置)
/// @param TimeStrLen 时间字符串长度(传入的字符串长度要>=26)
/// @return 时间字符串"yyyy-mm-dd hh:mm:ss.ms"
static char *User_tmToStrings(const int timezone_offset_hours, char *TimeStr, size_t TimeStrLen)
{
    if (TimeStr == NULL || TimeStrLen < 26)
        return NULL;
    static My_tm User_tm; // My_tm结构体
    size_t str_len = TimeStrLen, pos = 0;

    memset((void *)TimeStr, 0, str_len);
    get_time_from_timer_v2(&User_tm);

    // TimeStr[pos++] = '\"';

    // 填充年份
    intToString(User_tm.year, TimeStr + pos, str_len);
    pos = strlen(TimeStr);
    TimeStr[pos++] = '-'; // 目前写入了6个字符, |"yyyy-|

    // 填充月份
    switch (User_tm.month)
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9: // 月份小于10前面补0
        TimeStr[pos++] = '0';
        intToString(User_tm.month, TimeStr + pos, str_len - pos);
        pos += strlen(TimeStr + pos); // 计算已经写入的字符数
        break;
    default:
        intToString(User_tm.month, TimeStr + pos, str_len - pos);
        pos += strlen(TimeStr + pos);
        break;
    }
    TimeStr[pos++] = '-'; // 目前写入了9个字符, |"yyyy-mm-|

    // 填充日期
    switch (User_tm.day)
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9: // 日期小于10前面补0
        TimeStr[pos++] = '0';
        intToString(User_tm.day, TimeStr + pos, str_len - pos);
        pos += strlen(TimeStr + pos);
        break;
    default:
        intToString(User_tm.day, TimeStr + pos, str_len - pos);
        pos += strlen(TimeStr + pos);
        break;
    }
    TimeStr[pos++] = ' '; // 目前写入了12个字符, |"yyyy-mm-dd |

    // 填充小时
    switch (User_tm.hour)
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9: // 小时小于10前面补0
        TimeStr[pos++] = '0';
        intToString(User_tm.hour, TimeStr + pos, str_len - pos);
        pos += strlen(TimeStr + pos);
        break;
    default:
        intToString(User_tm.hour, TimeStr + pos, str_len - pos);
        pos += strlen(TimeStr + pos);
        break;
    }
    TimeStr[pos++] = ':'; // 目前写入了15个字符, |"yyyy-mm-dd hh:|

    // 填充分钟
    switch (User_tm.minute)
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9: // 分钟小于10前面补0
        TimeStr[pos++] = '0';
        intToString(User_tm.minute, TimeStr + pos, str_len - pos);
        pos += strlen(TimeStr + pos);
        break;
    default:
        intToString(User_tm.minute, TimeStr + pos, str_len - pos);
        pos += strlen(TimeStr + pos);
        break;
    }
    TimeStr[pos++] = ':'; // 目前写入了18个字符, |"yyyy-mm-dd- hh:mm:|

    // 填充秒数
    switch (User_tm.second)
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9: // 秒数小于10前面补0
        TimeStr[pos++] = '0';
        intToString(User_tm.second, TimeStr + pos, str_len - pos);
        pos += strlen(TimeStr + pos);
        break;
    default:
        intToString(User_tm.second, TimeStr + pos, str_len - pos);
        pos += strlen(TimeStr + pos);
        break;
    }
    TimeStr[pos++] = '.'; // 目前写入了21个字符, |"yyyy-mm-dd- hh:mm:ss.|

    // 填充毫秒数
    TimeStr[pos++] = '0';
    TimeStr[pos++] = '0';
    TimeStr[pos++] = '0';
    // UserStrcat(TimeStr, "000", strlen("000"), pos);
    // strcat(TimeStr + pos, "000"); // 毫秒数, 由于获取的时间时间戳是unit: s, 所以直接加3个0
    // pos += 3;                     // 目前写入了25个字符, "yyyy-mm-dd- hh:mm:ss.000"

    switch (timezone_offset_hours)
    {
    case 0:
        TimeStr[pos++] = 'Z';
        // TimeStr[pos++] = '\"';
        TimeStr[pos++] = '\0';
        while (TimeStr[pos] != ' ')
        {
            pos--;
        }
        TimeStr[pos] = 'T';
        return TimeStr;
    default:
        // TimeStr[pos++] = '\"';
        break;
    }

    return TimeStr;
}

/// @brief 生成设备属性JSON对象
/// @return JSON_STR
static cJSON *devicePropertiesPadding(void)
{
    cJSON *root, *sub0, *sub1, *sub2, *sub3, *sub4;

    root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "method", "thing.event.property.post");
    cJSON_AddNumberToObject(root, "id", 111);
    cJSON_AddItemToObject(root, "properties", sub0 = cJSON_CreateArray());

    sub1 = cJSON_CreateObject();
    cJSON_AddStringToObject(sub1, "identifier", "WifiName");
    cJSON_AddItemToObject(sub1, "dataType", sub2 = cJSON_CreateObject());
    cJSON_AddStringToObject(sub2, "type", "text");

    cJSON_AddStringToObject(sub1, "WifiName", GetConnect_AP_info());

    sub3 = cJSON_CreateObject();
    cJSON_AddStringToObject(sub3, "identifier", "LEDSwitch");
    cJSON_AddItemToObject(sub3, "dataType", sub4 = cJSON_CreateObject());
    cJSON_AddStringToObject(sub4, "type", "bool");
    cJSON_AddNumberToObject(sub3, "LEDSwitch", 1);

    cJSON_AddItemReferenceToArray(sub0, sub1);
    cJSON_AddItemReferenceToArray(sub0, sub3);
#if 0
    sub3 = cJSON_CreateObject();
    cJSON_AddStringToObject(sub3, "identifier", "LEDSwitch");
    cJSON_AddNumberToObject(sub3, "LEDSwitch", 1);
    cJSON_AddNumberToObject(sub3, "time", GetTimestamp());

    cJSON_AddItemReferenceToArray(sub1, sub2);
    cJSON_AddItemReferenceToArray(sub1, sub3);
#endif
    cJSON_AddNumberToObject(root, "time", GetTimestamp());
    cJSON_AddNumberToObject(root, "version", 1.0);
    return root;
}

/// @brief 生成设备标签JSON对象
/// @return cJSON:订阅设备标签JSON对象
/// @note 请勿越界访问, 传参时一定要传足够大的数组缓存
static cJSON *sub_deviceTiltlePadding(void)
{

    cJSON *root, *sub0, *sub1, *sub2, *sub3;
    char TimeStr[TIME_STR_MAX_LEN] = {0};

    root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "id", 111);
    cJSON_AddNumberToObject(root, "version", 1.0);
    cJSON_AddItemToObject(root, "sys", sub0 = cJSON_CreateObject());
    cJSON_AddNumberToObject(sub0, "ack", 1); // 云端应答(1:响应, 0:不响应)
    cJSON_AddItemToObject(root, "params", sub1 = cJSON_CreateArray());

    sub2 = cJSON_CreateObject();
    cJSON_AddStringToObject(sub2, "attrKey", "localTime");
    cJSON_AddStringToObject(sub2, "attrValue", User_tmToStrings(8, TimeStr, TIME_STR_MAX_LEN));

    sub3 = cJSON_CreateObject();
    cJSON_AddStringToObject(sub3, "attrKey", "deviceName");
    cJSON_AddStringToObject(sub3, "attrValue", "ESP32Clock");

    cJSON_AddItemReferenceToArray(sub1, sub2);
    cJSON_AddItemReferenceToArray(sub1, sub3);

    cJSON_AddStringToObject(root, "method", "thing.deviceinfo.update");
    return root;
}

static cJSON *pub_GetDeviceTiltle(void)
{
    cJSON *root, *sub0, *sub1, *sub2, *sub3;
    char TimeStr[TIME_STR_MAX_LEN] = {0};

    root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "id", 111);
    cJSON_AddNumberToObject(root, "version", 1.0);
    #if 0
    cJSON_AddItemToObject(root, "sys", sub0 = cJSON_CreateObject());
    cJSON_AddNumberToObject(sub0, "ack", 1); // 云端应答(1:响应, 0:不响应)
    #endif
    cJSON_AddItemToObject(root, "params", sub1 = cJSON_CreateObject());

    cJSON_AddItemToObject(sub1, "attrKeys", sub3 = cJSON_CreateArray());

    cJSON_AddItemReferenceToArray(sub3, cJSON_CreateString("localTime"));
    cJSON_AddItemReferenceToArray(sub3, cJSON_CreateString("deviceName"));

    cJSON_AddStringToObject(root, "method", "thing.deviceinfo.get");
    return root;
}

static cJSON *pub_DeleteDeviceTiltle(void)
{
    cJSON *root, *sub0, *sub1, *sub2, *sub3;
    char TimeStr[TIME_STR_MAX_LEN] = {0};

    root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "id", 111);
    cJSON_AddNumberToObject(root, "version", 1.0);
    cJSON_AddItemToObject(root, "sys", sub0 = cJSON_CreateObject());
    cJSON_AddNumberToObject(sub0, "ack", 1); // 云端应答(1:响应, 0:不响应)
    cJSON_AddItemToObject(root, "params", sub1 = cJSON_CreateArray());

    sub2 = cJSON_CreateObject();
    cJSON_AddStringToObject(sub2, "attrKey", "localTime");

    cJSON_AddItemReferenceToArray(sub1, sub2);

    cJSON_AddStringToObject(root, "method", "thing.deviceinfo.update");
    return root;
}

/// @brief 串口屏数据JSON对象
/// @param
/// @return
static cJSON *uartTFTpadding(void)
{
    cJSON *root, *sub0, *sub1, *sub2, *sub3;
    char TimeStr[TIME_STR_MAX_LEN] = {0};

    root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "id", 111);
    cJSON_AddNumberToObject(root, "version", 1.0);
    cJSON_AddItemToObject(root, "sys", sub0 = cJSON_CreateObject());
    cJSON_AddNumberToObject(sub0, "ack", 1); // 云端应答(1:响应, 0:不响应)
    cJSON_AddItemToObject(root, "params", sub1 = cJSON_CreateArray());

    cJSON_AddStringToObject(root, "method", "user.uartTFTstringsPUB");

    return root;
}

/// @brief 生成本地时间戳JSON对象
/// @param
/// @return
static cJSON *localTimestampPadding(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "id", GetTimestamp());
    return root;
}

/// @brief 返回用户自定义的JSON对象字符串
/// @param root
/// @return JSON_STR
/// @note 该函数返回字符串前会将传入的JSON对象释放
static char *GetStringFromJSON(cJSON *root)
{
    if (root == NULL)
    {
        ESP_LOGE(__func__, "The root object is NULL... ");
        return NULL;
    }

    /* declarations */
    char *out = NULL;
    char *buf = NULL;
    size_t len = 0;

    /* formatted print */
    out = cJSON_Print(root);

    /* create buffer to succeed */
    /* the extra 5 bytes are because of inaccuracies when reserving memory */
    len = strlen(out) + 5;
    buf = (char *)malloc(len);
    if (buf == NULL)
    {
        printf("Failed to allocate memory.\n");
        return NULL;
    }

    /* Print to buffer */
    if (!cJSON_PrintPreallocated(root, buf, (int)len, 1))
    {
        printf("cJSON_PrintPreallocated failed!\n");
        if (strcmp(out, buf) != 0)
        {
            printf("cJSON_PrintPreallocated not the same as cJSON_Print!\n");
            printf("cJSON_Print result:\n%s\n", out);
            printf("cJSON_PrintPreallocated result:\n%s\n", buf);
        }
        free(out);
        free(buf);
        return NULL;
    }

    /* success */
    free(out);
    cJSON_Delete(root);
    return buf;
}

static void SeversTimeParse(const char *cjsonStr,
                            const time_t devRev, const time_t devSend)
{
    time_t severRev, severSend, temp;
    cJSON *root = NULL;
    struct tm *localTime;
    if (cjsonStr == NULL)
        return;

    root = cJSON_Parse(cjsonStr);
    cJSON *serverSendTime = cJSON_GetObjectItem(root, "serverSendTime");
    cJSON *serverRecvTime = cJSON_GetObjectItem(root, "serverRecvTime");

    if (serverSendTime == NULL || serverRecvTime == NULL)
    {
        ESP_LOGE(__func__, "Error: Missing field\n");
        cJSON_Delete(root);
        return;
    }

    severRev = (time_t)serverSendTime->valuedouble;
    severSend = (time_t)serverRecvTime->valuedouble;

    severRev /= 1000; // ms to s
    severSend /= 1000;

    temp = (severRev + severSend + devRev - devSend) / 2;
    ESP_LOGI(__func__, "Local time: %lld, sever time: %lld, temp: %lld",
             severSend, GetTimestamp(), temp);
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    client = event->client;

#if SUB_DEBUG_TASK
    char devInfo[128] = {0};
#endif
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        esp_mqtt_client_unsubscribe(client, TOPIC_TITLE_UPDATA_REPLY);
        esp_mqtt_client_unsubscribe(client, TOPIC_PROPERTY_POST_REPLY);
        esp_mqtt_client_unsubscribe(client, UART_TFT_STRINGS_SUB);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);

        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);

        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);

        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);

        MQTT_topicRecive = (char *)malloc(event->topic_len + 2);
        MQTT_dataRecive = (char *)malloc(event->data_len + 5);

        memcpy(MQTT_topicRecive, event->topic, event->topic_len);
        memcpy(MQTT_dataRecive, event->data, event->data_len);

        if (strstr(MQTT_topicRecive, TOPIC_CLOUDE_NTP_REPLY)) // ✔
        {
            DeviceRecvTime = GetTimestamp();
            SeversTimeParse(MQTT_dataRecive, DeviceRecvTime, DeviceSendTime);
        }

#if 0
        memset(MQTT_topicRecive, 0, sizeof(MQTT_topicRecive));
        UserStrcat(MQTT_topicRecive, event->topic, event->topic_len, 0U);

        memset(MQTT_dataRecive, 0, sizeof(MQTT_dataRecive));
        UserStrcat(MQTT_dataRecive, event->data, event->data_len, 0U);
#endif
        free(MQTT_topicRecive);
        free(MQTT_dataRecive);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGI(TAG, "MQTT_EVENT_BEFORE_CONNECT");
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

/// @brief MQTT客户端初始化
/// @param
esp_err_t mqtt_client_init(void)
{
WAIT_NTP:
    if (UserNTP_ready() == false)
    {
        ESP_LOGW(TAG, "Waiting for NTP ready...");
        vTaskDelay(pdMS_TO_TICKS(3000));
        goto WAIT_NTP;
    }

    /*
    {"
        {"clientId":"k1mopgbwe56.WJP_esp32|securemode=2,signmethod=hmacsha256,timestamp=1722877509058|",
        "username":"WJP_esp32&k1mopgbwe56",
        "mqttHostUrl":"iot-06z00doqpm9whvw.mqtt.iothub.aliyuncs.com",
        "passwd":"0002f264787d9e8766f4a65d413c51add4324324d46bf05953e0b9c725d6b2b6",
        "port":1883}
    */
    // ps: 每次在服务器web端点击"MQTT连接参数"都会导致password&clientId改变，所以每次都需要重新初始化
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.verification.use_global_ca_store = 0,
        .broker.verification.skip_cert_common_name_check = 0,
        .broker.address.uri = "" MQTT_PROTOCOL "iot-06z00doqpm9whvw.mqtt.iothub.aliyuncs.com",
        .broker.address.port = 1883,
        .credentials.username = "WJP_esp32&k1mopgbwe56",
        .credentials.client_id = "k1mopgbwe56.WJP_esp32|securemode=2,signmethod=hmacsha256,timestamp=1722877509058|",
        .credentials.authentication.use_secure_element = 0,
        .credentials.authentication.password = "0002f264787d9e8766f4a65d413c51add4324324d46bf05953e0b9c725d6b2b6",
        .session.disable_keepalive = 0,
        .session.disable_clean_session = 0,
    };
#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

    if (strcmp(mqtt_cfg.broker.address.uri, "FROM_STDIN") == 0)
    {
        int count = 0;
        printf("Please enter url of mqtt broker\n");
        while (count < 128)
        {
            int c = fgetc(stdin);
            if (c == '\n')
            {
                line[count] = '\0';
                break;
            }
            else if (c > 0 && c < 127)
            {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt_cfg.broker.address.uri = line;
        printf("Broker url: %s\n", line);
    }
    else
    {
        ESP_LOGE(TAG, "Configuration mismatch: wrong broker url");
        abort();
    }
#endif /* CONFIG_BROKER_URL_FROM_STDIN */

    client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL)
    {
        ESP_LOGE(TAG, "esp_mqtt_client_init failed");
        return ESP_FAIL;
    }

    /*
     * The last argument may be used to pass data to the event handler,
     * in this example mqtt_event_handler
     */
    esp_err_t ret = esp_mqtt_client_register_event(client,
                                                   ESP_EVENT_ANY_ID,
                                                   mqtt_event_handler,
                                                   NULL);
    if (ret)
    {
        switch (ret)
        {
        case ESP_ERR_NO_MEM:
            ESP_LOGE(TAG, "esp_mqtt_client_register_event failed: failed to allocate");
            break;
        case ESP_ERR_INVALID_ARG:
            ESP_LOGE(TAG, "esp_mqtt_client_register_event failed: on wrong initialization");
            break;
        default:
            ESP_LOGE(TAG, "esp_mqtt_client_register_event failed: unknown error( %s )",
                     esp_err_to_name(ret));
            break;
        }
        return ret;
    }

    ret = esp_mqtt_client_start(client);
    if (ret)
    {
        switch (ret)
        {
        case ESP_ERR_INVALID_ARG:
            ESP_LOGE(TAG, "esp_mqtt_client_register_event failed: on wrong initialization");
            break;
        default:
            ESP_LOGE(TAG, "esp_mqtt_client_register_event failed: unknown error( %s )",
                     esp_err_to_name(ret));
            break;
        }
        return ret;
    }

    mqttInitReady = true;
    return ret;
}

/// @brief MQTT客户端释放
/// @param
/// @return
esp_err_t mqtt_client_deinit(void)
{
    esp_err_t err = ESP_OK;
    err = esp_mqtt_client_disconnect(client);
    if (err)
    {
        ESP_LOGW(TAG, "esp_mqtt_client on wrong initialization");
        return err;
    }

    err = esp_mqtt_client_stop(client);
    if (err)
    {
        switch (err)
        {
        case ESP_FAIL:
            ESP_LOGW(TAG, "esp_mqtt_client_stop failed: client is in invalid state");
            break;
        case ESP_ERR_INVALID_ARG:
            ESP_LOGW(TAG, "esp_mqtt_client_stop failed:  on wrong initialization");
            break;
        default:
            ESP_LOGW(TAG, "esp_mqtt_client_stop failed: unknown error( %s )",
                     esp_err_to_name(err));
            break;
        }
        return err;
    }

    err = esp_mqtt_client_unregister_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler);
    if (err)
    {
        switch (err)
        {
        case ESP_ERR_NO_MEM:
            ESP_LOGW(TAG, "esp_mqtt_client_unregister_event failed: failed to allocate");
            break;
        case ESP_ERR_INVALID_ARG:
            ESP_LOGW(TAG, "esp_mqtt_client_unregister_event failed: on invalid event id");
            break;
        default:
            ESP_LOGW(TAG, "esp_mqtt_client_unregister_event failed: unknown error( %s )",
                     esp_err_to_name(err));
            break;
        }
        return err;
    }

    err = esp_mqtt_client_destroy(client);
    if (err)
    {
        ESP_LOGW(TAG, "esp_mqtt_client_destroy failed: on wrong initialization");
        return err;
    }

    mqttInitReady = false;
    return err;
}

/// @brief 获取MQTT是否初始化完成
/// @param
/// @return OK:true 1, FAIL:false 0
unsigned int GetMQTTinitReady(void)
{
    return mqttInitReady;
}

/// @brief MQTT客户端订阅
/// @param
/// @return
esp_err_t User_mqtt_client_subscribe(const int type)
{
    char *topic = NULL;

    switch (type)
    {
    case '0':
        esp_mqtt_client_subscribe(client, UART_TFT_STRINGS_SUB, 1);
        topic = (char *)malloc(strlen(UART_TFT_STRINGS_SUB) + 2);
        strcpy(topic, UART_TFT_STRINGS_SUB);
        break;
    case '1':
        esp_mqtt_client_subscribe(client, TOPIC_TITLE_UPDATA_REPLY, 1);
        topic = (char *)malloc(strlen(TOPIC_TITLE_UPDATA_REPLY) + 2);
        strcpy(topic, TOPIC_TITLE_UPDATA_REPLY);
        break;
    case '2':
        esp_mqtt_client_subscribe(client, TOPIC_PROPERTY_POST_REPLY, 1);
        topic = (char *)malloc(strlen(TOPIC_PROPERTY_POST_REPLY) + 2);
        strcpy(topic, TOPIC_PROPERTY_POST_REPLY);
        break;
    case '3': // NTP服务目前仅支持QoS=0的消息, NTP订阅在云上不显示
        esp_mqtt_client_subscribe(client, TOPIC_CLOUDE_NTP_REPLY, 0);
        topic = (char *)malloc(strlen(TOPIC_CLOUDE_NTP_REPLY) + 2);
        strcpy(topic, TOPIC_CLOUDE_NTP_REPLY);
        break;
    case '4':
        esp_mqtt_client_subscribe(client, TOPIC_TITLE_DELETE_REPLY, 1);
        topic = (char *)malloc(strlen(TOPIC_TITLE_DELETE_REPLY) + 2);
        strcpy(topic, TOPIC_TITLE_DELETE_REPLY);
        break;
    case '5':
        esp_mqtt_client_subscribe(client, TOPIC_TITLE_GET_REPLY, 1);
        topic = (char *)malloc(strlen(TOPIC_TITLE_GET_REPLY) + 2);
        strcpy(topic, TOPIC_TITLE_GET_REPLY);
        break;
    default:
        return ESP_OK;
    }

    ESP_LOGI(TAG, "topic=%s subscribed", topic);
    free(topic);
    return ESP_OK;
}

/// @brief MQTT客户端取消订阅
/// @param
/// @return
esp_err_t User_mqtt_client_unsubscribe(const int type)
{
    char *topic = NULL;

    switch (type)
    {
    case '0':
        esp_mqtt_client_unsubscribe(client, UART_TFT_STRINGS_SUB);
        topic = (char *)malloc(strlen(UART_TFT_STRINGS_SUB) + 2);
        strcpy(topic, UART_TFT_STRINGS_SUB);
        break;
    case '1':
        esp_mqtt_client_unsubscribe(client, TOPIC_TITLE_UPDATA_REPLY);
        topic = (char *)malloc(strlen(TOPIC_TITLE_UPDATA_REPLY) + 2);
        strcpy(topic, TOPIC_TITLE_UPDATA_REPLY);
        break;
    case '2':
        esp_mqtt_client_unsubscribe(client, TOPIC_PROPERTY_POST_REPLY);
        topic = (char *)malloc(strlen(TOPIC_PROPERTY_POST_REPLY) + 2);
        strcpy(topic, TOPIC_PROPERTY_POST_REPLY);
        break;
    case '3': // NTP订阅在云上不显示
        esp_mqtt_client_unsubscribe(client, TOPIC_CLOUDE_NTP_REPLY);
        topic = (char *)malloc(strlen(TOPIC_CLOUDE_NTP_REPLY) + 2);
        strcpy(topic, TOPIC_CLOUDE_NTP_REPLY);
        break;
    case '4':
        esp_mqtt_client_unsubscribe(client, TOPIC_TITLE_DELETE_REPLY);
        topic = (char *)malloc(strlen(TOPIC_TITLE_DELETE_REPLY) + 2);
        strcpy(topic, TOPIC_TITLE_DELETE_REPLY);
        break;
    case '5':
        esp_mqtt_client_unsubscribe(client, TOPIC_TITLE_GET_REPLY);
        topic = (char *)malloc(strlen(TOPIC_TITLE_GET_REPLY) + 2);
        strcpy(topic, TOPIC_TITLE_GET_REPLY);
        break;
    default:
        return ESP_OK;
    }

    ESP_LOGI(TAG, "topic=%s unsubscribed", topic);
    free(topic);
    return ESP_OK;
}

/// @brief MQTT客户端发送数据
/// @param
/// @return
esp_err_t User_mqtt_client_publish(const int type)
{
    int msg_id = 0;
    char *localString = NULL;
    char *topic = NULL;
    switch (type)
    {
    case '0':
        localString = GetStringFromJSON(uartTFTpadding());
        topic = (char *)malloc(strlen(UART_TFT_STRINGS_PUB) + 2);
        strcpy(topic, UART_TFT_STRINGS_PUB);
        msg_id = esp_mqtt_client_publish(client,
                                         UART_TFT_STRINGS_PUB,
                                         localString,
                                         0, 1, 0);
        break;
    case '1':
        localString = GetStringFromJSON(sub_deviceTiltlePadding());
        topic = (char *)malloc(strlen(TOPIC_TITLE_UPDATA) + 2);
        strcpy(topic, TOPIC_TITLE_UPDATA);
        msg_id = esp_mqtt_client_publish(client,
                                         TOPIC_TITLE_UPDATA,
                                         localString,
                                         0, 1, 0);
        break;
    case '2':
        localString = GetStringFromJSON(devicePropertiesPadding());
        topic = (char *)malloc(strlen(TOPIC_PROPERTY_POST) + 2);
        strcpy(topic, TOPIC_PROPERTY_POST);
        msg_id = esp_mqtt_client_publish(client,
                                         TOPIC_PROPERTY_POST,
                                         localString,
                                         0, 1, 0);
        break;
    case '3':
        localString = GetStringFromJSON(localTimestampPadding());
        topic = (char *)malloc(strlen(TOPIC_CLOUDE_NTP) + 2);
        strcpy(topic, TOPIC_CLOUDE_NTP);
        DeviceSendTime = GetTimestamp(); // 本地时间戳精度为second, 服务器时间戳精度为ms
        msg_id = esp_mqtt_client_publish(client,
                                         TOPIC_CLOUDE_NTP,
                                         localString,
                                         0, 1, 0);
        break;
    case '4':
        localString = GetStringFromJSON(pub_DeleteDeviceTiltle());
        topic = (char *)malloc(strlen(TOPIC_TITLE_DELETE) + 2);
        strcpy(topic, TOPIC_TITLE_DELETE);
        msg_id = esp_mqtt_client_publish(client,
                                         TOPIC_TITLE_DELETE,
                                         localString,
                                         0, 1, 0);
        break;
    case '5':
        localString = GetStringFromJSON(pub_GetDeviceTiltle());
        topic = (char *)malloc(strlen(TOPIC_TITLE_GET) + 2);
        strcpy(topic, TOPIC_TITLE_GET);
        msg_id = esp_mqtt_client_publish(client,
                                         TOPIC_TITLE_GET,
                                         localString,
                                         0, 1, 0);
        break;
    default:
        return ESP_OK;
    }
    ESP_LOGI(TAG, "publish  topic=%s, msg_id=%d", topic, msg_id);

    free(localString);
    free(topic);
    return ESP_OK;
}

esp_err_t User_mqtt_client_connect(void)
{
    return esp_mqtt_client_reconnect(client);
}

esp_err_t User_mqtt_client_disconnect(void)
{
    return esp_mqtt_client_disconnect(client);
}

esp_err_t User_mqtt_test1(void)
{
    ESP_LOGI(__func__, " ");
    return ESP_OK;
}

esp_err_t User_mqtt_test2(void)
{
    char *localString = NULL;
    localString = GetStringFromJSON(devicePropertiesPadding());
    if (localString)
    {
        printf("%s\n", localString);
    }
    // vTaskDelay(1000);
    free(localString);
    return ESP_OK;
}

esp_err_t User_mqtt_test3(void)
{
    char *String = NULL;
    String = GetStringFromJSON(uartTFTpadding());
    if (String)
    {
        printf("%s\n", String);
    }
    // vTaskDelay(1000);
    free(String);
    return ESP_OK;
}

/// @brief topic对比处理
/// @param
/// @return 返回topic所对应的值
/// @return ZERO: 0, null
/// @return ONE:  1, TOPIC_TITLE_UPDATA_REPLY
/// @return THREE: 2, UART_TFT_STRINGS_SUB
int topic_handle(void)
{
    char *ret_point = NULL;

    ret_point = strstr(MQTT_topicRecive, TOPIC_TITLE_UPDATA_REPLY);
    if (ret_point != NULL)
        return 1;

    ret_point = strstr(MQTT_topicRecive, UART_TFT_STRINGS_SUB);
    if (ret_point != NULL)
        return 2;

    return 0;
}
#if 0
/// @brief 根据topic列表值选择不同方式的数据处理
/// @param select topic列表中的值
/// @return
dataInfo_fromMQTT dataParse(const int select)
{
    switch (select)
    {
    case 0:
        return;
    }
}
#endif

/// @brief 释放dataInfo_fromMQTT的字符串
/// @param data
/// @return OK:0
/// @return FAIL:
int free_userStr(dataInfo_fromMQTT data)
{
    free(data.userStr);
    return 0;
}