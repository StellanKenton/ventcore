/************************************************************************************
* @file     : cycleengine.c
* @brief    : Breath cycle engine.
* @details  : Provides cycle engine initialization and periodic processing.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "cycleengine.h"

#include <float.h>
#include <string.h>

#include "controldata.h"

static stCycleEngine gCycleEngine;

/** Reset flow-cycle tracking outside a supported inspiration. */
static void cycleEngineIdleEnter(ePhaseControllerState phase)
{
    gCycleEngine.state = CYCLE_ENGINE_IDLE;
    gCycleEngine.previousPhase = phase;
    gCycleEngine.planSequence = 0U;
    gCycleEngine.inspirationStartedMs = 0U;
    gCycleEngine.peakInspiratoryFlowLpm = 0.0F;
    gCycleEngine.confirmSamples = 0U;
}

/** Return true for a finite proximal-flow sample. */
static uint8_t cycleEngineFlowValid(float flow)
{
    return (uint8_t)((flow >= -FLT_MAX) && (flow <= FLT_MAX));
}

void cycleEngineInit(void)
{
    (void)memset(&gCycleEngine, 0, sizeof(gCycleEngine));
    gCycleEngine.previousPhase = PHASE_IDLE;
}

void cycleEngineProcess(uint32_t nowMs)
{
    stBreathPlan lPlan;
    ePhaseControllerState lPhase = phaseControllerStateGet();
    float lCycleThreshold;
    float lFlow;
    uint32_t lInspiratoryElapsedMs;

    if ((lPhase != PHASE_INSP) ||
        (phaseControllerActivePlanGet(&lPlan) != PHASE_CONTROL_SUCCESS) ||
        (lPlan.breathType != BREATH_TYPE_SPONTANEOUS_PRESSURE_SUPPORT) ||
        (lPlan.cycleType != BREATH_CYCLE_TYPE_FLOW)) {
        cycleEngineIdleEnter(lPhase);
        return;
    }

    if ((gCycleEngine.state == CYCLE_ENGINE_IDLE) ||
        (gCycleEngine.planSequence != lPlan.sequence)) {
        gCycleEngine.state = CYCLE_ENGINE_TRACKING;
        gCycleEngine.planSequence = lPlan.sequence;
        gCycleEngine.inspirationStartedMs = nowMs;
        gCycleEngine.peakInspiratoryFlowLpm = 0.0F;
        gCycleEngine.confirmSamples = 0U;
    }
    gCycleEngine.previousPhase = lPhase;

    lInspiratoryElapsedMs = nowMs - gCycleEngine.inspirationStartedMs;
    if (lInspiratoryElapsedMs >= lPlan.maximumInspiratoryTimeMs) {
        if (phaseControllerCycle(BREATH_CYCLE_REASON_MAX_INSPIRATORY_TIME,
                                 nowMs) == PHASE_CONTROL_SUCCESS) {
            cycleEngineIdleEnter(phaseControllerStateGet());
        }
        return;
    }
    lFlow = controlDataGet(MDIFF_REAL_FLOW);
    if (cycleEngineFlowValid(lFlow) == 0U) {
        gCycleEngine.confirmSamples = 0U;
        return;
    }
    if (lFlow > gCycleEngine.peakInspiratoryFlowLpm) {
        gCycleEngine.peakInspiratoryFlowLpm = lFlow;
    }
    if ((lInspiratoryElapsedMs < lPlan.riseTimeMs) ||
        (lInspiratoryElapsedMs < lPlan.minimumInspiratoryTimeMs) ||
        (gCycleEngine.peakInspiratoryFlowLpm <
         CYCLE_ENGINE_MINIMUM_PEAK_FLOW_LPM)) {
        return;
    }

    lCycleThreshold = gCycleEngine.peakInspiratoryFlowLpm *
                      lPlan.cycleOffPercent / 100.0F;
    if (lFlow > lCycleThreshold) {
        gCycleEngine.confirmSamples = 0U;
        return;
    }
    if (gCycleEngine.confirmSamples < CYCLE_ENGINE_CONFIRM_SAMPLES) {
        gCycleEngine.confirmSamples++;
    }
    if ((gCycleEngine.confirmSamples >= CYCLE_ENGINE_CONFIRM_SAMPLES) &&
        (phaseControllerCycle(BREATH_CYCLE_REASON_FLOW, nowMs) ==
         PHASE_CONTROL_SUCCESS)) {
        cycleEngineIdleEnter(phaseControllerStateGet());
    }
}

/**************************End of file********************************/
