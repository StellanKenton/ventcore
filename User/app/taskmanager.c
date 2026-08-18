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

#include "log.h"
#include "rtos.h"
#include "sfm3119.h"

static const char *const gTaskManagerTag = "taskManager";
static bool gWorkerTasksCreated = false;
static repRtosTaskHandle gDefaultTaskHandle = NULL;
static repRtosTaskHandle gVentTaskHandle = NULL;
static repRtosTaskHandle gSensorTaskHandle = NULL;
static repRtosTaskHandle gHmiTaskHandle = NULL;
static repRtosTaskHandle gAlarmTaskHandle = NULL;

static void defaultTask(void *argument);
static void ventTask(void *argument);
static void sensorTask(void *argument);
static void hmiTask(void *argument);
static void alarmTask(void *argument);
static int8_t taskManagerCreateTask(const stRepRtosTaskConfig *config);

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

static void defaultTask(void *argument)
{
    uint32_t lPreviousWakeMs = repRtosGetTickMs();

    (void)argument;

    for (;;) {
        (void)logProcess((uint16_t)DEFAULT_TASK_INTERVAL_MS);
        (void)repRtosTaskDelayUntilMs(&lPreviousWakeMs, DEFAULT_TASK_INTERVAL_MS);
    }
}

static void ventTask(void *argument)
{
    uint32_t lPreviousWakeMs = repRtosGetTickMs();

    (void)argument;

    for (;;) {
        (void)repRtosTaskDelayUntilMs(&lPreviousWakeMs, VENT_TASK_INTERVAL_MS);
    }
}

static void sensorTask(void *argument)
{
    uint32_t lPreviousWakeMs;

    (void)argument;
    (void)sfm3119Init();
    lPreviousWakeMs = repRtosGetTickMs();

    for (;;) {
        (void)sfm3119Process();
        (void)repRtosTaskDelayUntilMs(&lPreviousWakeMs, SENSOR_TASK_INTERVAL_MS);
    }
}

static void hmiTask(void *argument)
{
    uint32_t lPreviousWakeMs = repRtosGetTickMs();

    (void)argument;

    for (;;) {
        (void)repRtosTaskDelayUntilMs(&lPreviousWakeMs, HMI_TASK_INTERVAL_MS);
    }
}

static void alarmTask(void *argument)
{
    uint32_t lPreviousWakeMs = repRtosGetTickMs();

    (void)argument;

    for (;;) {
        (void)repRtosTaskDelayUntilMs(&lPreviousWakeMs, ALARM_TASK_INTERVAL_MS);
    }
}

static int8_t taskManagerCreateTask(const stRepRtosTaskConfig *config)
{
    int8_t lStatus = repRtosTaskCreate(config);

    if (lStatus != REP_RTOS_STATUS_OK) {
        LOG_T(gTaskManagerTag,
              "task create failed name=%s priority=%lu stack=%lu status=%d",
              config->name,
              (unsigned long)config->priority,
              (unsigned long)config->stackSize,
              (int)lStatus);
    }

    return lStatus;
}

bool WorkerTasksRegister(void)
{
    uint32_t lIndex;
    bool lAllCreated = true;

    if (gWorkerTasksCreated) {
        return true;
    }

    for (lIndex = 0U; lIndex < (sizeof(gWorkerTaskConfigs) / sizeof(gWorkerTaskConfigs[0])); lIndex++) {
        if (taskManagerCreateTask(&gWorkerTaskConfigs[lIndex]) != REP_RTOS_STATUS_OK) {
            lAllCreated = false;
        }
    }

    if (!lAllCreated) {
        LOG_T(gTaskManagerTag, "worker task registration failed");
        return false;
    }

    gWorkerTasksCreated = true;
    LOG_I(gTaskManagerTag, "worker tasks ready");
    return true;
}

/**************************End of file********************************/
