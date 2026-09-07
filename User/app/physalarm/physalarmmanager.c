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
    [PHYS_ALARM_PEEP_HIGH] = {
        .type = PHYS_ALARM_PEEP_HIGH,
        .enabled = true,
        .detector = physAlarmVentPeepHighDetect,
        .active = false,
    },
    [PHYS_ALARM_PEEP_LOW] = {
        .type = PHYS_ALARM_PEEP_LOW,
        .enabled = true,
        .detector = physAlarmVentPeepLowDetect,
        .active = false,
    },
    /* Keep placeholder detectors disabled until implemented. */
    [PHYS_ALARM_PIPELINE_BLOCKAGE] = {
        .type = PHYS_ALARM_PIPELINE_BLOCKAGE,
        .enabled = false,
        .detector = physAlarmVentPipelineBlockageDetect,
        .active = false,
    },
    [PHYS_ALARM_INSP_BRANCH_BLOCKAGE] = {
        .type = PHYS_ALARM_INSP_BRANCH_BLOCKAGE,
        .enabled = false,
        .detector = physAlarmVentInspBranchBlockageDetect,
        .active = false,
    },
    [PHYS_ALARM_CPAP_TOO_HIGH] = {
        .type = PHYS_ALARM_CPAP_TOO_HIGH,
        .enabled = true,
        .detector = physAlarmVentCpapTooHighDetect,
        .active = false,
    },
    [PHYS_ALARM_PIPELINE_LEAK] = {
        .type = PHYS_ALARM_PIPELINE_LEAK,
        .enabled = false,
        .detector = physAlarmVentPipelineLeakDetect,
        .active = false,
    },
    [PHYS_ALARM_PIPELINE_DISCONNECT] = {
        .type = PHYS_ALARM_PIPELINE_DISCONNECT,
        .enabled = false,
        .detector = physAlarmVentPipelineDisconnectDetect,
        .active = false,
    },
    [PHYS_ALARM_PRESSURE_LIMIT] = {
        .type = PHYS_ALARM_PRESSURE_LIMIT,
        .enabled = false,
        .detector = physAlarmVentPressureLimitDetect,
        .active = false,
    },
    [PHYS_ALARM_VOLUME_LIMIT] = {
        .type = PHYS_ALARM_VOLUME_LIMIT,
        .enabled = false,
        .detector = physAlarmVentVolumeLimitDetect,
        .active = false,
    },
    [PHYS_ALARM_INSP_PRESS_NOT_REACHED] = {
        .type = PHYS_ALARM_INSP_PRESS_NOT_REACHED,
        .enabled = false,
        .detector = physAlarmVentInspPressNotReachedDetect,
        .active = false,
    },
    [PHYS_ALARM_TIDAL_VOL_NOT_REACHED] = {
        .type = PHYS_ALARM_TIDAL_VOL_NOT_REACHED,
        .enabled = false,
        .detector = physAlarmVentTidalVolNotReachedDetect,
        .active = false,
    },
    [PHYS_ALARM_SIGH_CYCLE_PRESS_LIMIT] = {
        .type = PHYS_ALARM_SIGH_CYCLE_PRESS_LIMIT,
        .enabled = false,
        .detector = physAlarmVentSighCyclePressLimitDetect,
        .active = false,
    },
    [PHYS_ALARM_O2_SUPPLY_INSUFFICIENT] = {
        .type = PHYS_ALARM_O2_SUPPLY_INSUFFICIENT,
        .enabled = false,
        .detector = physAlarmVentO2SupplyInsufficientDetect,
        .active = false,
    },
    [PHYS_ALARM_INSP_TIME_TOO_LONG] = {
        .type = PHYS_ALARM_INSP_TIME_TOO_LONG,
        .enabled = false,
        .detector = physAlarmVentInspTimeTooLongDetect,
        .active = false,
    },
    [PHYS_ALARM_INHALED_GAS_TEMP_HIGH] = {
        .type = PHYS_ALARM_INHALED_GAS_TEMP_HIGH,
        .enabled = false,
        .detector = physAlarmVentInhaledGasTempHighDetect,
        .active = false,
    },
    [PHYS_ALARM_AMV_TARGET_NOT_REACHED] = {
        .type = PHYS_ALARM_AMV_TARGET_NOT_REACHED,
        .enabled = false,
        .detector = physAlarmVentAmvTargetNotReachedDetect,
        .active = false,
    },
    [PHYS_ALARM_O2_FLOW_NOT_REACHED] = {
        .type = PHYS_ALARM_O2_FLOW_NOT_REACHED,
        .enabled = false,
        .detector = physAlarmVentO2FlowNotReachedDetect,
        .active = false,
    },
    [PHYS_ALARM_PAT_FLOW_SENSOR_FAULT] = {
        .type = PHYS_ALARM_PAT_FLOW_SENSOR_FAULT,
        .enabled = false,
        .detector = physAlarmVentPatFlowSensorFaultDetect,
        .active = false,
    },
    [PHYS_ALARM_PAT_PRESS_SENSOR_FAULT] = {
        .type = PHYS_ALARM_PAT_PRESS_SENSOR_FAULT,
        .enabled = false,
        .detector = physAlarmVentPatPressSensorFaultDetect,
        .active = false,
    },
    [PHYS_ALARM_MECH_PIPELINE_DISCONNECT] = {
        .type = PHYS_ALARM_MECH_PIPELINE_DISCONNECT,
        .enabled = false,
        .detector = physAlarmVentMechPipelineDisconnectDetect,
        .active = false,
    },
    [PHYS_ALARM_EXP_BRANCH_BLOCKAGE] = {
        .type = PHYS_ALARM_EXP_BRANCH_BLOCKAGE,
        .enabled = false,
        .detector = physAlarmVentExpBranchBlockageDetect,
        .active = false,
    },
    [PHYS_ALARM_MAX_INSP_NEG_PRESSURE] = {
        .type = PHYS_ALARM_MAX_INSP_NEG_PRESSURE,
        .enabled = false,
        .detector = physAlarmVentMaxInspNegPressureDetect,
        .active = false,
    },
    [PHYS_ALARM_INSP_PRESSURE_NOT_RELEASED] = {
        .type = PHYS_ALARM_INSP_PRESSURE_NOT_RELEASED,
        .enabled = false,
        .detector = physAlarmVentInspPressureNotReleasedDetect,
        .active = false,
    },
    [PHYS_ALARM_O2_SOURCE_FAILURE] = {
        .type = PHYS_ALARM_O2_SOURCE_FAILURE,
        .enabled = false,
        .detector = physAlarmVentO2SourceFailureDetect,
        .active = false,
    },
    [PHYS_ALARM_PROXIMAL_PRESS_TUBE_DISCONNECT] = {
        .type = PHYS_ALARM_PROXIMAL_PRESS_TUBE_DISCONNECT,
        .enabled = false,
        .detector = physAlarmVentProximalPressTubeDisconnectDetect,
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

    if (type >= PHYS_ALARM_COUNT) {
        return false;
    }
    repRtosEnterCritical();
    lActive = gPhysAlarmRegistrations[type].active;
    repRtosExitCritical();
    return lActive;
}

/*************************************** End of file ********************************/
