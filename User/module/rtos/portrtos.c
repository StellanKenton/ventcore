/************************************************************************************
* @file     : portrtos.c
* @brief    : FreeRTOS provider for the project RTOS abstraction.
* @details  : Maps project task operations to FreeRTOS.
* @author   :
* @date     :
* @version  :
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "portrtos.h"

#include <stddef.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

static int8_t portRtosTaskCreate(const stRepRtosTaskConfig *config)
{
    TaskHandle_t lTaskHandle = NULL;
    BaseType_t lResult;

    if ((config == NULL) || (config->name == NULL) || (config->entry == NULL) ||
        (config->stackSize == 0U) || (config->stackSize > UINT16_MAX) ||
        (config->priority >= configMAX_PRIORITIES)) {
        return REP_RTOS_STATUS_INVALID_PARAM;
    }

    if ((config->handle != NULL) && (*config->handle != NULL)) {
        return REP_RTOS_STATUS_OK;
    }

    lResult = xTaskCreate(config->entry,
                          config->name,
                          (configSTACK_DEPTH_TYPE)config->stackSize,
                          config->argument,
                          (UBaseType_t)config->priority,
                          &lTaskHandle);
    if (lResult != pdPASS) {
        return REP_RTOS_STATUS_ERROR;
    }

    if (config->handle != NULL) {
        *config->handle = (repRtosTaskHandle)lTaskHandle;
    }

    return REP_RTOS_STATUS_OK;
}

static void portRtosTaskDelete(repRtosTaskHandle handle)
{
    vTaskDelete((TaskHandle_t)handle);
}

static int8_t portRtosTaskDelayMs(uint32_t delayMs)
{
    TickType_t lTicks;

    if (delayMs == 0U) {
        taskYIELD();
        return REP_RTOS_STATUS_OK;
    }

    lTicks = pdMS_TO_TICKS(delayMs);
    if (lTicks == 0U) {
        lTicks = 1U;
    }
    vTaskDelay(lTicks);

    return REP_RTOS_STATUS_OK;
}

static uint32_t portRtosGetTickMs(void)
{
    return (uint32_t)(((uint64_t)xTaskGetTickCount() * 1000ULL) / (uint64_t)configTICK_RATE_HZ);
}

static void portRtosEnterCritical(void)
{
    taskENTER_CRITICAL();
}

static void portRtosExitCritical(void)
{
    taskEXIT_CRITICAL();
}

static int8_t portRtosSchedulerStart(void)
{
    vTaskStartScheduler();
    return REP_RTOS_STATUS_ERROR;
}

static const stRepRtosOps gRtosOps = {
    .taskCreate = portRtosTaskCreate,
    .taskDelete = portRtosTaskDelete,
    .taskDelayMs = portRtosTaskDelayMs,
    .getTickMs = portRtosGetTickMs,
    .enterCritical = portRtosEnterCritical,
    .exitCritical = portRtosExitCritical,
    .schedulerStart = portRtosSchedulerStart,
};

const stRepRtosOps *portRtosGetOps(void)
{
    return &gRtosOps;
}

/**************************End of file********************************/
