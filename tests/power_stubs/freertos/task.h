#pragma once
#include <stdint.h>
void vTaskDelay(uint32_t ticks);
typedef void *TaskHandle_t;
TaskHandle_t xTaskGetCurrentTaskHandle(void);
void vTaskSuspend(TaskHandle_t task);
void vTaskResume(TaskHandle_t task);
