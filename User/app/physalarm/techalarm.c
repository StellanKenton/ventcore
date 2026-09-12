/************************************************************************************
* @file     : techalarm.c
* @brief    : Technical alarm detectors and current status collection.
* @details  : Current alarm states using the legacy MCM wire mapping.
* @author   :
* @date     : 2026-09-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "techalarm.h"

#include <string.h>
#include "controldata.h"
#include "monitorengine.h"
#include "phasecontroller.h"
#include "rtos.h"

static stPhysAlarmVentRuntime gTechAlarmPeepRuntime[2];
static stPhysAlarmVentRuntime gTechAlarmCpapRuntime;

/** Reset detector history at alarm manager initialization. */
void techAlarmInit(void) {
    (void)memset(gTechAlarmPeepRuntime, 0, sizeof(gTechAlarmPeepRuntime));
    (void)memset(&gTechAlarmCpapRuntime, 0, sizeof(gTechAlarmCpapRuntime));
}

/** Check one completed PEEP per inspiration and time strict recovery each call. */
static bool techAlarmPeepDetect(ePhysAlarmType type, uint32_t nowMs) {
    stPhysAlarmVentRuntime *lRuntime = &gTechAlarmPeepRuntime[type == PHYS_ALARM_PEEP_HIGH ? 0U : 1U];
    stBreathPlan lPlan;
    stBreathResult lResult;
    ePhaseControllerState lPhase;
    float lPeep;
    float lLimit;
    bool lAvailable;
    bool lHigh = (type == PHYS_ALARM_PEEP_HIGH);
    bool lRecovered;

    /* Snapshot VentTask publications together before evaluating in AlarmTask. */
    repRtosEnterCritical();
    lPhase = phaseControllerStateGet();
    lAvailable = (phaseControllerActivePlanGet(&lPlan) == PHASE_CONTROL_SUCCESS) &&
                 (monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    lPeep = monitorEngineGet(MONITOR_DYN_PEEP);
    repRtosExitCritical();
    if ((lPhase != PHASE_INSP) && (lPhase != PHASE_EXP)) {
        (void)memset(lRuntime, 0, sizeof(*lRuntime));
        return false;
    }
    if (!lAvailable || ((lResult.validMask & BREATH_RESULT_VALID_COMPLETE) == 0U)) {
        lRuntime->timing = false;
        return lRuntime->active;
    }
    lLimit = lPlan.peepCmh2o + (lHigh ? PHYS_ALARM_PEEP_HIGH_OFFSET_CMH2O :
                                      -PHYS_ALARM_PEEP_LOW_OFFSET_CMH2O);
    if ((lPhase == PHASE_INSP) && (lResult.sequence != lPlan.sequence) &&
        (!lRuntime->sequenceInitialized ||
         (lRuntime->processedSequence != lPlan.sequence))) {
        lRuntime->processedSequence = lPlan.sequence;
        lRuntime->sequenceInitialized = true;
        if (lHigh ? (lPeep > lLimit) : (lPeep < lLimit)) {
            lRuntime->active = true;
            lRuntime->timing = false;
        }
    }
    lRecovered = lHigh ? (lPeep < lLimit) : (lPeep > lLimit);
    if (!lRuntime->active || !lRecovered) {
        lRuntime->timing = false;
    } else if (!lRuntime->timing) {
        lRuntime->referenceMs = nowMs;
        lRuntime->timing = true;
    } else if ((nowMs - lRuntime->referenceMs) >= PHYS_ALARM_PEEP_RECOVERY_MS) {
        lRuntime->active = false;
        lRuntime->timing = false;
    }
    return lRuntime->active;
}

bool techAlarmPeepHighDetect(uint32_t nowMs) {
    return techAlarmPeepDetect(PHYS_ALARM_PEEP_HIGH, nowMs);
}

bool techAlarmPeepLowDetect(uint32_t nowMs) {
    return techAlarmPeepDetect(PHYS_ALARM_PEEP_LOW, nowMs);
}

/** Placeholder: Patient circuit blockage - H. */
bool techAlarmPipelineBlockageDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Inspiratory branch blockage - M. */
bool techAlarmInspBranchBlockageDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Time both circuit pressures across breaths, with separate recovery hysteresis. */
bool techAlarmCpapTooHighDetect(uint32_t nowMs) {
    stPhysAlarmVentRuntime *lRuntime = &gTechAlarmCpapRuntime;
    stBreathPlan lPlan;
    ePhaseControllerState lPhase;
    float lInspPressure;
    float lPatientPressure;
    float lLimit;
    uint32_t lDurationMs;
    bool lAvailable;
    bool lCondition;

    /* Read the plan and both pressures from the same VentTask publication. */
    repRtosEnterCritical();
    lPhase = phaseControllerStateGet();
    lAvailable = (phaseControllerActivePlanGet(&lPlan) == PHASE_CONTROL_SUCCESS);
    lInspPressure = controlDataGet(INSP_REAL_PRS);
    lPatientPressure = controlDataGet(PAT_REAL_PRS);
    repRtosExitCritical();
    if ((lPhase != PHASE_INSP) && (lPhase != PHASE_EXP)) {
        (void)memset(lRuntime, 0, sizeof(*lRuntime));
        return false;
    }
    if (!lAvailable) {
        lRuntime->timing = false;
        return lRuntime->active;
    }
    if (lRuntime->active) {
        lLimit = lPlan.peepCmh2o + PHYS_ALARM_CPAP_RECOVERY_OFFSET_CMH2O;
        lCondition = (lInspPressure < lLimit) && (lPatientPressure < lLimit);
        lDurationMs = PHYS_ALARM_CPAP_RECOVERY_MS;
    } else {
        lLimit = lPlan.peepCmh2o + PHYS_ALARM_CPAP_HIGH_OFFSET_CMH2O;
        lCondition = (lInspPressure > lLimit) && (lPatientPressure > lLimit);
        lDurationMs = PHYS_ALARM_CPAP_CONFIRM_MS;
    }
    if (!lCondition) {
        lRuntime->timing = false;
    } else if (!lRuntime->timing) {
        lRuntime->referenceMs = nowMs;
        lRuntime->timing = true;
    } else if ((nowMs - lRuntime->referenceMs) >= lDurationMs) {
        lRuntime->active = !lRuntime->active;
        lRuntime->timing = false;
    }
    return lRuntime->active;
}

/** Placeholder: Circuit leak - L. */
bool techAlarmPipelineLeakDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Patient circuit disconnection - H. */
bool techAlarmPipelineDisconnectDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Pressure limitation - L. */
bool techAlarmPressureLimitDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Volume limitation - L. */
bool techAlarmVolumeLimitDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Inspiratory pressure not reached - L. */
bool techAlarmInspPressNotReachedDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Tidal volume not reached - L. */
bool techAlarmTidalVolNotReachedDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Sigh cycle pressure limitation - L. */
bool techAlarmSighCyclePressLimitDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Insufficient oxygen supply - H. */
bool techAlarmO2SupplyInsufficientDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Inspiratory time too long - L. */
bool techAlarmInspTimeTooLongDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Inhaled gas temperature too high - H. */
bool techAlarmInhaledGasTempHighDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: AMV target not reached - L. */
bool techAlarmAmvTargetNotReachedDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Oxygen therapy flow not reached - H. */
bool techAlarmO2FlowNotReachedDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Patient flow sensor fault - H. */
bool techAlarmPatFlowSensorFaultDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Patient pressure sensor fault - H. */
bool techAlarmPatPressSensorFaultDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Machine circuit disconnection - H. */
bool techAlarmMechPipelineDisconnectDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Expiratory branch blockage - M. */
bool techAlarmExpBranchBlockageDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Maximum inspiratory negative pressure - H. */
bool techAlarmMaxInspNegPressureDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Inspiratory pressure not released - H. */
bool techAlarmInspPressureNotReleasedDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Oxygen source failure - H. */
bool techAlarmO2SourceFailureDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Proximal pressure sampling tube disconnection - H. */
bool techAlarmProximalPressTubeDisconnectDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Collect current states without latching or clearing short-lived events. */
void techAlarmSnapshotGet(stMcmTechAlarmStatusSnapshot *status) {
    if (status == NULL) {
        return;
    }
    (void)memset(status, 0, sizeof(*status));
    repRtosEnterCritical();
    if (physAlarmManagerStateGet(PHYS_ALARM_PEEP_HIGH)) {
        status->phys.value |= (1UL << PHYSIO_FAULT_PEEP_TOO_HIGH);
    }
    if (physAlarmManagerStateGet(PHYS_ALARM_PEEP_LOW)) {
        status->phys.value |= (1UL << PHYSIO_FAULT_PEEP_TOO_LOW);
    }
    if (physAlarmManagerStateGet(PHYS_ALARM_PIPELINE_BLOCKAGE)) {
        status->phys.value |= (1UL << PHYSIO_FAULT_PIPELINE_BLOCKAGE);
    }
    if (physAlarmManagerStateGet(PHYS_ALARM_INSP_BRANCH_BLOCKAGE)) {
        status->phys.value |= (1UL << PHYSIO_FAULT_INSP_BRANCH_BLOCKAGE);
    }
    if (physAlarmManagerStateGet(PHYS_ALARM_CPAP_TOO_HIGH)) {
        status->phys.value |= (1UL << PHYSIO_FAULT_CPAP_TOO_HIGH);
    }
    if (physAlarmManagerStateGet(PHYS_ALARM_PIPELINE_LEAK)) {
        status->phys.value |= (1UL << PHYSIO_FAULT_PIPELINE_LEAK);
    }
    if (physAlarmManagerStateGet(PHYS_ALARM_PIPELINE_DISCONNECT)) {
        status->phys.value |= (1UL << PHYSIO_FAULT_PIPELINE_DISCONNECT);
    }
    if (physAlarmManagerStateGet(PHYS_ALARM_PRESSURE_LIMIT)) {
        status->phys.value |= (1UL << PHYSIO_FAULT_PRESSURE_LIMIT);
    }
    if (physAlarmManagerStateGet(PHYS_ALARM_VOLUME_LIMIT)) {
        status->phys.value |= (1UL << PHYSIO_FAULT_VOLUME_LIMIT);
    }
    if (physAlarmManagerStateGet(PHYS_ALARM_INSP_PRESS_NOT_REACHED)) {
        status->phys.value |= (1UL << PHYSIO_FAULT_INSP_PRESS_NOT_REACHED);
    }
    if (physAlarmManagerStateGet(PHYS_ALARM_TIDAL_VOL_NOT_REACHED)) {
        status->phys.value |= (1UL << PHYSIO_FAULT_TIDAL_VOL_NOT_REACHED);
    }
    if (physAlarmManagerStateGet(PHYS_ALARM_SIGH_CYCLE_PRESS_LIMIT)) {
        status->phys.value |= (1UL << PHYSIO_FAULT_SIGH_CYCLE_PRESS_LIMIT);
    }
    if (physAlarmManagerStateGet(PHYS_ALARM_O2_SUPPLY_INSUFFICIENT)) {
        status->tech.value |= (1UL << TECH_FAULT_O2_SOURCE_LOW);
    }
    if (physAlarmManagerStateGet(PHYS_ALARM_INSP_TIME_TOO_LONG)) {
        status->phys.value |= (1UL << PHYSIO_FAULT_INSP_TIME_TOO_LONG);
    }
    if (physAlarmManagerStateGet(PHYS_ALARM_INHALED_GAS_TEMP_HIGH)) {
        status->phys.value |= (1UL << PHYSIO_FAULT_INHALED_GAS_TEMP_HIGH);
    }
    if (physAlarmManagerStateGet(PHYS_ALARM_AMV_TARGET_NOT_REACHED)) {
        status->phys.value |= (1UL << PHYSIO_FAULT_AMV_TARGET_NOT_REACHED);
    }
    if (physAlarmManagerStateGet(PHYS_ALARM_O2_FLOW_NOT_REACHED)) {
        status->phys.value |= (1UL << PHYSIO_FAULT_O2_FLOW_NOT_REACHED);
    }
    if (physAlarmManagerStateGet(PHYS_ALARM_PAT_FLOW_SENSOR_FAULT)) {
        status->phys.value |= (1UL << PHYSIO_FAULT_PAT_FLOW_SENSOR_FAULT);
    }
    if (physAlarmManagerStateGet(PHYS_ALARM_PAT_PRESS_SENSOR_FAULT)) {
        status->phys.value |= (1UL << PHYSIO_FAULT_PAT_PRESS_SENSOR_FAULT);
    }
    if (physAlarmManagerStateGet(PHYS_ALARM_MECH_PIPELINE_DISCONNECT)) {
        status->phys.value |= (1UL << PHYSIO_FAULT_MECH_PIPELINE_DISCONNECT);
    }
    if (physAlarmManagerStateGet(PHYS_ALARM_EXP_BRANCH_BLOCKAGE)) {
        status->phys.value |= (1UL << PHYSIO_FAULT_EXP_BRANCH_BLOCKAGE);
    }
    if (physAlarmManagerStateGet(PHYS_ALARM_MAX_INSP_NEG_PRESSURE)) {
        status->phys.value |= (1UL << PHYSIO_FAULT_MAX_INSP_NEG_PRESSURE);
    }
    if (physAlarmManagerStateGet(PHYS_ALARM_INSP_PRESSURE_NOT_RELEASED)) {
        status->phys.value |= (1UL << PHYSIO_FAULT_INSP_PRESSURE_NOT_RELEASED);
    }
    if (physAlarmManagerStateGet(PHYS_ALARM_O2_SOURCE_FAILURE)) {
        status->phys.value |= (1UL << PHYSIO_FAULT_O2_SOURCE_FAILURE);
    }
    if (physAlarmManagerStateGet(PHYS_ALARM_PROXIMAL_PRESS_TUBE_DISCONNECT)) {
        status->phys.value |= (1UL << PHYSIO_FAULT_PROXIMAL_PRESS_TUBE_DISCONNECT);
    }
    repRtosExitCritical();
}

/*************************************** End of file ********************************/
