/************************************************************************************
* @file     : monitorengine.c
* @brief    : Breath monitor and completed-result publisher.
* @details  : Integrates VTi/VTe and publishes one result at each breath boundary.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "monitorengine.h"

#include <float.h>
#include <stddef.h>
#include <string.h>

#include "controldata.h"
#include "databus.h"
#include "phasecontroller.h"
#include "rtos.h"

static stMonitorEngine gMonitorEngine;
static float gMonitorData[MONITOR_DATA_COUNT];
static stBreathResult gMonitorLatestBreathResult;
static uint8_t gMonitorBreathResultAvailable;

/** Return true for a finite single-precision measurement. */
static uint8_t monitorEngineFinite(float value)
{
    return (uint8_t)((value >= -FLT_MAX) && (value <= FLT_MAX));
}

/** Calculate a positive square root without pulling in the C math library. */
static float monitorEngineSqrt(float value)
{
    uint32_t lBits;
    float lRoot;

    (void)memcpy(&lBits, &value, sizeof(lBits));
    lBits = (lBits >> 1U) + 0x1FC00000UL;
    (void)memcpy(&lRoot, &lBits, sizeof(lRoot));
    lRoot = 0.5F * (lRoot + (value / lRoot));
    lRoot = 0.5F * (lRoot + (value / lRoot));
    return lRoot;
}

/** Store a monitor result selected by type. */
static int8_t monitorEngineSet(eMonitorDataType type, float value)
{
    if ((type <= MONITOR_DATA_NONE) || (type >= MONITOR_DATA_COUNT)) {
        return MONITOR_ENGINE_ERROR_PARAM;
    }
    gMonitorData[type] = value;
    return MONITOR_ENGINE_SUCCESS;
}

/** Integrate patient flow in litres per minute into millilitres. */
static void monitorEngineTidalVolumeIntegrate(eMonitorDataType type, float flow)
{
    gMonitorData[type] += flow * MONITOR_FLOW_SAMPLE_VOLUME_ML;
}

/** Accumulate paired flow and pressure-root samples for leak estimation. */
static void monitorEngineLeakAccumulate(float flow)
{
    float lPressure = controlDataGet(PAT_REAL_PRS);

    if ((monitorEngineFinite(flow) == 0U) ||
        (monitorEngineFinite(lPressure) == 0U) ||
        (lPressure <= 0.0F)) {
        gMonitorEngine.leakCycleInvalid = 1U;
        return;
    }
    gMonitorEngine.leakFlowSumLpm += flow;
    gMonitorEngine.leakPressureRootSum += monitorEngineSqrt(lPressure);
}

/** Publish the completed-cycle coefficient for Qleak = K * sqrt(Paw). */
static void monitorEngineLeakCoefficientCalculate(void)
{
    float lCoefficient;

    (void)monitorEngineSet(MONITOR_LEAK_VALID, 0.0F);
    (void)monitorEngineSet(MONITOR_LEAK_COEFFICIENT, 0.0F);
    (void)monitorEngineSet(MONITOR_LEAK_BALANCE_COEFFICIENT, 0.0F);
    if ((gMonitorEngine.leakCycleInvalid != 0U) ||
        (monitorEngineFinite(gMonitorEngine.leakFlowSumLpm) == 0U) ||
        (monitorEngineFinite(gMonitorEngine.leakPressureRootSum) == 0U) ||
        (gMonitorEngine.leakPressureRootSum < MONITOR_LEAK_PRESSURE_SUM_MIN)) {
        return;
    }
    /* The common fixed 6 ms sample interval cancels in this ratio. */
    lCoefficient = gMonitorEngine.leakFlowSumLpm /
                   gMonitorEngine.leakPressureRootSum;
    if (monitorEngineFinite(lCoefficient) == 0U) {
        return;
    }
    (void)monitorEngineSet(MONITOR_LEAK_BALANCE_COEFFICIENT, lCoefficient);
    if (lCoefficient < MONITOR_LEAK_COEFFICIENT_MIN) {
        lCoefficient = MONITOR_LEAK_COEFFICIENT_MIN;
    } else if (lCoefficient > MONITOR_LEAK_COEFFICIENT_MAX) {
        lCoefficient = MONITOR_LEAK_COEFFICIENT_MAX;
    }
    (void)monitorEngineSet(MONITOR_LEAK_COEFFICIENT, lCoefficient);
    (void)monitorEngineSet(MONITOR_LEAK_VALID, 1.0F);
}

/** Publish the shared nonnegative downstream leak compensation. */
static void monitorEngineLeakFlowProcess(void)
{
    float lCoefficient = monitorEngineGet(MONITOR_LEAK_COEFFICIENT);
    float lPressure = controlDataGet(PAT_REAL_PRS);
    float lLeakFlow = 0.0F;

    if ((monitorEngineGet(MONITOR_LEAK_VALID) != 0.0F) &&
        (monitorEngineFinite(lCoefficient) != 0U) && (lCoefficient > 0.0F) &&
        (monitorEngineFinite(lPressure) != 0U) &&
        (lPressure > 0.0F)) {
        lLeakFlow = lCoefficient * monitorEngineSqrt(lPressure);
        if (lLeakFlow > MONITOR_PATIENT_LEAK_FLOW_MAX_LPM) {
            lLeakFlow = MONITOR_PATIENT_LEAK_FLOW_MAX_LPM;
        }
    }
    (void)monitorEngineSet(MONITOR_LEAK_FLOW, lLeakFlow);
    if ((gMonitorEngine.breathActive != 0U) && (gMonitorEngine.breathCompleted == 0U)) {
        /* Fixed 6 ms samples: integral divided by duration is mean L/min. */
        gMonitorEngine.minuteLeakSumLpm += monitorEngineGet(MONITOR_LEAK_FLOW);
        gMonitorEngine.minuteLeakSampleCount++;
        if ((monitorEngineGet(MONITOR_LEAK_VALID) == 0.0F) ||
            (monitorEngineFinite(lPressure) == 0U) || (lPressure <= 0.0F)) {
            gMonitorEngine.minuteLeakInvalid = 1U;
        }
    }
}

/** Average PAC end-inspiration pressure; other modes require near-zero flow. */
static void monitorEnginePlateauPressureProcess(uint32_t nowMs)
{
    ePhaseControllerState lPhaseState;
    float lFlow;
    float lPressure;
    uint8_t lSampleActive;

    if ((gMonitorEngine.breathActive == 0U) ||
        (gMonitorEngine.breathCompleted != 0U) ||
        (gMonitorEngine.runState != MONITOR_STATE_INSP)) {
        return;
    }

    lPhaseState = phaseControllerStateGet();
    /* PAC must exclude pressure decay after the controller ends inspiration. */
    if ((gMonitorEngine.breathPlan.mode == VENT_MD_PAC) &&
        (lPhaseState != PHASE_INSP)) {
        return;
    }
    lSampleActive = phaseControllerVolumePauseActiveGet();
    if ((lSampleActive == 0U) &&
        (((nowMs - gMonitorEngine.breathStartedMs) +
          MONITOR_PLATEAU_END_WINDOW_MS) >=
         gMonitorEngine.breathPlan.maximumInspiratoryTimeMs)) {
        lSampleActive = 1U;
    }
    /* Include the zero-flow boundary while expiration waits for reverse flow. */
    if ((lSampleActive == 0U) && (lPhaseState == PHASE_EXP)) {
        lSampleActive = 1U;
    }
    if (lSampleActive == 0U) {
        return;
    }

    lFlow = controlDataGet(PAT_REAL_FLOW) -
            monitorEngineGet(MONITOR_LEAK_FLOW);
    lPressure = controlDataGet(PAT_REAL_PRS);
    if ((monitorEngineFinite(lFlow) == 0U) ||
        (monitorEngineFinite(lPressure) == 0U) ||
        ((gMonitorEngine.breathPlan.mode != VENT_MD_PAC) &&
         ((lFlow <= -MONITOR_FLOW_DEADBAND_LPM) ||
          (lFlow >= MONITOR_FLOW_DEADBAND_LPM)))) {
        return;
    }

    gMonitorEngine.plateauPressureSumCmh2o += lPressure;
    gMonitorEngine.plateauPressureSampleCount++;
    (void)monitorEngineSet(
        MONITOR_PLATEAU_PRS,
        gMonitorEngine.plateauPressureSumCmh2o /
        (float)gMonitorEngine.plateauPressureSampleCount);
}

/** Clear the expiration window after publishing its completed value. */
static void monitorEngineDynamicPeepReset(void) {
    (void)memset(gMonitorEngine.peepSamplesCmh2o, 0, sizeof(gMonitorEngine.peepSamplesCmh2o));
    gMonitorEngine.peepSampleCount = 0U;
    gMonitorEngine.peepSampleIndex = 0U;
    gMonitorEngine.peepReadySampleCount = 0U;
}

/** Retain the latest five finite expiration pressures without flow gating. */
static void monitorEngineDynamicPeepAccumulate(void) {
    float lPressure = controlDataGet(PAT_REAL_PRS);

    if (monitorEngineFinite(lPressure) == 0U) {
        return;
    }
    gMonitorEngine.peepSamplesCmh2o[gMonitorEngine.peepSampleIndex] = lPressure;
    gMonitorEngine.peepSampleIndex = (uint8_t)((gMonitorEngine.peepSampleIndex + 1U) %
                                              MONITOR_DYN_PEEP_WINDOW_SIZE);
    if (gMonitorEngine.peepSampleCount < MONITOR_DYN_PEEP_WINDOW_SIZE) {
        gMonitorEngine.peepSampleCount++;
    }
}

/** Publish the window mean; short expirations average only available points. */
static void monitorEngineDynamicPeepCalculate(void) {
    float lPressure = 0.0F;
    uint8_t lIndex;

    for (lIndex = 0U; lIndex < gMonitorEngine.peepSampleCount; lIndex++) {
        lPressure += gMonitorEngine.peepSamplesCmh2o[lIndex] /
                     (float)gMonitorEngine.peepSampleCount;
    }
    (void)monitorEngineSet(MONITOR_DYN_PEEP, lPressure);
    (void)monitorEngineSet(MONITOR_DYN_PEEP_VALID, (float)(gMonitorEngine.peepSampleCount != 0U));
}

/** Keep PAC latched; require about 100 ms of quiet pressure for PSV/ST display. */
static void monitorEnginePeepDisplayProcess(eVentMode mode) {
    float lPressure = controlDataGet(PAT_REAL_PRS);

    if (mode == VENT_MD_PAC) {
        return;
    }
    if ((mode == VENT_MD_CPAP_PSV) || (mode == VENT_MD_PSV_ST)) {
        if ((phaseControllerExpirationReadyGet() == 0U) ||
            (monitorEngineFinite(lPressure) == 0U)) {
            gMonitorEngine.peepReadySampleCount = 0U;
            return;
        }
        if ((gMonitorEngine.peepReadySampleCount == 0U) ||
            (lPressure < gMonitorEngine.peepStableMinimumCmh2o)) {
            gMonitorEngine.peepStableMinimumCmh2o = lPressure;
        }
        if ((gMonitorEngine.peepReadySampleCount == 0U) ||
            (lPressure > gMonitorEngine.peepStableMaximumCmh2o)) {
            gMonitorEngine.peepStableMaximumCmh2o = lPressure;
        }
        /* Capture can time out; short plateaus during a trigger are not stable. */
        if ((gMonitorEngine.peepStableMaximumCmh2o -
             gMonitorEngine.peepStableMinimumCmh2o) >
            MONITOR_HMI_PEEP_STABLE_RANGE_CMH2O) {
            gMonitorEngine.peepStableMinimumCmh2o = lPressure;
            gMonitorEngine.peepStableMaximumCmh2o = lPressure;
            gMonitorEngine.peepReadySampleCount = 0U;
        }
        if (gMonitorEngine.peepReadySampleCount < MONITOR_HMI_PEEP_STABLE_SAMPLE_COUNT) {
            gMonitorEngine.peepReadySampleCount++;
        }
        if (gMonitorEngine.peepReadySampleCount < MONITOR_HMI_PEEP_STABLE_SAMPLE_COUNT) {
            return;
        }
    }
    repRtosEnterCritical();
    gMonitorData[MONITOR_HMI_PEEP] = monitorEngineGet(MONITOR_DYN_PEEP);
    gMonitorData[MONITOR_HMI_PEEP_VALID] = monitorEngineGet(MONITOR_DYN_PEEP_VALID);
    repRtosExitCritical();
}

/** Convert the shared phase to the local monitoring state. */
static eMonitorEngineState monitorEngineStateFromPhase(ePhaseControllerState phaseState)
{
    switch (phaseState) {
        case PHASE_INSP:
            return MONITOR_STATE_INSP;
        case PHASE_EXP:
            return MONITOR_STATE_EXP;
        case PHASE_IDLE:
        default:
            return MONITOR_STATE_IDLE;
    }
}

/** Accumulate all PAT samples at the fixed 6 ms monitoring interval. */
static void monitorEngineMeanPressureAccumulate(void) {
    float lPressure = controlDataGet(PAT_REAL_PRS);

    if (monitorEngineFinite(lPressure) == 0U) {
        gMonitorEngine.meanPressureInvalid = 1U;
        return;
    }
    gMonitorEngine.meanPressureSumCmh2o += lPressure;
    gMonitorEngine.meanPressureSampleCount++;
}

/** Capture the completed breath measurements and timing. */
static void monitorEngineBreathResultCapture(stBreathResult *result, uint32_t nowMs) {
    result->sequence = gMonitorEngine.breathPlan.sequence;
    result->mode = gMonitorEngine.breathPlan.mode;
    result->breathType = gMonitorEngine.breathPlan.breathType;
    result->triggerReason = gMonitorEngine.breathPlan.triggerReason;
    result->vtiMl = gMonitorData[MONITOR_TIDA_VOL_INSP];
    result->vteMl = gMonitorData[MONITOR_TIDA_VOL_EXP];
    result->ppeakCmh2o = gMonitorEngine.peakPressureCmh2o;
    result->plateauPressureCmh2o = gMonitorData[MONITOR_PLATEAU_PRS];
    result->peepCmh2o = monitorEngineGet(MONITOR_DYN_PEEP);
    result->peakInspiratoryFlowLpm = gMonitorEngine.peakInspiratoryFlowLpm;
    result->peakExpiratoryFlowLpm = gMonitorEngine.peakExpiratoryFlowLpm;
    result->cycleReason = gMonitorEngine.cycleReason;
    result->inspiratoryTimeMs = gMonitorEngine.inspiratoryTimeMs;
    result->cycleTimeMs = nowMs - gMonitorEngine.breathStartedMs;
    result->validMask = BREATH_RESULT_VALID_COMPLETE |
                        BREATH_RESULT_VALID_CYCLE_TIME |
                        BREATH_RESULT_VALID_INSPIRATORY_TIME;
}

/** Calculate whole-breath mean pressure and leak flow. */
static void monitorEngineBreathResultAveragesCalculate(stBreathResult *result) {
    if ((gMonitorEngine.meanPressureInvalid == 0U) &&
        (gMonitorEngine.meanPressureSampleCount > 0U) &&
        (monitorEngineFinite(gMonitorEngine.meanPressureSumCmh2o) != 0U)) {
        result->meanPressureCmh2o = gMonitorEngine.meanPressureSumCmh2o /
                                  (float)gMonitorEngine.meanPressureSampleCount;
        result->validMask |= BREATH_RESULT_VALID_MEAN_PRESSURE;
    }
    if ((gMonitorEngine.minuteLeakInvalid == 0U) &&
        (gMonitorEngine.leakCycleInvalid == 0U) &&
        (gMonitorEngine.minuteLeakSampleCount > 0U) &&
        (monitorEngineFinite(gMonitorEngine.minuteLeakSumLpm) != 0U)) {
        result->minuteLeakLpm = gMonitorEngine.minuteLeakSumLpm /
                               (float)gMonitorEngine.minuteLeakSampleCount;
        result->validMask |= BREATH_RESULT_VALID_MINUTE_LEAK;
    }
}

/** Mark valid measurements and discard incomplete expiratory peaks. */
static void monitorEngineBreathResultValidate(stBreathResult *result) {
    if ((monitorEngineFinite(result->vtiMl) != 0U) &&
        (gMonitorEngine.volumeInvalid == 0U)) {
        result->validMask |= BREATH_RESULT_VALID_VTI;
    }
    if (monitorEngineFinite(result->vteMl) != 0U) {
        result->validMask |= BREATH_RESULT_VALID_VTE;
    }
    if (monitorEngineFinite(result->ppeakCmh2o) != 0U) {
        result->validMask |= BREATH_RESULT_VALID_PPEAK;
    }
    if ((gMonitorEngine.plateauPressureSampleCount > 0U) &&
        (monitorEngineFinite(result->plateauPressureCmh2o) != 0U)) {
        result->validMask |= BREATH_RESULT_VALID_PLATEAU_PRESSURE;
    }
    if ((gMonitorEngine.peepSampleCount != 0U) &&
        (monitorEngineFinite(result->peepCmh2o) != 0U)) {
        result->validMask |= BREATH_RESULT_VALID_PEEP;
    }
    if (monitorEngineFinite(result->peakInspiratoryFlowLpm) != 0U) {
        result->validMask |= BREATH_RESULT_VALID_PEAK_INSP_FLOW;
    }
    /* Reject incomplete flow measurements even if a partial peak is finite. */
    if ((gMonitorEngine.volumeInvalid == 0U) &&
        (monitorEngineFinite(result->peakExpiratoryFlowLpm) != 0U)) {
        result->validMask |= BREATH_RESULT_VALID_PEAK_EXP_FLOW;
    } else {
        result->peakExpiratoryFlowLpm = 0.0F;
    }
    if ((gMonitorEngine.volumeLimited != 0U) || (gMonitorEngine.volumeBlowerLimited != 0U)) {
        result->validMask |= BREATH_RESULT_VOLUME_LIMITED;
    }
}

/** Calculate minute volumes and the completed-breath leak percentage. */
static void monitorEngineBreathResultMinuteVolumeCalculate(stBreathResult *result) {
    /* Completed tidal volume in mL times 60 / cycle ms gives L/min. */
    if (((result->validMask & BREATH_RESULT_VALID_VTI) != 0U) &&
        (result->vtiMl >= 0.0F) && (result->cycleTimeMs > 0U)) {
        result->minuteInspiratoryLpm = result->vtiMl * (60.0F / (float)result->cycleTimeMs);
        if (monitorEngineFinite(result->minuteInspiratoryLpm) != 0U) {
            result->validMask |= BREATH_RESULT_VALID_MVI;
        } else {
            result->minuteInspiratoryLpm = 0.0F;
        }
    }
    if (((result->validMask & BREATH_RESULT_VALID_VTE) != 0U) &&
        (gMonitorEngine.volumeInvalid == 0U) &&
        (result->vteMl >= 0.0F) && (result->cycleTimeMs > 0U)) {
        float lTotalFlow;
        result->minuteTotalLpm = result->vteMl * (60.0F / (float)result->cycleTimeMs);
        lTotalFlow = result->minuteTotalLpm + result->minuteLeakLpm;
        if (((result->validMask & BREATH_RESULT_VALID_MINUTE_LEAK) != 0U) &&
            (monitorEngineFinite(lTotalFlow) != 0U) && (lTotalFlow > 0.0F)) {
            result->leakPercent = (result->minuteLeakLpm / lTotalFlow) * 100.0F;
            result->validMask |= BREATH_RESULT_VALID_LEAK_PERCENT;
        }
        if (monitorEngineFinite(result->minuteTotalLpm) == 0U) {
            result->minuteTotalLpm = 0.0F;
        } else {
            result->validMask |= BREATH_RESULT_VALID_MVE;
        }
    }
}

/** Calculate inspiratory and expiratory resistance. */
static void monitorEngineBreathResultResistanceCalculate(stBreathResult *result) {
    /* Convert L/min to L/s for both resistance values. */
    if ((gMonitorEngine.volumeInvalid == 0U) &&
        ((result->validMask & BREATH_RESULT_VALID_PEEP) != 0U)) {
        float lResistance;
        if (((result->validMask & (BREATH_RESULT_VALID_PPEAK |
                                  BREATH_RESULT_VALID_PLATEAU_PRESSURE)) ==
             (BREATH_RESULT_VALID_PPEAK | BREATH_RESULT_VALID_PLATEAU_PRESSURE)) &&
            (result->peakInspiratoryFlowLpm > 0.0F)) {
            lResistance = 60.0F * (result->ppeakCmh2o - result->peepCmh2o) /
                          result->peakInspiratoryFlowLpm;
            if ((monitorEngineFinite(lResistance) != 0U) && (lResistance >= 0.0F)) {
                result->resistanceInspiratory = lResistance;
                result->validMask |= BREATH_RESULT_VALID_RES_INSP;
            }
        }
        if (((result->validMask & BREATH_RESULT_VALID_PLATEAU_PRESSURE) != 0U) &&
            (gMonitorEngine.peakExpiratoryFlowLpm > 0.0F)) {
            lResistance = 60.0F * (result->plateauPressureCmh2o - result->peepCmh2o) /
                          gMonitorEngine.peakExpiratoryFlowLpm;
            if ((monitorEngineFinite(lResistance) != 0U) && (lResistance >= 0.0F)) {
                result->resistanceExpiratory = lResistance;
                result->validMask |= BREATH_RESULT_VALID_RES_EXP;
            }
        }
    }
}

/** Calculate dynamic and static compliance. */
static void monitorEngineBreathResultComplianceCalculate(stBreathResult *result) {
    /* Compliance uses completed volumes in mL and positive pressure differences. */
    if ((gMonitorEngine.volumeInvalid == 0U) &&
        ((result->validMask & (BREATH_RESULT_VALID_VTI | BREATH_RESULT_VALID_PPEAK |
                              BREATH_RESULT_VALID_PEEP)) ==
         (BREATH_RESULT_VALID_VTI | BREATH_RESULT_VALID_PPEAK | BREATH_RESULT_VALID_PEEP))) {
        float lPressureDifference = result->ppeakCmh2o - result->peepCmh2o;
        if ((monitorEngineFinite(lPressureDifference) != 0U) &&
            (lPressureDifference > 0.0F) && (result->vtiMl >= 0.0F)) {
            float lCompliance = result->vtiMl / lPressureDifference;
            if (monitorEngineFinite(lCompliance) != 0U) {
                result->complianceDynamic = lCompliance;
                result->validMask |= BREATH_RESULT_VALID_C_DYNC;
            }
        }
    }
    if ((gMonitorEngine.volumeInvalid == 0U) &&
        ((result->validMask & (BREATH_RESULT_VALID_VTE | BREATH_RESULT_VALID_PLATEAU_PRESSURE |
                              BREATH_RESULT_VALID_PEEP)) ==
         (BREATH_RESULT_VALID_VTE | BREATH_RESULT_VALID_PLATEAU_PRESSURE | BREATH_RESULT_VALID_PEEP))) {
        float lPressureDifference = result->plateauPressureCmh2o - result->peepCmh2o;
        if ((monitorEngineFinite(lPressureDifference) != 0U) &&
            (lPressureDifference > 0.0F) && (result->vteMl >= 0.0F)) {
            float lCompliance = result->vteMl / lPressureDifference;
            if (monitorEngineFinite(lCompliance) != 0U) {
                result->complianceStatic = lCompliance;
                result->validMask |= BREATH_RESULT_VALID_C_STAT;
            }
        }
    }
}

/** Atomically store the completed snapshot and HMI values. */
static void monitorEngineBreathResultStore(const stBreathResult *result) {
    repRtosEnterCritical();
    gMonitorData[MONITOR_HMI_C_DYNC] = result->complianceDynamic;
    gMonitorData[MONITOR_HMI_C_STAT] = result->complianceStatic;
    gMonitorData[MONITOR_HMI_RES_INSP] = result->resistanceInspiratory;
    gMonitorData[MONITOR_HMI_RES_EXP] = result->resistanceExpiratory;
    gMonitorData[MONITOR_HMI_TIDA_VOL_INSP] = result->vtiMl;
    gMonitorData[MONITOR_HMI_PRS_MEAN] = result->meanPressureCmh2o;
    gMonitorData[MONITOR_HMI_MV_LEAK] = result->minuteLeakLpm;
    gMonitorData[MONITOR_HMI_MV_TOTAL] = result->minuteTotalLpm;
    gMonitorData[MONITOR_HMI_MV_INSP] = result->minuteInspiratoryLpm;
    gMonitorData[MONITOR_HMI_LEAK_PERCENT] = result->leakPercent;
    gMonitorData[MONITOR_HMI_TIDA_VOL_EXP] = result->vteMl;
    gMonitorData[MONITOR_HMI_PPEAK] = result->ppeakCmh2o;
    gMonitorData[MONITOR_HMI_PLATEAU_PRS] = result->plateauPressureCmh2o;
    if ((result->mode != VENT_MD_CPAP_PSV) && (result->mode != VENT_MD_PSV_ST)) {
        gMonitorData[MONITOR_HMI_PEEP] = result->peepCmh2o;
        gMonitorData[MONITOR_HMI_PEEP_VALID] =
            (float)((result->validMask & BREATH_RESULT_VALID_PEEP) != 0U);
    }
    gMonitorData[MONITOR_HMI_PEAK_INSP_FLOW] = result->peakInspiratoryFlowLpm;
    gMonitorData[MONITOR_HMI_PEAK_EXP_FLOW] = result->peakExpiratoryFlowLpm;
    gMonitorData[MONITOR_HMI_INSP_TIME_MS] = (float)result->inspiratoryTimeMs;
    gMonitorData[MONITOR_HMI_CYCLE_TIME_MS] = (float)result->cycleTimeMs;
    gMonitorLatestBreathResult = *result;
    gMonitorBreathResultAvailable = 1U;
    repRtosExitCritical();
}

/** Pass completed inspiratory volume to scheduler compensation. */
static void monitorEngineBreathResultVolumeFeedback(const stBreathResult *result) {
    breathSchedulerVolumeFeedback(&gMonitorEngine.breathPlan, result->vtiMl,
        (uint8_t)(((result->validMask & BREATH_RESULT_VALID_VTI) != 0U) &&
                  (gMonitorEngine.volumeLimited == 0U) &&
                  ((BREATH_VOLUME_FLOW_COMPENSATION_ENABLE == 0) ||
                   (gMonitorEngine.volumeBlowerLimited == 0U)) &&
                  (gMonitorEngine.runState == MONITOR_STATE_EXP) &&
                  (result->cycleReason == BREATH_CYCLE_REASON_TIME)));
}

/** Calculate and publish the breath that ended before a new inspiration. */
static void monitorEngineBreathResultPublish(uint32_t nowMs) {
    stBreathResult lResult = {0};

    monitorEngineBreathResultCapture(&lResult, nowMs);
    monitorEngineBreathResultAveragesCalculate(&lResult);
    monitorEngineBreathResultValidate(&lResult);
    monitorEngineBreathResultMinuteVolumeCalculate(&lResult);
    monitorEngineBreathResultResistanceCalculate(&lResult);
    monitorEngineBreathResultComplianceCalculate(&lResult);
    monitorEngineBreathResultStore(&lResult);
    monitorEngineBreathResultVolumeFeedback(&lResult);
}

/** Complete a cycle once, shared by explicit and observed breath boundaries. */
static void monitorEngineBreathFinish(uint32_t nowMs) {
    if ((gMonitorEngine.breathActive != 0U) &&
        (gMonitorEngine.expirationSeen != 0U) &&
        (gMonitorEngine.breathCompleted == 0U)) {
        monitorEngineLeakCoefficientCalculate();
        monitorEngineDynamicPeepCalculate();
        monitorEngineBreathResultPublish(nowMs);
        monitorEngineDynamicPeepReset();
        gMonitorEngine.breathCompleted = 1U;
    }
}

void monitorEngineBreathComplete(uint32_t nowMs) {
    if (gMonitorEngine.flowZeroOffsetLpm != controlDataMdiffFlowZeroOffsetGet()) {
        monitorEngineInit();
        return;
    }
    if (phaseControllerStateGet() == PHASE_EXP) {
        monitorEngineBreathFinish(nowMs);
    }
}

void monitorEngineVolumeLimitedNotify(void) {
    if ((gMonitorEngine.breathActive != 0U) &&
        (gMonitorEngine.breathCompleted == 0U)) {
        gMonitorEngine.volumeLimited = 1U;
    }
}

/** Keep saturation visible while distinguishing it from a hard pressure limit. */
void monitorEngineBlowerLimitedNotify(void) {
    if ((gMonitorEngine.breathActive != 0U) &&
        (gMonitorEngine.breathCompleted == 0U)) {
        gMonitorEngine.volumeBlowerLimited = 1U;
    }
}

/** Start accumulation for the plan that just entered inspiration. */
static int8_t monitorEngineBreathStart(uint32_t nowMs)
{
    stBreathPlan lPlan;
    float lPressure;

    if (phaseControllerActivePlanGet(&lPlan) != PHASE_CONTROL_SUCCESS) {
        return MONITOR_ENGINE_ERROR_STATE;
    }
    lPressure = controlDataGet(PAT_REAL_PRS);
    gMonitorEngine.breathPlan = lPlan;
    gMonitorEngine.breathStartedMs = nowMs;
    gMonitorEngine.peakPressureCmh2o = lPressure;
    gMonitorEngine.peakInspiratoryFlowLpm = 0.0F;
    gMonitorEngine.peakExpiratoryFlowLpm = 0.0F;
    gMonitorEngine.plateauPressureSumCmh2o = 0.0F;
    gMonitorEngine.plateauPressureSampleCount = 0U;
    gMonitorEngine.meanPressureSumCmh2o = 0.0F;
    gMonitorEngine.meanPressureSampleCount = 0U;
    gMonitorEngine.meanPressureInvalid = 0U;
    monitorEngineDynamicPeepReset();
    gMonitorEngine.minuteLeakSumLpm = 0.0F;
    gMonitorEngine.minuteLeakSampleCount = 0U;
    gMonitorEngine.minuteLeakInvalid = 0U;
    gMonitorEngine.leakFlowSumLpm = 0.0F;
    gMonitorEngine.leakPressureRootSum = 0.0F;
    gMonitorEngine.leakCycleInvalid = 0U;
    gMonitorEngine.expirationSeen = 0U;
    gMonitorEngine.breathCompleted = 0U;
    gMonitorEngine.volumeInvalid = 0U;
    gMonitorEngine.volumeLimited = 0U;
    gMonitorEngine.volumeBlowerLimited = 0U;
    gMonitorEngine.inspiratoryTimeMs = 0U;
    gMonitorEngine.cycleReason = BREATH_CYCLE_REASON_NONE;
    gMonitorEngine.breathActive = 1U;
    (void)monitorEngineSet(MONITOR_TIDA_VOL, 0.0F);
    (void)monitorEngineSet(MONITOR_TIDA_VOL_INSP, 0.0F);
    (void)monitorEngineSet(MONITOR_TIDA_VOL_EXP, 0.0F);
    (void)monitorEngineSet(MONITOR_PLATEAU_PRS, 0.0F);
    return MONITOR_ENGINE_SUCCESS;
}

void monitorEngineInit(void)
{
    breathSchedulerVolumeReset();
    (void)memset(&gMonitorEngine, 0, sizeof(gMonitorEngine));
    (void)memset(gMonitorData, 0, sizeof(gMonitorData));
    (void)memset(&gMonitorLatestBreathResult, 0,
                 sizeof(gMonitorLatestBreathResult));
    gMonitorEngine.runState = MONITOR_STATE_IDLE;
    gMonitorBreathResultAvailable = 0U;
    gMonitorEngine.flowZeroOffsetLpm = controlDataMdiffFlowZeroOffsetGet();
}

float monitorEngineGet(eMonitorDataType type)
{
    if ((type <= MONITOR_DATA_NONE) || (type >= MONITOR_DATA_COUNT)) {
        return 0.0F;
    }
    return gMonitorData[type];
}

int8_t monitorEngineBreathResultGet(stBreathResult *result)
{
    if (result == NULL) {
        return MONITOR_ENGINE_ERROR_PARAM;
    }
    repRtosEnterCritical();
    if (gMonitorBreathResultAvailable == 0U) {
        repRtosExitCritical();
        return MONITOR_ENGINE_ERROR_STATE;
    }
    *result = gMonitorLatestBreathResult;
    repRtosExitCritical();
    return MONITOR_ENGINE_SUCCESS;
}

/** Calculate tidal volumes and maintain the completed-breath result. */
static void monitorEngineTidalVolumeProcess(uint32_t nowMs)
{
    eMonitorEngineState lNextState =
        monitorEngineStateFromPhase(phaseControllerStateGet());
    float lFlow = controlDataGet(PAT_REAL_FLOW);
    float lPressure;

    if ((gMonitorEngine.breathActive == 0U) || (gMonitorEngine.breathCompleted != 0U)) {
        return;
    }

    /* Confirm expiration with negative patient flow before leaving inspiration. */
    if ((gMonitorEngine.runState == MONITOR_STATE_INSP) &&
        (lNextState == MONITOR_STATE_EXP) &&
        !(lFlow < 0.0F)) {
        lNextState = MONITOR_STATE_INSP;
    }

    if ((lNextState == MONITOR_STATE_EXP) &&
               (gMonitorEngine.runState != MONITOR_STATE_EXP)) {
        if (gMonitorEngine.breathActive != 0U) {
            gMonitorEngine.inspiratoryTimeMs = nowMs - gMonitorEngine.breathStartedMs;
            gMonitorEngine.cycleReason = phaseControllerCycleReasonGet();
        }
        (void)monitorEngineSet(MONITOR_TIDA_VOL_EXP, 0.0F);
    }
    gMonitorEngine.runState = lNextState;

    if (gMonitorEngine.runState == MONITOR_STATE_IDLE) {
        return;
    }

    if (gMonitorEngine.runState == MONITOR_STATE_INSP) {
        lPressure = controlDataGet(PAT_REAL_PRS);
        if ((monitorEngineFinite(lPressure) != 0U) &&
            ((monitorEngineFinite(gMonitorEngine.peakPressureCmh2o) == 0U) ||
             (lPressure > gMonitorEngine.peakPressureCmh2o))) {
            gMonitorEngine.peakPressureCmh2o = lPressure;
        }
    }

    if ((monitorEngineFinite(lFlow) == 0U) ||
        ((lFlow > -MONITOR_FLOW_DEADBAND_LPM) &&
         (lFlow < MONITOR_FLOW_DEADBAND_LPM))) {
        return;
    }

    if ((gMonitorEngine.runState == MONITOR_STATE_INSP) &&
        (lFlow > gMonitorEngine.peakInspiratoryFlowLpm)) {
        gMonitorEngine.peakInspiratoryFlowLpm = lFlow;
    }

    if ((gMonitorEngine.runState == MONITOR_STATE_EXP) &&
        (-lFlow > gMonitorEngine.peakExpiratoryFlowLpm)) {
        gMonitorEngine.peakExpiratoryFlowLpm = -lFlow;
    }

    /* Keep a signed whole-breath integral for volume-balance diagnostics. */
    monitorEngineTidalVolumeIntegrate(MONITOR_TIDA_VOL, lFlow);

    /* Keep a signed flow integral for each phase. */
    if (gMonitorEngine.runState == MONITOR_STATE_INSP) {
        monitorEngineTidalVolumeIntegrate(MONITOR_TIDA_VOL_INSP, lFlow);
    } else if (gMonitorEngine.runState == MONITOR_STATE_EXP) {
        monitorEngineTidalVolumeIntegrate(MONITOR_TIDA_VOL_EXP, -lFlow);
    }
}

/** Handle cycle boundaries, reset invalid sessions and accumulate leak samples. */
static void monitorEngineBreathProcess(uint32_t nowMs)
{
    stBreathPlan lPlan;
    ePhaseControllerState lPhase = phaseControllerStateGet();

    /* Discard partial cycles and old coefficients across idle or re-zeroing. */
    if ((lPhase != PHASE_INSP && lPhase != PHASE_EXP) ||
        (gMonitorEngine.flowZeroOffsetLpm != controlDataMdiffFlowZeroOffsetGet()) ||
        (phaseControllerActivePlanGet(&lPlan) != PHASE_CONTROL_SUCCESS)) {
        monitorEngineInit();
        /* Recovery during inspiration must wait for a fresh phase boundary. */
        gMonitorEngine.inspirationObserved = (uint8_t)(lPhase == PHASE_INSP);
        return;
    }
    /* Plan boundaries are independent of proximal-flow direction. */
    if ((lPhase == PHASE_INSP) &&
        (((gMonitorEngine.breathActive == 0U) &&
          (gMonitorEngine.inspirationObserved == 0U)) ||
         ((gMonitorEngine.breathActive != 0U) &&
          (lPlan.sequence != gMonitorEngine.breathPlan.sequence)))) {
        if ((gMonitorEngine.breathActive != 0U) &&
            (gMonitorEngine.expirationSeen != 0U)) {
            monitorEngineBreathFinish(nowMs);
        } else {
            (void)monitorEngineSet(MONITOR_LEAK_VALID, 0.0F);
            (void)monitorEngineSet(MONITOR_LEAK_COEFFICIENT, 0.0F);
            (void)monitorEngineSet(MONITOR_LEAK_BALANCE_COEFFICIENT, 0.0F);
        }
        if (monitorEngineBreathStart(nowMs) != MONITOR_ENGINE_SUCCESS) {
            monitorEngineInit();
            gMonitorEngine.inspirationObserved = 1U;
            return;
        }
        gMonitorEngine.runState = MONITOR_STATE_INSP;
    }
    gMonitorEngine.inspirationObserved = (uint8_t)(lPhase == PHASE_INSP);
    if ((gMonitorEngine.breathActive != 0U) && (gMonitorEngine.breathCompleted == 0U)) {
        if ((monitorEngineFinite(controlDataGet(PAT_REAL_FLOW)) == 0U) ||
            (monitorEngineFinite(controlDataGet(PAT_REAL_PRS)) == 0U)) {
            gMonitorEngine.volumeInvalid = 1U;
        }
        if ((lPhase == PHASE_INSP) && (lPlan.limitSettings != NULL) &&
            (controlDataGet(PAT_REAL_PRS) >= lPlan.limitSettings->pressureHigh)) {
            gMonitorEngine.volumeLimited = 1U;
        }
        if ((lPhase == PHASE_EXP) && (gMonitorEngine.expirationSeen == 0U)) {
            gMonitorEngine.expirationSeen = 1U;
            gMonitorEngine.inspiratoryTimeMs = nowMs - gMonitorEngine.breathStartedMs;
            gMonitorEngine.cycleReason = phaseControllerCycleReasonGet();
        }
        monitorEngineLeakAccumulate(controlDataGet(PAT_REAL_FLOW));
        monitorEngineMeanPressureAccumulate();
    }
    /* Include PSV waiting for its first trigger; inspiration holds this value. */
    if ((lPhase == PHASE_EXP) && (gMonitorEngine.breathCompleted == 0U)) {
        monitorEngineDynamicPeepAccumulate();
        monitorEngineDynamicPeepCalculate();
        monitorEnginePeepDisplayProcess(lPlan.mode);
    }
}

void monitorEngineProcess(uint32_t nowMs)
{
    monitorEngineBreathProcess(nowMs);
    monitorEngineLeakFlowProcess();
    monitorEnginePlateauPressureProcess(nowMs);
    monitorEngineTidalVolumeProcess(nowMs);
}

/**************************End of file********************************/
