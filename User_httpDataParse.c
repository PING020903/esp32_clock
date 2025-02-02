#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cJSON.h"
#include "User_httpDataParse.h"

#if HTTP_PARSE_DEBUG
static const char *testStr0 =
    "{\"bells_time\":\"11:11\",\"intervals_between_bells\":\"1\",\"total_days\":\"127\"}\"";
static const char *testStr1 =
    "{\"bells_time\":\"12:21\",\"intervals_between_bells\":\"1\",\"total_days\":\"0\"}\"";
#endif

/**
 * @brief 上一个错误
 */
static int lastError = HTTPDATA_PARSE_OK;

/**
 * @brief cJSON 对象
 */
static cJSON *obj = NULL;

/**
 * @brief 接收到的铃响信息
 */
static BellInfo recvBellInfo = {0};

static char ssid[32] = {0};
static char password[32] = {0};

#if DISABLE
#if 0
/**
 * @brief 还原数据
 * @param data
 * @return OK: HTTPDATA_PARSE_OK
 * @return ERROR: HTTPDATA_PARSE_ARG_ERR
 * @note client 发过来的数据, 除了26个字符和数字, 均以 "%FF" 这样的形式发送过来, 需还原
 */
static int RecoverData(char* data)
{
#define FIND_HEX(POS, LEN) for (; (*POS != '%') && (POS < LEN); POS++)
    char newData[4] = {0};
    char* pos = data, * unknow = NULL;
    size_t len = 0, cnt = 0;
    long long hex = 0;
    if ( data == NULL )
        return lastError = HTTPDATA_PARSE_ARG_ERR;

    do
    {
        len = strlen(data);
        FIND_HEX(pos, data + len);
        if ( pos >= data + len )
            break;
        memset(newData, 0, sizeof(newData));
        memcpy(newData, pos + 1, 2); // 将 hex 值取出
        hex = strtoll(newData, &unknow, 16); // 转为原始数据
        *(data + (pos - data)) = hex; // 修改数据
        for ( size_t i = pos - data + 3; i < len; i++ ) // 将后面的数据往前挪
        {
            *(data + i - 2) = *(data + i);
            *(data + i) = 0;
        }
        pos++;
    } while ( 1 );
#if 0
    *(pos - 1) = 0; // 将最后两位清零
    *(pos - 2) = 0;
#endif
    return lastError = HTTPDATA_PARSE_OK;
}
#else
/**
 * @brief 还原数据
 * @param data
 * @return OK: HTTPDATA_PARSE_OK
 * @return ERROR: HTTPDATA_PARSE_ARG_ERR
 * @note client 发过来的数据, 除了26个字符和数字, 均以 "%FF" 这样的形式发送过来, 需还原
 */
static int RecoverData(char *data)
{
    size_t len = 0;
    char *writePos = data;
    char *readPos = data;
    char hexStr[3] = {0};
    char ch = 0;
    if (data == NULL)
    {
        return lastError = HTTPDATA_PARSE_ARG_ERR;
    }

    len = strlen(data);
    while (*readPos)
    {
        if (*readPos == '%' && readPos[1] && readPos[2])
        {
            hexStr[0] = readPos[1];
            hexStr[1] = readPos[2];
            ch = (char)strtoul(hexStr, NULL, 16);
            *writePos++ = ch;
            readPos += 3; // 跳过 %XX
        }
        else
        {
            *writePos++ = *readPos++;
        }
    }

    *writePos = '\0'; // 确保字符串以 null 结尾

    return lastError = HTTPDATA_PARSE_OK;
}
#endif

/**
 * @brief 从 HTTP client 发送的表单中分离数据标签与数据
 * @param data
 * @return OK: HTTPDATA_PARSE_OK
 * @return ERROR: HTTPDATA_PARSE_ARG_ERR, HTTPDATA_PARSE_ALLOC_ERR
 */
static int LabelParse(const char *data)
{
    const char *separator = "=";
    const char *temp = NULL, *pos = data;
    HttpData *MapTemp = NULL;
    size_t len = 0, cnt = 0;
    if (data == NULL)
        return lastError = HTTPDATA_PARSE_ARG_ERR;

    if (dataLabelMap != NULL)
        return lastError = HTTPDATA_PARSE_LAST_MEM_NOT_FREE;

    while (1)
    {
#if LABEL_DEBUG
        printf("--%llu--\n", ++cnt);
#endif
        temp = strstr(pos, separator);
        if (temp == NULL)
            break;

        len = temp - pos + 1; // name 长度, 含 '\0'
        MapTemp = dataLabelMap;
        dataLabelCnt++;
        dataLabelMap = (HttpData *)realloc(MapTemp, sizeof(HttpData) * dataLabelCnt);
        if (dataLabelMap == NULL)
        {
            free(MapTemp);
            return lastError = HTTPDATA_PARSE_ALLOC_ERR;
        }

#if LABEL_DEBUG
        printf("label alloc len:%llu\n", len);
#endif
        (dataLabelMap + dataLabelCnt - 1)->label = (char *)malloc(len);
        if ((dataLabelMap + dataLabelCnt - 1)->label == NULL)
        {
            return lastError = HTTPDATA_PARSE_ALLOC_ERR;
        }
        memset((dataLabelMap + dataLabelCnt - 1)->label, 0, len);
        memcpy((dataLabelMap + dataLabelCnt - 1)->label, pos, len - 1);

        for (; *pos != '&' && pos < data + strlen(data); pos++)
            ; // 跳过 data
#if LABEL_DEBUG
        printf("data alloc len:%llu, pos:%p, temp:%p\n", pos - temp + 1, pos, temp);
#endif
        len = pos - temp + 1; // 字符串数据, 含 '\0'
        (dataLabelMap + dataLabelCnt - 1)->data = (char *)malloc(len);
        if ((dataLabelMap + dataLabelCnt - 1)->data == NULL)
        {
            return lastError = HTTPDATA_PARSE_ALLOC_ERR;
        }
        memset((dataLabelMap + dataLabelCnt - 1)->data, 0, len);
        memcpy((dataLabelMap + dataLabelCnt - 1)->data, temp + 1, len - 2); // 减去 '&' and '\0'
#if LABEL_DEBUG
        printf("label:%s, map len:%llu\n",
               (dataLabelMap + dataLabelCnt - 1)->label, dataLabelCnt);
#endif

        pos += 1; // 使得 pos 跳过 '&'
    }

#if 1
    // 还原原始数据
    for (size_t i = 0; i < dataLabelCnt; i++)
    {
        RecoverData((dataLabelMap + i)->data);
    }
#endif
    return lastError = HTTPDATA_PARSE_OK; // 正常退出循环
}
#else
/**
 * @brief 从 HTTP client 发送的 JSON 格式的表单中解析出 JSON 对象
 * @param data
 * @return
 */
static int LabelParse(const char *data)
{
    if (data == NULL)
        return lastError = HTTPDATA_PARSE_ARG_ERR;

    obj = cJSON_Parse(data);
    lastError = (obj != NULL)
                    ? HTTPDATA_PARSE_OK
                    : HTTPDATA_PARSE_FAIL;
    return lastError;
}
#endif

int GetLastParseError()
{
    return lastError;
}

int HttpDataParse_clock(const char *data)
{
    int err = HTTPDATA_PARSE_OK;
    cJSON *objTemp = NULL;
    char *strPos = NULL, *temp;
    double tmpVal = 0.0;
    err = LabelParse(data);
    if (err)
        return err;

    // 获取铃响的 days
    objTemp = cJSON_GetObjectItem(obj, BELL_DAYS);
    if (objTemp == NULL)
    {
        printf("clock 'days' parse fail...\n");
        cJSON_Delete(obj);
        return lastError = HTTPDATA_PARSE_ARG_ERR;
    }
    temp = cJSON_GetStringValue(objTemp);
    if (!temp) // 处理空指针
        tmpVal = cJSON_GetNumberValue(objTemp);
    recvBellInfo.bellDays = (temp)
                                ? (unsigned char)atoi(temp)
                                : (unsigned char)tmpVal;

    // 获取铃响的 time
    objTemp = cJSON_GetObjectItem(obj, BELLS_TIME);
    if (objTemp == NULL)
    {
        printf("clock 'time' parse fail...\n");
        cJSON_Delete(obj);
        return lastError = HTTPDATA_PARSE_ARG_ERR;
    }
    strPos = cJSON_GetStringValue(objTemp);
    if (strPos == NULL)
    {
        printf("clock 'time' is null...\n");
        return lastError = HTTPDATA_PARSE_ARG_ERR;
    }
    recvBellInfo.hour = (unsigned char)atoi(strPos); // 解析 hours
    temp = strPos;
    while (*strPos != ':')
    {
        if (strPos >= temp + 4) // 预防一直找不到 ':'
        {
            printf("clock parse, 'time' not find ':'...\n");
            return lastError = HTTPDATA_PARSE_FAIL;
        }
        strPos++;
    }
    recvBellInfo.min = (unsigned char)atoi(++strPos); // 解析 minutes

    // 获取铃响是否周期响的标记
    objTemp = cJSON_GetObjectItem(obj, BELLS_INTERVALS);
    if (objTemp == NULL)
    {
        printf("clock 'interval' parse fail...\n");
        cJSON_Delete(obj);
        return lastError = HTTPDATA_PARSE_ARG_ERR;
    }
    temp = cJSON_GetStringValue(objTemp);
    if (!temp) // 处理空指针
        tmpVal = cJSON_GetNumberValue(objTemp);
    recvBellInfo.intervals = (temp)
                                 ? (unsigned char)atoi(temp)
                                 : (unsigned char)tmpVal;

    cJSON_Delete(obj);
    obj = NULL;
    return lastError = err;
}

int HttpDataParse_wifiConnect(const char *data)
{
    int err = HTTPDATA_PARSE_OK;
    cJSON *objTemp = NULL;
    char *strPos0 = NULL, *strPos1 = NULL;
    err = LabelParse(data);
    if (err)
        return err;

    objTemp = cJSON_GetObjectItem(obj, STA2AP_SSID);
    if (objTemp == NULL)
    {
        printf("wifiConnect 'ssid' parse fail...\n");
        cJSON_Delete(obj);
        return lastError = HTTPDATA_PARSE_ARG_ERR;
    }
    strPos0 = cJSON_GetStringValue(objTemp);

    objTemp = cJSON_GetObjectItem(obj, STA2SP_PASSWD);
    if (objTemp == NULL)
    {
        printf("wifiConnect 'password' parse fail...\n");
        cJSON_Delete(obj);
        return lastError = HTTPDATA_PARSE_ARG_ERR;
    }
    strPos1 = cJSON_GetStringValue(objTemp);

    if (strPos0 == NULL || strPos1 == NULL)
    {
        printf("wifiConnect 'ssid' or 'password' is null...\n");
        cJSON_Delete(obj);
        return lastError = HTTPDATA_PARSE_ARG_ERR;
    }
    memset(ssid, 0, sizeof(ssid));
    memcpy(ssid, strPos0, strlen(strPos0));
    memset(password, 0, sizeof(password));
    memcpy(password, strPos1, strlen(strPos1));

    cJSON_Delete(obj);
    obj = NULL;
    return lastError = err;
}

#if 0
/// @brief 检查本周的某一天是否需要铃响
/// @param today 
/// @return 1: 需要铃响, 0: 不需要铃响
int isBellCreateTimer(const int today)
{
    return (recvBellInfo.bellDays & (1 << today)) ? 1 : 0;
}
#endif
#if HTTP_PARSE_DEBUG

int HttpTest(const char *data)
{
    return LabelParse(data);
}
#endif

BellInfo GetBellInfo()
{
    return recvBellInfo;
}

/// @brief 获取 wifi 连接信息
/// @param type 0: ssid, 1: password
/// @return
char *GetWifiConnetInfo(const int type)
{
    switch (type)
    {
    case 0:
        return ssid;
    default:
        return password;
    }
    return NULL;
}

void HttpDataBufFree()
{
    return cJSON_Delete(obj);
}