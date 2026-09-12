/************************************************************************************
* @file     : techphys.c
* @brief    : Ventilation-related technical alarm detectors.
* @details  : Current alarm states using the legacy MCM wire mapping.
* @author   :
* @date     : 2026-09-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "techphys.h"

#include <string.h>
#include "controldata.h"
#include "monitorengine.h"
#include "phasecontroller.h"
#include "rtos.h"

static stTechPhysRuntime gTechPhysPeepRuntime[2];
static stTechPhysRuntime gTechPhysCpapRuntime;

/** Reset detector history at alarm manager initialization. */
void techPhysInit(void) {
    (void)memset(gTechPhysPeepRuntime, 0, sizeof(gTechPhysPeepRuntime));
    (void)memset(&gTechPhysCpapRuntime, 0, sizeof(gTechPhysCpapRuntime));
}

/** Check one completed PEEP per inspiration and time strict recovery each call. */
static bool techPhysPeepDetect(bool high, uint32_t nowMs) {
    stTechPhysRuntime *lRuntime = &gTechPhysPeepRuntime[high ? 0U : 1U];
    stBreathPlan lPlan;
    stBreathResult lResult;
    ePhaseControllerState lPhase;
    float lPeep;
    float lLimit;
    bool lAvailable;
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
    lLimit = lPlan.peepCmh2o + (high ? TECH_PHYS_PEEP_HIGH_OFFSET_CMH2O :
                                      -TECH_PHYS_PEEP_LOW_OFFSET_CMH2O);
    if ((lPhase == PHASE_INSP) && (lResult.sequence != lPlan.sequence) &&
        (!lRuntime->sequenceInitialized ||
         (lRuntime->processedSequence != lPlan.sequence))) {
        lRuntime->processedSequence = lPlan.sequence;
        lRuntime->sequenceInitialized = true;
        if (high ? (lPeep > lLimit) : (lPeep < lLimit)) {
            lRuntime->active = true;
            lRuntime->timing = false;
        }
    }
    lRecovered = high ? (lPeep < lLimit) : (lPeep > lLimit);
    if (!lRuntime->active || !lRecovered) {
        lRuntime->timing = false;
    } else if (!lRuntime->timing) {
        lRuntime->referenceMs = nowMs;
        lRuntime->timing = true;
    } else if ((nowMs - lRuntime->referenceMs) >= TECH_PHYS_PEEP_RECOVERY_MS) {
        lRuntime->active = false;
        lRuntime->timing = false;
    }
    return lRuntime->active;
}

/** Detect completed-cycle PEEP with timed recovery. */
bool techPhysPeepHighDetect(uint32_t nowMs) {
    return techPhysPeepDetect(true, nowMs);
}

/** Detect completed-cycle PEEP with timed recovery. */
bool techPhysPeepLowDetect(uint32_t nowMs) {
    return techPhysPeepDetect(false, nowMs);
}

/** Placeholder: Patient circuit blockage - H. */
bool techPhysPipelineBlockageDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Inspiratory branch blockage - M. */
bool techPhysInspBranchBlockageDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Time both circuit pressures across breaths, with separate recovery hysteresis. */
bool techPhysCpapTooHighDetect(uint32_t nowMs) {
    stTechPhysRuntime *lRuntime = &gTechPhysCpapRuntime;
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
        lLimit = lPlan.peepCmh2o + TECH_PHYS_CPAP_RECOVERY_OFFSET_CMH2O;
        lCondition = (lInspPressure < lLimit) && (lPatientPressure < lLimit);
        lDurationMs = TECH_PHYS_CPAP_RECOVERY_MS;
    } else {
        lLimit = lPlan.peepCmh2o + TECH_PHYS_CPAP_HIGH_OFFSET_CMH2O;
        lCondition = (lInspPressure > lLimit) && (lPatientPressure > lLimit);
        lDurationMs = TECH_PHYS_CPAP_CONFIRM_MS;
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
bool techPhysPipelineLeakDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Patient circuit disconnection - H. */
bool techPhysPipelineDisconnectDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Pressure limitation - L. */
bool techPhysPressureLimitDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Volume limitation - L. */
bool techPhysVolumeLimitDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Inspiratory pressure not reached - L. */
bool techPhysInspPressNotReachedDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Tidal volume not reached - L. */
bool techPhysTidalVolNotReachedDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Sigh cycle pressure limitation - L. */
bool techPhysSighCyclePressLimitDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Inspiratory time too long - L. */
bool techPhysInspTimeTooLongDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Inhaled gas temperature too high - H. */
bool techPhysInhaledGasTempHighDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: AMV target not reached - L. */
bool techPhysAmvTargetNotReachedDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Oxygen therapy flow not reached - H. */
bool techPhysO2FlowNotReachedDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Patient flow sensor fault - H. */
bool techPhysPatFlowSensorFaultDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Patient pressure sensor fault - H. */
bool techPhysPatPressSensorFaultDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Machine circuit disconnection - H. */
bool techPhysMechPipelineDisconnectDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Expiratory branch blockage - M. */
bool techPhysExpBranchBlockageDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Maximum inspiratory negative pressure - H. */
bool techPhysMaxInspNegPressureDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Inspiratory pressure not released - H. */
bool techPhysInspPressureNotReleasedDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Oxygen source failure - H. */
bool techPhysO2SourceFailureDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: Proximal pressure sampling tube disconnection - H. */
bool techPhysProximalPressTubeDisconnectDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/*************************************** End of file ********************************/
