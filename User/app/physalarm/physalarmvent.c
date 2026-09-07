/************************************************************************************
* @file     : physalarmvent.c
* @brief    : Ventilation physiological alarm detectors.
* @details  : Detects pressure and exhaled tidal-volume alarm conditions.
* @author   :
* @date     : 2026-09-07
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "physalarmvent.h"

#include <string.h>

#include "controldata.h"
#include "monitorengine.h"
#include "phasecontroller.h"
#include "rtos.h"
#include "settingdata.h"

static stPhysAlarmVentRuntime gPhysAlarmVentRuntime[PHYS_ALARM_COUNT];

/** Accept each completed breath sequence only once for one detector. */
static bool physAlarmVentBreathResultRead(ePhysAlarmType type, stBreathResult *result)
{
    stPhysAlarmVentRuntime *lRuntime = &gPhysAlarmVentRuntime[type];

    if ((monitorEngineBreathResultGet(result) != MONITOR_ENGINE_SUCCESS) ||
        ((result->validMask & BREATH_RESULT_VALID_COMPLETE) == 0U)) {
        return false;
    }
    if (lRuntime->sequenceInitialized &&
        (lRuntime->processedSequence == result->sequence)) {
        return false;
    }
    lRuntime->processedSequence = result->sequence;
    lRuntime->sequenceInitialized = true;
    return true;
}

void physAlarmVentInit(void)
{
    (void)memset(gPhysAlarmVentRuntime, 0,
                 sizeof(gPhysAlarmVentRuntime));
}

bool physAlarmVentAirwayPressureHighDetect(uint32_t nowMs)
{
    stPhysAlarmVentRuntime *lRuntime =
        &gPhysAlarmVentRuntime[PHYS_ALARM_AIRWAY_PRESSURE_HIGH];
    const stVentLimitSettings *lLimits = GetVentLimitSettings();
    float lPressure;

    if (phaseControllerStateGet() == PHASE_EXP) {
        lRuntime->timing = false;
        lRuntime->active = false;
        return lRuntime->active;
    }
    lPressure = controlDataGet(PAT_REAL_PRS);
    if (!(lPressure > lLimits->pressureHigh)) {
        lRuntime->timing = false;
        return lRuntime->active;
    }
    if (!lRuntime->timing) {
        lRuntime->referenceMs = nowMs;
        lRuntime->timing = true;
        return lRuntime->active;
    }
    if ((nowMs - lRuntime->referenceMs) >=
        PHYS_ALARM_PRESSURE_HIGH_CONFIRM_MS) {
        lRuntime->active = true;
    }
    return lRuntime->active;
}

bool physAlarmVentAirwayPressureLowDetect(uint32_t nowMs)
{
    stPhysAlarmVentRuntime *lRuntime =
        &gPhysAlarmVentRuntime[PHYS_ALARM_AIRWAY_PRESSURE_LOW];
    const stVentLimitSettings *lLimits = GetVentLimitSettings();
    stBreathResult lResult;
    bool lPeakValid;
    bool lPlateauValid;
    bool lLow;
    bool lRecovered;

    (void)nowMs;
    if (!physAlarmVentBreathResultRead(PHYS_ALARM_AIRWAY_PRESSURE_LOW,
                                       &lResult)) {
        return lRuntime->active;
    }
    lPeakValid = (lResult.validMask & BREATH_RESULT_VALID_PPEAK) != 0U;
    lPlateauValid =
        (lResult.validMask & BREATH_RESULT_VALID_PLATEAU_PRESSURE) != 0U;
    lLow = (lPeakValid && (lResult.ppeakCmh2o < lLimits->pressureLow)) ||
           (lPlateauValid &&
            (lResult.plateauPressureCmh2o < lLimits->pressureLow));
    lRecovered =
        (lPeakValid && (lResult.ppeakCmh2o > lLimits->pressureLow)) ||
        (lPlateauValid &&
         (lResult.plateauPressureCmh2o > lLimits->pressureLow));

    if (lRecovered) {
        lRuntime->consecutiveBreaths = 0U;
        lRuntime->active = false;
    } else if (lLow) {
        if (lRuntime->consecutiveBreaths <
            PHYS_ALARM_PRESSURE_LOW_CONFIRM_BREATHS) {
            lRuntime->consecutiveBreaths++;
        }
        if (lRuntime->consecutiveBreaths >=
            PHYS_ALARM_PRESSURE_LOW_CONFIRM_BREATHS) {
            lRuntime->active = true;
        }
    } else {
        lRuntime->consecutiveBreaths = 0U;
    }
    return lRuntime->active;
}

bool physAlarmVentExhaledVolumeHighDetect(uint32_t nowMs)
{
    stPhysAlarmVentRuntime *lRuntime =
        &gPhysAlarmVentRuntime[PHYS_ALARM_EXHALED_VOLUME_HIGH];
    const float lLimit = (float)GetVentLimitSettings()->tidalVolumeHigh;
    stBreathResult lResult;

    (void)nowMs;
    if (!physAlarmVentBreathResultRead(PHYS_ALARM_EXHALED_VOLUME_HIGH,
                                       &lResult)) {
        return lRuntime->active;
    }
    if ((lResult.validMask & BREATH_RESULT_VALID_VTE) == 0U) {
        lRuntime->consecutiveBreaths = 0U;
        return lRuntime->active;
    }
    if (lResult.vteMl < lLimit) {
        lRuntime->consecutiveBreaths = 0U;
        lRuntime->active = false;
    } else if (lResult.vteMl >
               (PHYS_ALARM_VTE_HIGH_IMMEDIATE_RATIO * lLimit)) {
        lRuntime->consecutiveBreaths = PHYS_ALARM_VTE_CONFIRM_BREATHS;
        lRuntime->active = true;
    } else if (lResult.vteMl > lLimit) {
        if (lRuntime->consecutiveBreaths < PHYS_ALARM_VTE_CONFIRM_BREATHS) {
            lRuntime->consecutiveBreaths++;
        }
        if (lRuntime->consecutiveBreaths >= PHYS_ALARM_VTE_CONFIRM_BREATHS) {
            lRuntime->active = true;
        }
    } else {
        lRuntime->consecutiveBreaths = 0U;
    }
    return lRuntime->active;
}

bool physAlarmVentExhaledVolumeLowDetect(uint32_t nowMs)
{
    stPhysAlarmVentRuntime *lRuntime =
        &gPhysAlarmVentRuntime[PHYS_ALARM_EXHALED_VOLUME_LOW];
    const float lLimit = (float)GetVentLimitSettings()->tidalVolumeLow;
    stBreathResult lResult;

    (void)nowMs;
    if (!physAlarmVentBreathResultRead(PHYS_ALARM_EXHALED_VOLUME_LOW,
                                       &lResult)) {
        return lRuntime->active;
    }
    if ((lResult.validMask & BREATH_RESULT_VALID_VTE) == 0U) {
        lRuntime->consecutiveBreaths = 0U;
        return lRuntime->active;
    }
    if (lResult.vteMl > lLimit) {
        lRuntime->consecutiveBreaths = 0U;
        lRuntime->active = false;
    } else if (lResult.vteMl < lLimit) {
        if (lRuntime->consecutiveBreaths < PHYS_ALARM_VTE_CONFIRM_BREATHS) {
            lRuntime->consecutiveBreaths++;
        }
        if (lRuntime->consecutiveBreaths >= PHYS_ALARM_VTE_CONFIRM_BREATHS) {
            lRuntime->active = true;
        }
    } else {
        lRuntime->consecutiveBreaths = 0U;
    }
    return lRuntime->active;
}

/** Check one completed PEEP per inspiration and time strict recovery each call. */
static bool physAlarmVentPeepDetect(ePhysAlarmType type, uint32_t nowMs) {
    stPhysAlarmVentRuntime *lRuntime = &gPhysAlarmVentRuntime[type];
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

bool physAlarmVentPeepHighDetect(uint32_t nowMs) {
    return physAlarmVentPeepDetect(PHYS_ALARM_PEEP_HIGH, nowMs);
}

bool physAlarmVentPeepLowDetect(uint32_t nowMs) {
    return physAlarmVentPeepDetect(PHYS_ALARM_PEEP_LOW, nowMs);
}

/*************************************** End of file ********************************/
