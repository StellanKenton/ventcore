/************************************************************************************
* @file     : physalarmmanager.c
* @brief    : Physiological alarm manager implementation.
* @details  : Registers detectors, dispatches enabled alarms, and publishes states.
* @author   :
* @date     : 2026-09-07
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "physalarmmanager.h"

#include <stddef.h>

#include "physalarmvent.h"
#include "rtos.h"

/* Set enabled to false here to omit a detector from alarm processing. */
static stPhysAlarmRegistration gPhysAlarmRegistrations[PHYS_ALARM_COUNT] = {
    [PHYS_ALARM_AIRWAY_PRESSURE_HIGH] = {
        .type = PHYS_ALARM_AIRWAY_PRESSURE_HIGH,
        .enabled = true,
        .detector = physAlarmVentAirwayPressureHighDetect,
        .active = false,
    },
    [PHYS_ALARM_AIRWAY_PRESSURE_LOW] = {
        .type = PHYS_ALARM_AIRWAY_PRESSURE_LOW,
        .enabled = true,
        .detector = physAlarmVentAirwayPressureLowDetect,
        .active = false,
    },
    [PHYS_ALARM_EXHALED_VOLUME_HIGH] = {
        .type = PHYS_ALARM_EXHALED_VOLUME_HIGH,
        .enabled = true,
        .detector = physAlarmVentExhaledVolumeHighDetect,
        .active = false,
    },
    [PHYS_ALARM_EXHALED_VOLUME_LOW] = {
        .type = PHYS_ALARM_EXHALED_VOLUME_LOW,
        .enabled = true,
        .detector = physAlarmVentExhaledVolumeLowDetect,
        .active = false,
    },
};

/** Update an alarm state while allowing readers from other tasks. */
static void physAlarmManagerStateSet(stPhysAlarmRegistration *registration, bool active)
{
    if (registration->active == active) {
        return;
    }
    repRtosEnterCritical();
    registration->active = active;
    repRtosExitCritical();
}

void physAlarmManagerInit(void)
{
    uint32_t lIndex;

    for (lIndex = 0U; lIndex < PHYS_ALARM_COUNT; lIndex++) {
        gPhysAlarmRegistrations[lIndex].active = false;
    }
    physAlarmVentInit();
}

void physAlarmManagerProcess(uint32_t nowMs)
{
    uint32_t lIndex;

    for (lIndex = 0U; lIndex < PHYS_ALARM_COUNT; lIndex++) {
        stPhysAlarmRegistration *lRegistration =
            &gPhysAlarmRegistrations[lIndex];

        if ((!lRegistration->enabled) ||
            (lRegistration->detector == NULL)) {
            physAlarmManagerStateSet(lRegistration, false);
            continue;
        }
        physAlarmManagerStateSet(lRegistration,
                                 lRegistration->detector(nowMs));
    }
}

bool physAlarmManagerStateGet(ePhysAlarmType type)
{
    bool lActive;

    if ((uint32_t)type >= PHYS_ALARM_COUNT) {
        return false;
    }
    repRtosEnterCritical();
    lActive = gPhysAlarmRegistrations[type].active;
    repRtosExitCritical();
    return lActive;
}

/*************************************** End of file ********************************/
