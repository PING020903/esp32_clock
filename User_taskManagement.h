#pragma once
#ifndef _USER_TASKMANAGEMENT_H_
#define _USER_TASKMANAGEMENT_H_

#include "User_main.h"

#if (LED_DRIVER_VERSION_MAJOR == LED_TASK_VERSION)
#ifndef LED_TASK
#define LED_TASK_NAME "LED_TASK"
#define LED_TASK_PRIORITY 12
#define LED_TASK_STACK_SIZE 2048
#endif
#endif

#if (BELL_DRIVER_VERSION_MAJOR == BELL_TASK_VERSION)
#ifndef BELL_TASK
#define BELL_TASK_NAME "BELL_TASK"
#define BELL_TASK_PRIORITY 1
#define BELL_TASK_STACK_SIZE 768
#endif
#endif

#ifndef MAIN_TASK
#define MAIN_TASK_NAME "MAIN_TASK"
#define MAIN_TASK_PRIORITY 6
#define MAIN_TASK_STACK_SIZE 4096
#endif

#ifndef GPIO_ISR_TASK
#define GPIO_ISR_TASK_NAME "GPIO_ISR_TASK"
#define GPIO_ISR_TASK_PRIORITY 10
#define GPIO_ISR_TASK_STACK_SIZE 2048
#endif

#ifndef DEBUG_TASK
#define DEBUG_TASK_NAME "UART_DBG_TASK"
#endif

#ifndef HTTP_WIFI_CONNECT_TASK
#define HTTP_WIFI_CONNECT_TASK_NAME "HTTP_WIFI_TASK"
#define HTTP_WIFI_CONNECT_TASK_PRIORITY 1
#define HTTP_WIFI_CONNECT_TASK_STACK_SIZE 3072
#endif

#ifndef WIFI_CONTORL_TASK
#define AP_CONTROL_TASK_NAME "AP_CTRL_TASK"
#define AP_CONTROL_TASK_PRIORITY 1
#define AP_CONTROL_TASK_STACK_SIZE 3072
#endif

#if 0
#ifndef MQTT_TASK_NAME
#define MQTT_TASK_NAME "MQTT_TASK"
#endif
#endif

TaskHandle_t *User_NewTask(void);

TaskHandle_t User_GetTaskHandle(const char *taskName);

size_t User_GetTaskCnt(void);

#endif
