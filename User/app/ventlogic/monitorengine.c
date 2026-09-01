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
    lResult.peepCmh2o = lPeepPressure;
    lResult.peakInspiratoryFlowLpm = gMonitorEngine.peakInspiratoryFlowLpm;
    lResult.cycleReason = gMonitorEngine.cycleReason;
    lResult.inspiratoryTimeMs = gMonitorEngine.inspiratoryTimeMs;
    lResult.cycleTimeMs = nowMs - gMonitorEngine.breathStartedMs;
    lResult.validMask = BREATH_RESULT_VALID_COMPLETE |
                        BREATH_RESULT_VALID_CYCLE_TIME |
                        BREATH_RESULT_VALID_INSPIRATORY_TIME;
    if (monitorEngineFinite(lResult.vtiMl) != 0U) {
        lResult.validMask |= BREATH_RESULT_VALID_VTI;
    }
    if (monitorEngineFinite(lResult.vteMl) != 0U) {
        lResult.validMask |= BREATH_RESULT_VALID_VTE;
    }
    if (monitorEngineFinite(lResult.ppeakCmh2o) != 0U) {
        lResult.validMask |= BREATH_RESULT_VALID_PPEAK;
    }
    if (monitorEngineFinite(lResult.peepCmh2o) != 0U) {
        lResult.validMask |= BREATH_RESULT_VALID_PEEP;
    }
    if (monitorEngineFinite(lResult.peakInspiratoryFlowLpm) != 0U) {
        lResult.validMask |= BREATH_RESULT_VALID_PEAK_INSP_FLOW;
    }
    repRtosEnterCritical();
    gMonitorLatestBreathResult = lResult;
    gMonitorBreathResultAvailable = 1U;
    repRtosExitCritical();
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
    gMonitorEngine.inspiratoryTimeMs = 0U;
    gMonitorEngine.cycleReason = BREATH_CYCLE_REASON_NONE;
    gMonitorEngine.breathActive = 1U;
    (void)monitorEngineSet(MONITOR_TIDA_VOL, 0.0F);
    (void)monitorEngineSet(MONITOR_TIDA_VOL_INSP, 0.0F);
    (void)monitorEngineSet(MONITOR_TIDA_VOL_EXP, 0.0F);
    return MONITOR_ENGINE_SUCCESS;
}

void monitorEngineInit(void)
{
    (void)memset(&gMonitorEngine, 0, sizeof(gMonitorEngine));
    (void)memset(&gMonitorLatestBreathResult, 0,
                 sizeof(gMonitorLatestBreathResult));
    gMonitorEngine.runState = MONITOR_STATE_IDLE;
    gMonitorBreathResultAvailable = 0U;
    (void)monitorEngineSet(MONITOR_TIDA_VOL, 0.0F);
    (void)monitorEngineSet(MONITOR_TIDA_VOL_INSP, 0.0F);
    (void)monitorEngineSet(MONITOR_TIDA_VOL_EXP, 0.0F);
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

void monitorEngineProcess(uint32_t nowMs)
{
    eMonitorEngineState lNextState =
        monitorEngineStateFromPhase(phaseControllerStateGet());
    float lFlow;
    float lPressure;

    if ((lNextState == MONITOR_STATE_INSP) &&
        (gMonitorEngine.runState != MONITOR_STATE_INSP)) {
        if (gMonitorEngine.breathActive != 0U) {
            monitorEngineBreathResultPublish(nowMs);
        }
        if (monitorEngineBreathStart(nowMs) != MONITOR_ENGINE_SUCCESS) {
            gMonitorEngine.breathActive = 0U;
        }
    } else if ((lNextState == MONITOR_STATE_EXP) &&
               (gMonitorEngine.runState != MONITOR_STATE_EXP)) {
        if (gMonitorEngine.breathActive != 0U) {
            gMonitorEngine.inspiratoryTimeMs = nowMs - gMonitorEngine.breathStartedMs;
            gMonitorEngine.cycleReason = phaseControllerCycleReasonGet();
        }
        (void)monitorEngineSet(MONITOR_TIDA_VOL_EXP, 0.0F);
    }
    gMonitorEngine.runState = lNextState;

    if ((gMonitorEngine.runState == MONITOR_STATE_IDLE) ||
        (gMonitorEngine.breathActive == 0U)) {
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

    lFlow = controlDataGet(MDIFF_REAL_FLOW);
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

    /*
     * VTi/VTe are net phase volumes, not one-direction-only sums.
     * This preserves reverse flow near phase transitions and PEEP recovery:
     *   inspiration: +flow increases VTi, -flow decreases VTi
     *   expiration : -flow increases VTe, +flow decreases VTe
     */
    if (gMonitorEngine.runState == MONITOR_STATE_INSP) {
        monitorEngineTidalVolumeIntegrate(MONITOR_TIDA_VOL_INSP, lFlow);
    } else if (gMonitorEngine.runState == MONITOR_STATE_EXP) {
        monitorEngineTidalVolumeIntegrate(MONITOR_TIDA_VOL_EXP, -lFlow);
    }
}

/**************************End of file********************************/
