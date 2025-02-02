#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdio.h"
#include "string.h"
#include "User_taskManagement.h"

static size_t TaskCnt = 0; // 记录用户创建任务的数量

static TaskHandle_t *TaskMap = NULL;

TaskHandle_t *User_NewTask(void)
{
    TaskHandle_t *TempMap;
    TaskCnt++;
    TempMap = realloc(TaskMap, TaskCnt * sizeof(TaskHandle_t));
    if (TempMap != NULL)
    {
        TaskMap = TempMap;
        return &TaskMap[TaskCnt - 1];
    }

    printf("TaskMap realloc failed\n");
    return NULL;
}

TaskHandle_t User_GetTaskHandle(const char *taskName)
{
    if (taskName == NULL || TaskCnt == 0 || TaskMap == NULL)
        return NULL;

    for (size_t index = 0; index < TaskCnt; index++)
        if (strcmp(taskName, pcTaskGetName(TaskMap[index])) == 0)
            return TaskMap[index];
    return NULL;
}

size_t User_GetTaskCnt(void)
{
    return TaskCnt;
}