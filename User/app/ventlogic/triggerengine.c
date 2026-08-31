/************************************************************************************
* @file     : triggerengine.c
* @brief    : Breath trigger engine.
* @details  : Provides trigger engine initialization and periodic processing.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "triggerengine.h"

#include <float.h>
#include <stdbool.h>
#include <string.h>

#include "controldata.h"

static stTriggerEngine gTriggerEngine;

/** Return true when a sensor value can be used by trigger detection. */
static bool triggerEngineFinite(float value)
{
    return ((value == value) && (value <= FLT_MAX) && (value >= -FLT_MAX));
}

/** Return the positive magnitude of a configured pressure threshold. */
static float triggerEngineMagnitude(float value)
{
    return (value < 0.0F) ? -value : value;
}

/** Leave trigger detection idle and discard any pending candidate. */
static void triggerEngineIdleEnter(ePhaseControllerState phase)
{
    gTriggerEngine.state = TRIGGER_ENGINE_IDLE;
    gTriggerEngine.previousPhase = phase;
    gTriggerEngine.planSequence = 0U;
    gTriggerEngine.pressureBaselineCmh2o = 0.0F;
    gTriggerEngine.flowBaselineLpm = 0.0F;
    gTriggerEngine.settleSamples = 0U;
    gTriggerEngine.confirmSamples = 0U;
}

/** Start building pressure and flow baselines for one PEEP interval. */
static void triggerEngineSettlingEnter(const stBreathPlan *plan,
                                       float patientPressure,
                                       float proximalFlow)
{
    gTriggerEngine.state = TRIGGER_ENGINE_SETTLING;
    gTriggerEngine.planSequence = plan->sequence;
    gTriggerEngine.pressureBaselineCmh2o = patientPressure;
    gTriggerEngine.flowBaselineLpm = proximalFlow;
    gTriggerEngine.settleSamples = 1U;
    gTriggerEngine.confirmSamples = 0U;
}

/** Track the stable expiratory baseline with a lightweight IIR filter. */
static void triggerEngineBaselineUpdate(float patientPressure, float proximalFlow)
{
    gTriggerEngine.pressureBaselineCmh2o +=
        TRIGGER_ENGINE_BASELINE_GAIN *
        (patientPressure - gTriggerEngine.pressureBaselineCmh2o);
    gTriggerEngine.flowBaselineLpm +=
        TRIGGER_ENGINE_BASELINE_GAIN *
        (proximalFlow - gTriggerEngine.flowBaselineLpm);
}

void triggerEngineInit(void)
{
    (void)memset(&gTriggerEngine, 0, sizeof(gTriggerEngine));
    gTriggerEngine.previousPhase = PHASE_IDLE;
}

void triggerEngineProcess(uint32_t nowMs)
{
    stBreathPlan lPlan;
    eBreathTriggerReason lTriggerReason;
    ePhaseControllerState lPhase = phaseControllerStateGet();
    float lPatientPressure;
    float lProximalFlow;
    float lTriggerEffort;
    float lTriggerThreshold;
    bool lCandidate;

    if ((lPhase != PHASE_EXP) ||
        (phaseControllerExpirationReadyGet() == 0U) ||
        (phaseControllerActivePlanGet(&lPlan) != PHASE_CONTROL_SUCCESS) ||
        ((lPlan.mode != VENT_MD_PAC) &&
         (lPlan.mode != VENT_MD_CPAP_PSV) &&
         (lPlan.mode != VENT_MD_PSV_ST)) ||
        (lPlan.allowedTriggerType == VENT_TRIGGER_OFF)) {
        triggerEngineIdleEnter(lPhase);
        return;
    }

    lPatientPressure = controlDataGet(PAT_REAL_PRS);
    lProximalFlow = controlDataGet(MDIFF_REAL_FLOW);
    if (!triggerEngineFinite(lPatientPressure) ||
        !triggerEngineFinite(lProximalFlow)) {
        triggerEngineIdleEnter(lPhase);
        return;
    }

    if ((gTriggerEngine.previousPhase != PHASE_EXP) ||
        (gTriggerEngine.planSequence != lPlan.sequence) ||
        (gTriggerEngine.state == TRIGGER_ENGINE_IDLE)) {
        triggerEngineSettlingEnter(&lPlan, lPatientPressure, lProximalFlow);
        gTriggerEngine.previousPhase = lPhase;
        return;
    }
    gTriggerEngine.previousPhase = lPhase;

    if (gTriggerEngine.state == TRIGGER_ENGINE_SETTLING) {
        triggerEngineBaselineUpdate(lPatientPressure, lProximalFlow);
        if (gTriggerEngine.settleSamples < TRIGGER_ENGINE_SETTLE_SAMPLES) {
            gTriggerEngine.settleSamples++;
        }
        if (gTriggerEngine.settleSamples >= TRIGGER_ENGINE_SETTLE_SAMPLES) {
            gTriggerEngine.state = TRIGGER_ENGINE_ARMED;
        }
        return;
    }

    if (lPlan.allowedTriggerType == VENT_TRIGGER_PRESSURE) {
        lTriggerReason = BREATH_TRIGGER_REASON_PRESSURE;
        lTriggerEffort = gTriggerEngine.pressureBaselineCmh2o - lPatientPressure;
        lTriggerThreshold = triggerEngineMagnitude(lPlan.pressureTriggerCmh2o);
    } else if (lPlan.allowedTriggerType == VENT_TRIGGER_FLOW) {
        lTriggerReason = BREATH_TRIGGER_REASON_FLOW;
        lTriggerEffort = lProximalFlow - gTriggerEngine.flowBaselineLpm;
        lTriggerThreshold = lPlan.flowTriggerLpm;
    } else {
        triggerEngineIdleEnter(lPhase);
        return;
    }

    lCandidate = (lTriggerEffort >= lTriggerThreshold);
    if (!lCandidate) {
        gTriggerEngine.confirmSamples = 0U;
        triggerEngineBaselineUpdate(lPatientPressure, lProximalFlow);
        return;
    }

    if (gTriggerEngine.confirmSamples < TRIGGER_ENGINE_CONFIRM_SAMPLES) {
        gTriggerEngine.confirmSamples++;
    }
    if ((gTriggerEngine.confirmSamples >= TRIGGER_ENGINE_CONFIRM_SAMPLES) &&
        (phaseControllerTrigger(lTriggerReason, nowMs) == PHASE_CONTROL_SUCCESS)) {
        triggerEngineIdleEnter(phaseControllerStateGet());
    }
}

/**************************End of file********************************/
