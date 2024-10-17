#ifndef _USER__MQTT_H_
#define _USER__MQTT_H_

/// @brief 从MQTT中接收后并解析的数据
/// @note - topic_num topic索引
/// @note - userStr 用户数据
/// @note - arrLen 数据长度( 按照char为基准的长度 )
/// @note - typeSize 数据应该以几个byte的形式读取
typedef struct
{
    size_t topic_num;       ///< topic索引
    unsigned char *userStr; ///< 用户数据
    size_t arrLen;          ///< 数据长度( 按照char为基准的长度 )
    size_t typeSize;        ///< 数据应该以几个byte的形式读取
} dataInfo_fromMQTT;

esp_err_t mqtt_client_init(void);

esp_err_t mqtt_client_deinit(void);

unsigned int GetMQTTinitReady(void);

esp_err_t User_mqtt_client_subscribe(const int type);

esp_err_t User_mqtt_client_unsubscribe(const int type);

esp_err_t User_mqtt_client_publish(const int type);

esp_err_t User_mqtt_client_connect(void);

esp_err_t User_mqtt_client_disconnect(void);

esp_err_t User_mqtt_test1(void);

esp_err_t User_mqtt_test2(void);

esp_err_t User_mqtt_test3(void);

int topic_handle(void);

int free_userStr(dataInfo_fromMQTT data);
#endif