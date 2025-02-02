#pragma once
#ifndef _USER_MAIN_H_
#define _USER_MAIN_H_

#define LED_TASK_VERSION 0
#define LED_TICK_VERSION 1

#define BELL_TASK_VERSION 0
#define BELL_TICK_VERSION 1

// 当前LED驱动版本
#ifndef LED_DRIVER_VERSION_MAJOR
#define LED_DRIVER_VERSION_MAJOR LED_TICK_VERSION
#endif

#ifndef BELL_DRIVER_VERSION_MAJOR
#define BELL_DRIVER_VERSION_MAJOR BELL_TICK_VERSION
#endif


#ifndef ISR_SAFE
#define ISR_SAFE IRAM_ATTR
#endif

#if (LED_DRIVER_VERSION_MAJOR == LED_TICK_VERSION)
#include "esp_freertos_hooks.h"
#define DEFAULT_LED_TASK_CORE 1

/// @brief 
/// @param type 1:驱动LED, 0:关闭LED
/// @return 
esp_freertos_tick_cb_t GetLedDriverTask(const int type);
#endif

#include "cJSON.h"
cJSON *GetBoardInfo_JSON(void);

#endif