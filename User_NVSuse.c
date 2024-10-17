#include "string.h"
#include "nvs_flash.h" // nvs相关头文件
#include "esp_wifi_types.h"
#include "esp_log.h"
#include "User_NVSuse.h"

#define DEFAULT_NAMESPACE "storage"

#define ESP_WIFI_SSID "pP"
#define ESP_WIFI_PASS "pP440711"

#define CLOCK_HOUR (7)
#define CLOCK_MINUTE (00)
#define CLOCK_SECOND (00)

static const char *TAG = "User_NVS";

static int target_hour = -1,
           target_minute = -1,
           target_second = -1;
static uint64_t countdown = 0;

static nvs_handle_t nvs_handle_wifi = 0,
                    nvs_handle_ntp = 0,
                    nvs_handle_closeLed = 0;
static char connect_ssid[CONNECT_SSID_LEN] = {0};
static char connect_passwd[CONNECT_PASSWD_LEN] = {0};
static int32_t connect_auth_type = WIFI_AUTH_OPEN;
#define NVS_SPACE_KEY_SSID "sta__ssid"
#define NVS_SPACE_KEY_PASSWD "sta__passwd"
#define NVS_SPACE_KEY_AUTH_TYPE "sta__authType"

#define NVS_SPACE_KEY_HOUR "target_hour"
#define NVS_SPACE_KEY_MINUTE "target_minute"
#define NVS_SPACE_KEY_SECOND "target_second"

#define NVS_SPACE_KEY_CLOSE_LED_HOUR "OffLedHour"
#define NVS_SPACE_KEY_CLOSE_LED_MINUTE "OffLedMinute"
#define NVS_SPACE_KEY_CLOSE_LED_SECOND "OffLedSecond"

static uint8_t closeLedHour = 0,
               closeLedMinute = 0,
               closeLedSecond = 0,
               closeLedNvsFlag = true; // 判断是否有过写入

/// @brief NVS操作返回值解析打印
/// @param err NVS error code
/// @param ReadOrWrite 0:write, 1:read
/// @param key string key
/// @return ERROR:err
/// @note 用于str写入的API
static esp_err_t User_NVSretValueParse_str(esp_err_t err, const int ReadOrWrite, const char *key)
{
    const int flag = (ReadOrWrite) ? true : false;
    char *warnStr[] = {
        "(Write)string of key",
        "(Read)string of key"};
    char *warn_str = NULL;
    switch (flag)
    {
    case false:
        warn_str = warnStr[false];
        break;
    default:
        warn_str = warnStr[true];
        break;
    }

    switch (err)
    {
    case ESP_OK:
        break;
    case ESP_ERR_NVS_INVALID_HANDLE:
        ESP_LOGE(TAG, "(%s: %s)handle is closed", warn_str, key);
        break;
    case ESP_ERR_NVS_READ_ONLY:
        ESP_LOGE(TAG, "(%s: %s)handle is read only", warn_str, key);
        break;
    case ESP_ERR_NVS_INVALID_NAME:
        ESP_LOGE(TAG, "(%s: %s)invalid name", warn_str, key);
        break;
    case ESP_ERR_NVS_NOT_ENOUGH_SPACE:
        ESP_LOGE(TAG, "(%s: %s)NVS no enough space, will delete the string of key",
                 warn_str, key);
        nvs_erase_key(nvs_handle_wifi, key);
        break;
    case ESP_ERR_NVS_REMOVE_FAILED:
        ESP_LOGE(TAG, "(%s: %s)NVS write failed, please try reset NVS",
                 warn_str, key);
        break;
    case ESP_ERR_NVS_VALUE_TOO_LONG:
        ESP_LOGE(TAG, "(%s: %s)data too long", warn_str, key);
        break;
    case ESP_ERR_NVS_KEY_TOO_LONG:
        ESP_LOGE(TAG, "(%s: %s)key too long", warn_str, key);
        break;
    default:
        ESP_LOGE(TAG, "(%s: %s)unknown error(%s)",
                 warn_str, key, esp_err_to_name(err));
        break;
    }
    return err;
}

/// @brief NVS操作返回值解析打印
/// @param err NVS error code
/// @param key string key
/// @return ERROR:err
/// @note 用于int写入的API
static esp_err_t User_NVSretValueParse_int(esp_err_t err, const char *key)
{
    switch (err)
    {
    case ESP_OK:
#if NVS_DEBUG
        printf("Done\n");
        printf("target_hour = %d\n", target_hour);
#endif
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGE(TAG, "The value( key: \"%s\" ) is not initialized yet!\n", key);
        break;
    default:
        ESP_LOGE(TAG, "( key: \"%s\" )Error (%s) reading!\n", key, esp_err_to_name(err));
        break;
    }
    return err;
}

/// @brief 从NVS中删除WIFI连接信息
/// @param
/// @return
esp_err_t EraseWIFIconnetionInfoFromNVS(void)
{
    esp_err_t err = nvs_open(DEFAULT_NAMESPACE, NVS_READWRITE, &nvs_handle_wifi);
    err = nvs_erase_key(nvs_handle_wifi, NVS_SPACE_KEY_SSID);
    switch (err)
    {
    case ESP_OK:
        ESP_LOGI(TAG, "NVS erase success(%s)", NVS_SPACE_KEY_SSID);
        break;
    default:
        ESP_LOGE(TAG, "(%s)NVS erase failed(%s)", NVS_SPACE_KEY_SSID, esp_err_to_name(err));
        break;
    }

    err = nvs_erase_key(nvs_handle_wifi, NVS_SPACE_KEY_PASSWD);
    switch (err)
    {
    case ESP_OK:
        ESP_LOGI(TAG, "NVS erase success(%s)", NVS_SPACE_KEY_PASSWD);
        break;
    default:
        ESP_LOGE(TAG, "(%s)NVS erase failed(%s)", NVS_SPACE_KEY_PASSWD, esp_err_to_name(err));
        break;
    }

    err = nvs_erase_key(nvs_handle_wifi, NVS_SPACE_KEY_AUTH_TYPE);
    switch (err)
    {
    case ESP_OK:
        ESP_LOGI(TAG, "NVS erase success(%s)", NVS_SPACE_KEY_AUTH_TYPE);
        break;
    default:
        ESP_LOGE(TAG, "(%s)NVS erase failed(%s)", NVS_SPACE_KEY_AUTH_TYPE, esp_err_to_name(err));
        break;
    }
    nvs_close(nvs_handle_wifi);
    return err;
}

/// @brief 设置默认连接信息
/// @param
void SetDefaultConnetionInfo(void)
{
    strcpy(connect_ssid, ESP_WIFI_SSID);
    strcpy(connect_passwd, ESP_WIFI_PASS);
    connect_auth_type = WIFI_AUTH_WPA2_PSK;
    return;
}

/// @brief 获取连接SSID
/// @param
/// @return
char *GetConnectWIFI_SSID(void)
{
    return connect_ssid;
}

/// @brief 获取连接密码
/// @param
/// @return
char *GetConnectWIFI_PASSWD(void)
{
    return connect_passwd;
}

/// @brief 获取连接认证类型
/// @param
/// @return
int32_t GetConnectWIFI_AuthType(void)
{
    return connect_auth_type;
}

/// @brief 设置连接信息
/// @param ssid
/// @param passwd
/// @param auth_type
/// @return OK: ESP_OK, ERROR: ESP_ERR_INVALID_ARG
esp_err_t SetConnectionInfo(const char *ssid,
                            const char *passwd,
                            int32_t auth_type)
{
    if (ssid == NULL)
        return ESP_ERR_INVALID_ARG;

    strcpy(connect_ssid, ssid);
    strcpy(connect_passwd, passwd);

    if (passwd == NULL)
        connect_auth_type = WIFI_AUTH_OPEN;

    connect_auth_type = auth_type;
    return ESP_OK;
}

/// @brief 读取WIFI连接信息
/// @param
/// @return
esp_err_t Read_WIFIconnectionInfo_sta(void)
{
    const char *warn_str = "(Read)string of key";
    esp_err_t err = nvs_open(DEFAULT_NAMESPACE, NVS_READONLY, &nvs_handle_wifi);
    size_t passwd_length = CONNECT_PASSWD_LEN;
    size_t ssid_length = CONNECT_SSID_LEN;
    if (err)
    {
        ESP_LOGE(__func__, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        goto FUNC_EXIT;
    }

    /***************************************read***************************************/
    err = nvs_get_str(nvs_handle_wifi, NVS_SPACE_KEY_SSID, connect_ssid, &ssid_length);
    if (User_NVSretValueParse_str(err, 1, NVS_SPACE_KEY_SSID))
        goto FUNC_EXIT;

    err = nvs_get_str(nvs_handle_wifi, NVS_SPACE_KEY_PASSWD, connect_passwd, &passwd_length);
    if (User_NVSretValueParse_str(err, 1, NVS_SPACE_KEY_PASSWD))
        goto FUNC_EXIT;

    err = nvs_get_i32(nvs_handle_wifi, NVS_SPACE_KEY_AUTH_TYPE, &connect_auth_type);
    switch (err)
    {
    case ESP_OK:
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGW(TAG, "(%s)The value(%s) is not initialized yet!",
                 warn_str, NVS_SPACE_KEY_AUTH_TYPE);
        goto FUNC_EXIT;
    default:
        ESP_LOGW(__func__, "Error (%s) reading!", esp_err_to_name(err));
        goto FUNC_EXIT;
    }

FUNC_EXIT:
    nvs_close(nvs_handle_wifi);
    return err;
}

/// @brief 写入WIFI连接信息
/// @param
/// @return
esp_err_t Write_WIFIconnectionInfo_sta(void)
{
    const char *warn_str = "(Write)string of key";
    esp_err_t err = nvs_open(DEFAULT_NAMESPACE, NVS_READWRITE, &nvs_handle_wifi);
    if (err)
    {
        ESP_LOGW(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        goto FUNC_EXIT;
    }

    /***************************************write***************************************/
    if (strlen(NVS_SPACE_KEY_SSID) > (NVS_KEY_NAME_MAX_SIZE - 1))
    {
        ESP_LOGW(warn_str, "NVS key name(%s) too long, len=%d",
                 NVS_SPACE_KEY_SSID, strlen(NVS_SPACE_KEY_SSID));
        err = ESP_ERR_NVS_KEY_TOO_LONG;
        goto FUNC_EXIT;
    }
    err = nvs_set_str(nvs_handle_wifi, NVS_SPACE_KEY_SSID, connect_ssid);
    if (User_NVSretValueParse_str(err, 0, NVS_SPACE_KEY_SSID))
        goto FUNC_EXIT;

    if (strlen(NVS_SPACE_KEY_PASSWD) > (NVS_KEY_NAME_MAX_SIZE - 1))
    {
        ESP_LOGW(warn_str, "NVS key name(%s) too long, len=%d",
                 NVS_SPACE_KEY_SSID, strlen(NVS_SPACE_KEY_PASSWD));
        err = ESP_ERR_NVS_KEY_TOO_LONG;
        goto FUNC_EXIT;
    }
    err = nvs_set_str(nvs_handle_wifi, NVS_SPACE_KEY_PASSWD, connect_passwd);
    if (User_NVSretValueParse_str(err, 0, NVS_SPACE_KEY_PASSWD))
        goto FUNC_EXIT;

    if (strlen(NVS_SPACE_KEY_AUTH_TYPE) > (NVS_KEY_NAME_MAX_SIZE - 1))
    {
        ESP_LOGW(warn_str, "NVS key name(%s) too long, len=%d",
                 NVS_SPACE_KEY_SSID, strlen(NVS_SPACE_KEY_AUTH_TYPE));
        err = ESP_ERR_NVS_KEY_TOO_LONG;
        goto FUNC_EXIT;
    }
    err = nvs_set_i32(nvs_handle_wifi, NVS_SPACE_KEY_AUTH_TYPE, connect_auth_type);
    if (err)
        goto FUNC_EXIT;

FUNC_EXIT:
    nvs_close(nvs_handle_wifi);
    return err;
}

/// @brief 初始化NVS空间
/// @param
void User_NVS_Init(void)
{
    // Initialize NVS, be must componet ! ! !
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase()); // erase NVS partition(擦除NVS分区)
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

/// @brief 获取铃响目标时间
/// @param target 0:hour, 1:minute, 2:second
/// @return OK:target value, ERR:ESP_FAIL
int GetTargetTime(const unsigned int target)
{
    switch (target)
    {
    case 0:
        return target_hour;
    case 1:
        return target_minute;
    case 2:
        return target_second;

    default:
        break;
    }
    return ESP_FAIL;
}

/// @brief 从NVS获取铃响目标时间
/// @param
/// @return OK:ESP_OK, ERROR:others
esp_err_t updateTargetClockTimeFromNVS(void)
{
    esp_err_t err = nvs_open(DEFAULT_NAMESPACE, NVS_READONLY, &nvs_handle_ntp);
    if (err)
    {
        ESP_LOGE(__func__, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        goto FUNC_EXIT;
    }

    /***************************************read***************************************/
    err = nvs_get_i32(nvs_handle_ntp, NVS_SPACE_KEY_HOUR, (int32_t *)&target_hour);
    if (User_NVSretValueParse_int(err, NVS_SPACE_KEY_HOUR))
        goto FUNC_EXIT;

    err = nvs_get_i32(nvs_handle_ntp, NVS_SPACE_KEY_MINUTE, (int32_t *)&target_minute);
    if (User_NVSretValueParse_int(err, NVS_SPACE_KEY_MINUTE))
        goto FUNC_EXIT;

    err = nvs_get_i32(nvs_handle_ntp, NVS_SPACE_KEY_SECOND, (int32_t *)&target_second);
    if (User_NVSretValueParse_int(err, NVS_SPACE_KEY_SECOND))
        goto FUNC_EXIT;

FUNC_EXIT:
    nvs_close(nvs_handle_ntp);
    return err;
}

/// @brief 将当前铃响目标时间写入NVS
/// @param
/// @return OK:ESP_OK, ERR:others
esp_err_t updateTargetClockTimeToNVS(void)
{
    esp_err_t err = nvs_open(DEFAULT_NAMESPACE, NVS_READWRITE, &nvs_handle_ntp);
    if (err)
        goto FUNC_EXIT;
    /***************************************write***************************************/
    err = nvs_set_i32(nvs_handle_ntp, NVS_SPACE_KEY_HOUR, target_hour);
    if (err)
        goto FUNC_EXIT;

    err = nvs_set_i32(nvs_handle_ntp, NVS_SPACE_KEY_MINUTE, target_minute);
    if (err)
        goto FUNC_EXIT;

    err = nvs_set_i32(nvs_handle_ntp, NVS_SPACE_KEY_SECOND, target_second);
    if (err)
        goto FUNC_EXIT;

FUNC_EXIT:
    nvs_close(nvs_handle_ntp);
    return err;
}

/// @brief 装载铃响目标时间
/// @param hour target hour
/// @param minute target minute
/// @param second target second
/// @return OK:ESP_OK, ERR:others
esp_err_t SetTargetTime(const int hour, const int minute, const int second)
{
    if (hour < 0 || hour > 23)
    {
        ESP_LOGW(__func__, "hour(%d) error", hour);
        return ESP_ERR_INVALID_ARG;
    }

    if (minute < 0 || minute > 59)
    {
        ESP_LOGW(__func__, "minute(%d) error", minute);
        return ESP_ERR_INVALID_ARG;
    }

    if (second < 0 || second > 59)
    {
        ESP_LOGW(__func__, "second(%d) error", second);
        return ESP_ERR_INVALID_ARG;
    }

    target_hour = hour;
    target_minute = minute;
    target_second = second;

    return updateTargetClockTimeToNVS();
}

/// @brief 设置默认的铃响目标时间
/// @param
void SetDefaultTargetClockInfo(void)
{
    SetTargetTime(CLOCK_HOUR, CLOCK_MINUTE, CLOCK_SECOND);
}

/// @brief 获取倒计时时间
/// @param
/// @return
uint64_t GetCountdown(void)
{
    return countdown;
}

/// @brief 设置倒计时时间
/// @param
/// @return
uint64_t *SetCountdown(void)
{
    return &countdown;
}

/// @brief 设置循环(每天)关闭LED目标时间
/// @param hour
/// @param minute
/// @param second
/// @return
esp_err_t SetPeriodicCloseLedTargetTime(const uint8_t hour,
                                        const uint8_t minute,
                                        const uint8_t second)
{
    if (hour > 23 || minute > 59 || second > 59)// 无符号类型, 不会有零以下的值
    {
        ESP_LOGW(TAG, "hour(%d) or minute(%d) error", hour, minute);
        return ESP_ERR_INVALID_ARG;
    }

    closeLedHour = hour;
    closeLedMinute = minute;
    closeLedSecond = second;

    esp_err_t err = nvs_open(DEFAULT_NAMESPACE, NVS_READWRITE, &nvs_handle_closeLed);
    if (err)
        goto FUNC_EXIT;
    /***************************************write***************************************/
    err = nvs_set_u8(nvs_handle_closeLed, NVS_SPACE_KEY_CLOSE_LED_HOUR, closeLedHour);
    if (err)
        goto FUNC_EXIT;

    err = nvs_set_u8(nvs_handle_closeLed, NVS_SPACE_KEY_CLOSE_LED_MINUTE, closeLedMinute);
    if (err)
        goto FUNC_EXIT;

    err = nvs_set_u8(nvs_handle_closeLed, NVS_SPACE_KEY_CLOSE_LED_SECOND, closeLedSecond);
    if (err)
        goto FUNC_EXIT;

FUNC_EXIT:
    nvs_close(nvs_handle_closeLed);
    closeLedNvsFlag = true; // 已经写入NVS
    return err;
}

/// @brief 读取循环(每天)关闭LED的目标时间
/// @param hour
/// @param minute
/// @param second
/// @return
esp_err_t ReadPeriodicCloseLedTargetTime(uint8_t *hour,
                                         uint8_t *minute,
                                         uint8_t *second)
{
    esp_err_t err = nvs_open(DEFAULT_NAMESPACE, NVS_READONLY, &nvs_handle_closeLed);
    if (err)
        goto FUNC_EXIT;

    if (closeLedNvsFlag) // 如果已经读取过, 则直接返回
    {
        err = nvs_get_u8(nvs_handle_closeLed, NVS_SPACE_KEY_CLOSE_LED_HOUR, &closeLedHour);
        if (err)
            goto FUNC_EXIT;

        err = nvs_get_u8(nvs_handle_closeLed, NVS_SPACE_KEY_CLOSE_LED_MINUTE, &closeLedMinute);
        if (err)
            goto FUNC_EXIT;

        err = nvs_get_u8(nvs_handle_closeLed, NVS_SPACE_KEY_CLOSE_LED_SECOND, &closeLedSecond);
        if (err)
            goto FUNC_EXIT;
    }
    

FUNC_EXIT:
    nvs_close(nvs_handle_closeLed);
    *hour = closeLedHour;
    *minute = closeLedMinute;
    *second = closeLedSecond;
    closeLedNvsFlag = false;
    return err;
}
