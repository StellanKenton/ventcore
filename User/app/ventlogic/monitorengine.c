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
}

/** Average valid zero-flow pressure samples from the plateau window. */
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

    lFlow = controlDataGet(MDIFF_REAL_FLOW) -
            monitorEngineGet(MONITOR_LEAK_FLOW);
    lPressure = controlDataGet(PAT_REAL_PRS);
    if ((monitorEngineFinite(lFlow) == 0U) ||
        (monitorEngineFinite(lPressure) == 0U) ||
        (lFlow <= -MONITOR_FLOW_DEADBAND_LPM) ||
        (lFlow >= MONITOR_FLOW_DEADBAND_LPM)) {
        return;
    }

    gMonitorEngine.plateauPressureSumCmh2o += lPressure;
    gMonitorEngine.plateauPressureSampleCount++;
    (void)monitorEngineSet(
        MONITOR_PLATEAU_PRS,
        gMonitorEngine.plateauPressureSumCmh2o /
        (float)gMonitorEngine.plateauPressureSampleCount);
}

/** Retain the latest valid expiration pressures and the expiration minimum. */
static void monitorEngineDynamicPeepAccumulate(void) {
    float lPressure = controlDataGet(PAT_REAL_PRS);
    float lFlow = controlDataGet(MDIFF_REAL_FLOW);
    float lSlope;
    float lFlowSlope;

    if (monitorEngineFinite(lPressure) == 0U) {
        gMonitorEngine.peepPreviousValid = 0U;
        return;
    }
    if (lPressure < gMonitorEngine.peepMinimumCmh2o) {
        gMonitorEngine.peepMinimumCmh2o = lPressure;
    }
    if (monitorEngineFinite(lFlow) == 0U) {
        gMonitorEngine.peepPreviousValid = 0U;
        return;
    }
    lSlope = (lPressure - gMonitorEngine.peepPreviousCmh2o) /
             MONITOR_DYN_PEEP_SAMPLE_INTERVAL_S;
    lFlowSlope = (lFlow - gMonitorEngine.peepPreviousFlowLpm) /
                 MONITOR_DYN_PEEP_SAMPLE_INTERVAL_S;
    if ((gMonitorEngine.peepPreviousValid != 0U) &&
        (lSlope > -MONITOR_DYN_PEEP_SLOPE_LIMIT) &&
        (lSlope < MONITOR_DYN_PEEP_SLOPE_LIMIT) &&
        (lFlow >= -MONITOR_DYN_PEEP_FLOW_LIMIT_LPM) &&
        (lFlow <= MONITOR_DYN_PEEP_FLOW_LIMIT_LPM) &&
        (lFlowSlope > -MONITOR_DYN_PEEP_FLOW_SLOPE_LIMIT) &&
        (lFlowSlope < MONITOR_DYN_PEEP_FLOW_SLOPE_LIMIT)) {
        gMonitorEngine.peepSamplesCmh2o[gMonitorEngine.peepSampleIndex] = lPressure;
        gMonitorEngine.peepSampleIndex = (uint8_t)((gMonitorEngine.peepSampleIndex + 1U) %
                                                  MONITOR_DYN_PEEP_WINDOW_SIZE);
        if (gMonitorEngine.peepSampleCount < MONITOR_DYN_PEEP_WINDOW_SIZE) {
            gMonitorEngine.peepSampleCount++;
        }
    }
    /* Always compare adjacent samples, including rejected finite measurements. */
    gMonitorEngine.peepPreviousCmh2o = lPressure;
    gMonitorEngine.peepPreviousFlowLpm = lFlow;
    gMonitorEngine.peepPreviousValid = 1U;
}

/** Publish the latest five-point average, or the minimum for a short window. */
static void monitorEngineDynamicPeepCalculate(void) {
    float lPressure = 0.0F;
    uint8_t lIndex;

    if (gMonitorEngine.peepSampleCount == MONITOR_DYN_PEEP_WINDOW_SIZE) {
        for (lIndex = 0U; lIndex < gMonitorEngine.peepSampleCount; lIndex++) {
            lPressure += gMonitorEngine.peepSamplesCmh2o[lIndex];
        }
        lPressure /= (float)gMonitorEngine.peepSampleCount;
    } else if (gMonitorEngine.peepMinimumCmh2o != FLT_MAX) {
        lPressure = gMonitorEngine.peepMinimumCmh2o;
    }
    (void)monitorEngineSet(MONITOR_DYN_PEEP, lPressure);
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

/** Publish the breath that ended immediately before a new inspiration. */
static void monitorEngineBreathResultPublish(uint32_t nowMs)
{
    float lPeepPressure = controlDataGet(PAT_REAL_PRS);
    stBreathResult lResult = {0};

    lResult.sequence = gMonitorEngine.breathPlan.sequence;
    lResult.mode = gMonitorEngine.breathPlan.mode;
    lResult.breathType = gMonitorEngine.breathPlan.breathType;
    lResult.triggerReason = gMonitorEngine.breathPlan.triggerReason;
    lResult.vtiMl = gMonitorData[MONITOR_TIDA_VOL_INSP];
    lResult.vteMl = gMonitorData[MONITOR_TIDA_VOL_EXP];
    lResult.ppeakCmh2o = gMonitorEngine.peakPressureCmh2o;
    lResult.plateauPressureCmh2o = gMonitorData[MONITOR_PLATEAU_PRS];
    lResult.peepCmh2o = lPeepPressure;
    lResult.peakInspiratoryFlowLpm = gMonitorEngine.peakInspiratoryFlowLpm;
    lResult.cycleReason = gMonitorEngine.cycleReason;
    lResult.inspiratoryTimeMs = gMonitorEngine.inspiratoryTimeMs;
    lResult.cycleTimeMs = nowMs - gMonitorEngine.breathStartedMs;
    lResult.validMask = BREATH_RESULT_VALID_COMPLETE |
                        BREATH_RESULT_VALID_CYCLE_TIME |
                        BREATH_RESULT_VALID_INSPIRATORY_TIME;
    if ((monitorEngineFinite(lResult.vtiMl) != 0U) &&
        (gMonitorEngine.volumeInvalid == 0U)) {
        lResult.validMask |= BREATH_RESULT_VALID_VTI;
    }
    if (monitorEngineFinite(lResult.vteMl) != 0U) {
        lResult.validMask |= BREATH_RESULT_VALID_VTE;
    }
    if (monitorEngineFinite(lResult.ppeakCmh2o) != 0U) {
        lResult.validMask |= BREATH_RESULT_VALID_PPEAK;
    }
    if ((gMonitorEngine.plateauPressureSampleCount > 0U) &&
        (monitorEngineFinite(lResult.plateauPressureCmh2o) != 0U)) {
        lResult.validMask |= BREATH_RESULT_VALID_PLATEAU_PRESSURE;
    }
    if (monitorEngineFinite(lResult.peepCmh2o) != 0U) {
        lResult.validMask |= BREATH_RESULT_VALID_PEEP;
    }
    if (monitorEngineFinite(lResult.peakInspiratoryFlowLpm) != 0U) {
        lResult.validMask |= BREATH_RESULT_VALID_PEAK_INSP_FLOW;
    }
    if (gMonitorEngine.volumeLimited != 0U) {
        lResult.validMask |= BREATH_RESULT_VOLUME_LIMITED;
    }
    repRtosEnterCritical();
    gMonitorData[MONITOR_LAST_TIDA_VOL_INSP] = lResult.vtiMl;
    gMonitorData[MONITOR_LAST_TIDA_VOL_EXP] = lResult.vteMl;
    gMonitorData[MONITOR_LAST_PPEAK] = lResult.ppeakCmh2o;
    gMonitorData[MONITOR_LAST_PLATEAU_PRS] = lResult.plateauPressureCmh2o;
    gMonitorData[MONITOR_LAST_PEEP] = lResult.peepCmh2o;
    gMonitorData[MONITOR_LAST_PEAK_INSP_FLOW] = lResult.peakInspiratoryFlowLpm;
    gMonitorData[MONITOR_LAST_INSP_TIME_MS] = (float)lResult.inspiratoryTimeMs;
    gMonitorData[MONITOR_LAST_CYCLE_TIME_MS] = (float)lResult.cycleTimeMs;
    gMonitorLatestBreathResult = lResult;
    gMonitorBreathResultAvailable = 1U;
    repRtosExitCritical();
    breathSchedulerVolumeFeedback(&gMonitorEngine.breathPlan, lResult.vtiMl,
        (uint8_t)(((lResult.validMask & BREATH_RESULT_VALID_VTI) != 0U) &&
                  (gMonitorEngine.volumeLimited == 0U) &&
                  (gMonitorEngine.runState == MONITOR_STATE_EXP) &&
                  (lResult.cycleReason == BREATH_CYCLE_REASON_TIME)));
}

/** Complete a cycle once, shared by explicit and observed breath boundaries. */
static void monitorEngineBreathFinish(uint32_t nowMs) {
    if ((gMonitorEngine.breathActive != 0U) &&
        (gMonitorEngine.expirationSeen != 0U) &&
        (gMonitorEngine.breathCompleted == 0U)) {
        monitorEngineLeakCoefficientCalculate();
        monitorEngineDynamicPeepCalculate();
        monitorEngineBreathResultPublish(nowMs);
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
    gMonitorEngine.plateauPressureSumCmh2o = 0.0F;
    gMonitorEngine.plateauPressureSampleCount = 0U;
    gMonitorEngine.peepMinimumCmh2o = FLT_MAX;
    gMonitorEngine.peepPreviousValid = 0U;
    gMonitorEngine.peepSampleCount = 0U;
    gMonitorEngine.peepSampleIndex = 0U;
    gMonitorEngine.leakFlowSumLpm = 0.0F;
    gMonitorEngine.leakPressureRootSum = 0.0F;
    gMonitorEngine.leakCycleInvalid = 0U;
    gMonitorEngine.expirationSeen = 0U;
    gMonitorEngine.breathCompleted = 0U;
    gMonitorEngine.volumeInvalid = 0U;
    gMonitorEngine.volumeLimited = 0U;
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
    float lFlow = controlDataGet(MDIFF_REAL_FLOW);
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
        if ((monitorEngineFinite(controlDataGet(MDIFF_REAL_FLOW)) == 0U) ||
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
        monitorEngineLeakAccumulate(controlDataGet(MDIFF_REAL_FLOW));
        if (lPhase == PHASE_EXP) {
            monitorEngineDynamicPeepAccumulate();
        }
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
