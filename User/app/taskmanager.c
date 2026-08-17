/************************************************************************************
* @file     : taskmanager.c
* @brief    : Project task wiring.
* @details  : Creates the five project worker tasks.
* @author   :
* @date     :
* @version  :
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "taskmanager.h"

#include <stddef.h>

#include "rtos.h"

static repRtosTaskHandle gTaskManagerHandle = NULL;
static repRtosTaskHandle gDefaultTaskHandle = NULL;
static repRtosTaskHandle gVentTaskHandle = NULL;
static repRtosTaskHandle gSensorTaskHandle = NULL;
static repRtosTaskHandle gHmiTaskHandle = NULL;
static repRtosTaskHandle gAlarmTaskHandle = NULL;

static void taskManagerTask(void *argument);
static void defaultTask(void *argument);
static void ventTask(void *argument);
static void sensorTask(void *argument);
static void hmiTask(void *argument);
static void alarmTask(void *argument);
static bool taskManagerCreateWorkerTasks(void);

static const stRepRtosTaskConfig gTaskManagerConfig = {
    .name = "taskManager",
    .entry = taskManagerTask,
    .argument = NULL,
    .stackSize = TASK_MANAGER_STACK_SIZE,
    .priority = TASK_MANAGER_PRIORITY,
    .handle = &gTaskManagerHandle,
};

static const stRepRtosTaskConfig gWorkerTaskConfigs[] = {
    {
        .name = "defaultTask",
        .entry = defaultTask,
        .argument = NULL,
        .stackSize = DEFAULT_TASK_STACK_SIZE,
        .priority = DEFAULT_TASK_PRIORITY,
        .handle = &gDefaultTaskHandle,
    },
    {
        .name = "VentTask",
        .entry = ventTask,
        .argument = NULL,
        .stackSize = VENT_TASK_STACK_SIZE,
        .priority = VENT_TASK_PRIORITY,
        .handle = &gVentTaskHandle,
    },
    {
        .name = "SensorTask",
        .entry = sensorTask,
        .argument = NULL,
        .stackSize = SENSOR_TASK_STACK_SIZE,
        .priority = SENSOR_TASK_PRIORITY,
        .handle = &gSensorTaskHandle,
    },
    {
        .name = "HMITask",
        .entry = hmiTask,
        .argument = NULL,
        .stackSize = HMI_TASK_STACK_SIZE,
        .priority = HMI_TASK_PRIORITY,
        .handle = &gHmiTaskHandle,
    },
    {
        .name = "AlarmTask",
        .entry = alarmTask,
        .argument = NULL,
        .stackSize = ALARM_TASK_STACK_SIZE,
        .priority = ALARM_TASK_PRIORITY,
        .handle = &gAlarmTaskHandle,
    },
};

static void taskManagerTask(void *argument)
{
    (void)argument;

    if (!taskManagerCreateWorkerTasks()) {
        for (;;) {
            (void)repRtosTaskDelayMs(DEFAULT_TASK_INTERVAL_MS);
        }
    }

    repRtosTaskDelete(NULL);
}

static void defaultTask(void *argument)
{
    (void)argument;

    for (;;) {
        (void)repRtosTaskDelayMs(DEFAULT_TASK_INTERVAL_MS);
    }
}

static void ventTask(void *argument)
{
    (void)argument;

    for (;;) {
        (void)repRtosTaskDelayMs(VENT_TASK_INTERVAL_MS);
    }
}

static void sensorTask(void *argument)
{
    (void)argument;

    for (;;) {
        (void)repRtosTaskDelayMs(SENSOR_TASK_INTERVAL_MS);
    }
}

static void hmiTask(void *argument)
{
    (void)argument;

    for (;;) {
        (void)repRtosTaskDelayMs(HMI_TASK_INTERVAL_MS);
    }
}

static void alarmTask(void *argument)
{
    (void)argument;

    for (;;) {
        (void)repRtosTaskDelayMs(ALARM_TASK_INTERVAL_MS);
    }
}

static bool taskManagerCreateWorkerTasks(void)
{
    uint32_t lIndex;

    for (lIndex = 0U; lIndex < (sizeof(gWorkerTaskConfigs) / sizeof(gWorkerTaskConfigs[0])); lIndex++) {
        if (repRtosTaskCreate(&gWorkerTaskConfigs[lIndex]) != REP_RTOS_STATUS_OK) {
            return false;
        }
    }

    return true;
}

bool taskManagerStart(void)
{
    return repRtosTaskCreate(&gTaskManagerConfig) == REP_RTOS_STATUS_OK;
}

/**************************End of file********************************/
