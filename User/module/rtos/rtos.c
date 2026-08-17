/************************************************************************************
* @file     : rtos.c
* @brief    : Project RTOS abstraction.
* @details  : Forwards project RTOS calls to the selected platform port.
* @author   :
* @date     :
* @version  :
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "rtos.h"

#include <stddef.h>

#include "portrtos.h"

static const stRepRtosOps *repRtosGetOps(void)
{
    return portRtosGetOps();
}

int8_t repRtosTaskCreate(const stRepRtosTaskConfig *config)
{
    const stRepRtosOps *lOps = repRtosGetOps();

    if ((lOps == NULL) || (lOps->taskCreate == NULL)) {
        return REP_RTOS_STATUS_NOT_READY;
    }

    return lOps->taskCreate(config);
}

void repRtosTaskDelete(repRtosTaskHandle handle)
{
    const stRepRtosOps *lOps = repRtosGetOps();

    if ((lOps == NULL) || (lOps->taskDelete == NULL)) {
        return;
    }

    lOps->taskDelete(handle);
}

int8_t repRtosTaskDelayMs(uint32_t delayMs)
{
    const stRepRtosOps *lOps = repRtosGetOps();

    if ((lOps == NULL) || (lOps->taskDelayMs == NULL)) {
        return REP_RTOS_STATUS_NOT_READY;
    }

    return lOps->taskDelayMs(delayMs);
}

int8_t repRtosSchedulerStart(void)
{
    const stRepRtosOps *lOps = repRtosGetOps();

    if ((lOps == NULL) || (lOps->schedulerStart == NULL)) {
        return REP_RTOS_STATUS_NOT_READY;
    }

    return lOps->schedulerStart();
}

/**************************End of file********************************/
