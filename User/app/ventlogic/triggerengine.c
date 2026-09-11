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
#include "log.h"

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
    gTriggerEngine.triggerType = VENT_TRIGGER_OFF;
    gTriggerEngine.pressureBaselineCmh2o = 0.0F;
    gTriggerEngine.pressureStableSamples = 0U;
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
    gTriggerEngine.triggerType = plan->allowedTriggerType;
    gTriggerEngine.pressureBaselineCmh2o = patientPressure;
    gTriggerEngine.pressureWindowMinCmh2o = patientPressure;
    gTriggerEngine.pressureWindowMaxCmh2o = patientPressure;
    gTriggerEngine.pressureStableSamples = 1U;
    gTriggerEngine.flowBaselineLpm = proximalFlow;
    gTriggerEngine.settleSamples = 1U;
    gTriggerEngine.confirmSamples = 0U;
}

/** Rebuild pressure reference only after a bounded, quiet pressure window. */
static void triggerEnginePressureBaselineUpdate(float patientPressure) {
    if ((gTriggerEngine.pressureStableSamples == 0U) ||
        (patientPressure < gTriggerEngine.pressureWindowMaxCmh2o -
         TRIGGER_ENGINE_PRESSURE_STABLE_RANGE_CMH2O) ||
        (patientPressure > gTriggerEngine.pressureWindowMinCmh2o +
         TRIGGER_ENGINE_PRESSURE_STABLE_RANGE_CMH2O)) {
        gTriggerEngine.pressureWindowMinCmh2o = patientPressure;
        gTriggerEngine.pressureWindowMaxCmh2o = patientPressure;
        gTriggerEngine.pressureStableSamples = 1U;
        return;
    }
    if (patientPressure < gTriggerEngine.pressureWindowMinCmh2o) {
        gTriggerEngine.pressureWindowMinCmh2o = patientPressure;
    }
    if (patientPressure > gTriggerEngine.pressureWindowMaxCmh2o) {
        gTriggerEngine.pressureWindowMaxCmh2o = patientPressure;
    }
    if (gTriggerEngine.pressureStableSamples < TRIGGER_ENGINE_SETTLE_SAMPLES) {
        gTriggerEngine.pressureStableSamples++;
    }
    if (gTriggerEngine.pressureStableSamples >= TRIGGER_ENGINE_SETTLE_SAMPLES) {
        gTriggerEngine.pressureBaselineCmh2o +=
            TRIGGER_ENGINE_BASELINE_GAIN *
            (patientPressure - gTriggerEngine.pressureBaselineCmh2o);
    }
}

/** Follow signed expiratory flow; retain the reference on a positive effort. */
static void triggerEngineFlowBaselineUpdate(float proximalFlow) {
    if ((gTriggerEngine.state == TRIGGER_ENGINE_SETTLING) ||
        (proximalFlow < 0.0F) ||
        (proximalFlow < gTriggerEngine.flowBaselineLpm)) {
        gTriggerEngine.flowBaselineLpm +=
            TRIGGER_ENGINE_FLOW_BASELINE_GAIN *
            (proximalFlow - gTriggerEngine.flowBaselineLpm);
    }
    /* Do not carry a small negative residual into the zero-flow interval. */
    if ((proximalFlow >= 0.0F) && (gTriggerEngine.flowBaselineLpm < 0.0F)) {
        gTriggerEngine.flowBaselineLpm = 0.0F;
    }
}

/** Detect pressure effort before allowing stable samples to move its reference. */
static bool triggerEnginePressureProcess(float patientPressure, float peep, float threshold) {
    if (gTriggerEngine.pressureBaselineCmh2o > peep) {
        gTriggerEngine.pressureBaselineCmh2o = peep;
    }
    if ((gTriggerEngine.pressureBaselineCmh2o - patientPressure) >= threshold) {
        gTriggerEngine.pressureStableSamples = 0U;
        return true;
    }
    if (patientPressure <= peep + TRIGGER_ENGINE_PEEP_TOLERANCE_CMH2O) {
        triggerEnginePressureBaselineUpdate(patientPressure);
    } else {
        gTriggerEngine.pressureStableSamples = 0U;
    }
    return false;
}

/** Detect effort relative to moving expiratory flow, even before zero crossing. */
static bool triggerEngineFlowProcess(float proximalFlow, float threshold) {
    if ((proximalFlow - gTriggerEngine.flowBaselineLpm) >= threshold) {
        return true;
    }
    triggerEngineFlowBaselineUpdate(proximalFlow);
    return false;
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
    float lTriggerThreshold;
    bool lCandidate;

    if ((lPhase != PHASE_EXP) ||
        (phaseControllerActivePlanGet(&lPlan) != PHASE_CONTROL_SUCCESS) ||
        ((lPlan.mode != VENT_MD_PAC) &&
         (lPlan.mode != VENT_MD_VAC) &&
         (lPlan.mode != VENT_MD_CPAP_PSV) &&
         (lPlan.mode != VENT_MD_PSV_ST)) ||
        (lPlan.allowedTriggerType == VENT_TRIGGER_OFF)) {
        triggerEngineIdleEnter(lPhase);
        return;
    }

    lPatientPressure = controlDataGet(PAT_REAL_PRS);
    lProximalFlow = controlDataGet(PAT_REAL_FLOW);
    if (!triggerEngineFinite(lPatientPressure) ||
        !triggerEngineFinite(lProximalFlow)) {
        triggerEngineIdleEnter(lPhase);
        return;
    }

    /* Reject residual high pressure, but allow an actual baseline below PEEP. */
    if ((lPlan.allowedTriggerType == VENT_TRIGGER_PRESSURE) &&
        ((gTriggerEngine.state != TRIGGER_ENGINE_ARMED) ||
         (gTriggerEngine.planSequence != lPlan.sequence)) &&
        ((lPatientPressure - lPlan.peepCmh2o) >
         TRIGGER_ENGINE_PEEP_TOLERANCE_CMH2O)) {
        triggerEngineIdleEnter(lPhase);
        return;
    }

    if ((gTriggerEngine.previousPhase != PHASE_EXP) ||
        (gTriggerEngine.planSequence != lPlan.sequence) ||
        (gTriggerEngine.triggerType != lPlan.allowedTriggerType) ||
        (gTriggerEngine.state == TRIGGER_ENGINE_IDLE)) {
        triggerEngineSettlingEnter(&lPlan, lPatientPressure, lProximalFlow);
        gTriggerEngine.previousPhase = lPhase;
        return;
    }
    gTriggerEngine.previousPhase = lPhase;

    if (gTriggerEngine.state == TRIGGER_ENGINE_SETTLING) {
        if (lPlan.allowedTriggerType == VENT_TRIGGER_PRESSURE) {
            triggerEnginePressureBaselineUpdate(lPatientPressure);
            if (gTriggerEngine.pressureStableSamples < TRIGGER_ENGINE_SETTLE_SAMPLES) {
                return;
            }
            gTriggerEngine.pressureBaselineCmh2o = lPatientPressure;
            gTriggerEngine.state = TRIGGER_ENGINE_ARMED;
            return;
        } else {
            triggerEngineFlowBaselineUpdate(lProximalFlow);
        }
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
        lTriggerThreshold = triggerEngineMagnitude(lPlan.pressureTriggerCmh2o);
    } else if (lPlan.allowedTriggerType == VENT_TRIGGER_FLOW) {
        lTriggerReason = BREATH_TRIGGER_REASON_FLOW;
        lTriggerThreshold = lPlan.flowTriggerLpm;
    } else {
        triggerEngineIdleEnter(lPhase);
        return;
    }

    if (!triggerEngineFinite(lTriggerThreshold) || (lTriggerThreshold <= 0.0F)) {
        triggerEngineIdleEnter(lPhase);
        return;
    }

    if (phaseControllerExpirationReadyGet() == 0U) {
        gTriggerEngine.confirmSamples = 0U;
        if (lPlan.allowedTriggerType == VENT_TRIGGER_FLOW) {
            triggerEngineFlowBaselineUpdate(lProximalFlow);
        } else if (lPatientPressure <= lPlan.peepCmh2o +
                   TRIGGER_ENGINE_PEEP_TOLERANCE_CMH2O) {
            triggerEnginePressureBaselineUpdate(lPatientPressure);
        } else {
            gTriggerEngine.pressureStableSamples = 0U;
        }
        return;
    }
    lCandidate = (lPlan.allowedTriggerType == VENT_TRIGGER_PRESSURE) ?
        triggerEnginePressureProcess(lPatientPressure, lPlan.peepCmh2o, lTriggerThreshold) :
        triggerEngineFlowProcess(lProximalFlow, lTriggerThreshold);
    if (!lCandidate) {
        gTriggerEngine.confirmSamples = 0U;
        return;
    }

    if (gTriggerEngine.confirmSamples < TRIGGER_ENGINE_CONFIRM_SAMPLES) {
        gTriggerEngine.confirmSamples++;
    }
    if ((gTriggerEngine.confirmSamples >= TRIGGER_ENGINE_CONFIRM_SAMPLES) &&
        (phaseControllerTrigger(lTriggerReason, nowMs) == PHASE_CONTROL_SUCCESS)) {
        LOG_I("trigger", "reason=%u p100=%ld pb100=%ld q100=%ld qb100=%ld threshold100=%ld",
              (unsigned int)lTriggerReason,
              (long)(lPatientPressure * 100.0F),
              (long)(gTriggerEngine.pressureBaselineCmh2o * 100.0F),
              (long)(lProximalFlow * 100.0F),
              (long)(gTriggerEngine.flowBaselineLpm * 100.0F),
              (long)(lTriggerThreshold * 100.0F));
        triggerEngineIdleEnter(phaseControllerStateGet());
    }
}

/**************************End of file********************************/
