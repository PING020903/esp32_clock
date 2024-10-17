#ifndef _USER_NVSUSE_H_
#define _USER_NVSUSE_H_

#define CONNECT_SSID_LEN 32
#define CONNECT_PASSWD_LEN 32

esp_err_t EraseWIFIconnetionInfoFromNVS(void);

void SetDefaultConnetionInfo(void);

char *GetConnectWIFI_SSID(void);

char *GetConnectWIFI_PASSWD(void);

int32_t GetConnectWIFI_AuthType(void);

esp_err_t SetConnectionInfo(const char *ssid,
                            const char *passwd,
                            int32_t auth_type);

esp_err_t Read_WIFIconnectionInfo_sta(void);

esp_err_t Write_WIFIconnectionInfo_sta(void);

void User_NVS_Init(void);

int GetTargetTime(const unsigned int target);

esp_err_t updateTargetClockTimeFromNVS(void);

esp_err_t updateTargetClockTimeToNVS(void);

esp_err_t SetTargetTime(const int hour, const int minute, const int second);

void SetDefaultTargetClockInfo(void);

uint64_t GetCountdown(void);

uint64_t *SetCountdown(void);

esp_err_t SetPeriodicCloseLedTargetTime(const uint8_t hour,
                                        const uint8_t minute,
                                        const uint8_t second);

esp_err_t ReadPeriodicCloseLedTargetTime(uint8_t *hour,
                                         uint8_t *minute,
                                         uint8_t *second);

#endif
