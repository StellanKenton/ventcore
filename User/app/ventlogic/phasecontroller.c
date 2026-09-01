/************************************************************************************
* @file     : phasecontroller.c
* @brief    : Breath plan phase executor.
* @details  : Executes scheduler-selected pressure or volume breath plans.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "phasecontroller.h"

#include <string.h>

#include "controldata.h"

static stPhaseController gPhaseController;
static float gPhaseData[PHASE_COUNT];

/** Clear all phase references. */
static void phaseControllerReferencesClear(void)
{
    (void)phaseControlSet(PHASE_REF_PRESSURE, 0.0F);
    (void)phaseControlSet(PHASE_REF_FLOW, 0.0F);
}

/** Enter idle and discard the active breath plan. */
static void phaseControllerIdleEnter(void)
{
    gPhaseController.runState = PHASE_IDLE;
    gPhaseController.inspirationStartedMs = 0U;
    gPhaseController.expirationStartedMs = 0U;
    gPhaseController.planValid = 0U;
    gPhaseController.breathStarted = 0U;
    gPhaseController.expirationCaptureComplete = 0U;
    gPhaseController.cycleReason = BREATH_CYCLE_REASON_NONE;
    (void)memset(&gPhaseController.activePlan, 0, sizeof(gPhaseController.activePlan));
    phaseControllerReferencesClear();
}

/** Load the scheduler-selected plan used by the next inspiration. */
static int8_t phaseControllerPlanLoad(eBreathTriggerReason triggerReason)
{
    int8_t lStatus = breathSchedulerNextPlanGet(triggerReason,
                                                &gPhaseController.activePlan);

    if (lStatus != BREATH_CONTROL_SUCCESS) {
        gPhaseController.planValid = 0U;
        return PHASE_CONTROL_ERROR_STATE;
    }
    gPhaseController.planValid = 1U;
    return PHASE_CONTROL_SUCCESS;
}

/** Begin expiration before the first scheduled inspiration. */
static int8_t phaseControllerInitialExpirationStart(uint32_t nowMs)
{
    if (phaseControllerPlanLoad(BREATH_TRIGGER_REASON_TIME) != PHASE_CONTROL_SUCCESS) {
        return PHASE_CONTROL_ERROR_STATE;
    }
    gPhaseController.expirationStartedMs = nowMs;
    gPhaseController.expirationCaptureComplete = 0U;
    gPhaseController.runState = PHASE_EXP;
    return PHASE_CONTROL_SUCCESS;
}

/** Start inspiration using the next plan selected for the trigger reason. */
static int8_t phaseControllerInspirationStart(eBreathTriggerReason triggerReason,
                                              uint32_t nowMs)
{
    float lPatientPressure;

    if ((gPhaseController.breathStarted != 0U) ||
        (triggerReason == BREATH_TRIGGER_REASON_APNEA_BACKUP)) {
        if (phaseControllerPlanLoad(triggerReason) != PHASE_CONTROL_SUCCESS) {
            return PHASE_CONTROL_ERROR_STATE;
        }
    } else {
        gPhaseController.activePlan.triggerReason = triggerReason;
    }

    lPatientPressure = controlDataGet(PAT_REAL_PRS);
    gPhaseController.inspirationStartedMs = nowMs;
    gPhaseController.cycleReason = BREATH_CYCLE_REASON_NONE;
    gPhaseController.breathStarted = 1U;
    gPhaseController.expirationCaptureComplete = 0U;
    (void)phaseControlSet(PHASE_REF_PRESSURE, lPatientPressure);
    if (gPhaseController.activePlan.breathType == BREATH_TYPE_MANDATORY_VOLUME) {
        (void)phaseControlSet(PHASE_REF_FLOW,
                              gPhaseController.activePlan.inspiratoryFlowLpm);
    } else {
        (void)phaseControlSet(PHASE_REF_FLOW, 0.0F);
    }
    gPhaseController.runState = PHASE_INSP;
    return PHASE_CONTROL_SUCCESS;
}

void phaseControllerInit(void)
{
    phaseControllerIdleEnter();
}

int8_t phaseControlSet(ePhaseControlType type, float value)
{
    if ((type <= PHASE_NONE) || (type >= PHASE_COUNT)) {
        return PHASE_CONTROL_ERROR_PARAM;
    }
    gPhaseData[type] = value;
    return PHASE_CONTROL_SUCCESS;
}

float phaseControlGet(ePhaseControlType type)
{
    if ((type <= PHASE_NONE) || (type >= PHASE_COUNT)) {
        return 0.0F;
    }
    return gPhaseData[type];
}

ePhaseControllerState phaseControllerStateGet(void)
{
    return gPhaseController.runState;
}

int8_t phaseControllerActivePlanGet(stBreathPlan *plan)
{
    if (plan == NULL) {
        return PHASE_CONTROL_ERROR_PARAM;
    }
    if (gPhaseController.planValid == 0U) {
        return PHASE_CONTROL_ERROR_STATE;
    }
    *plan = gPhaseController.activePlan;
    return PHASE_CONTROL_SUCCESS;
}

int8_t phaseControllerTrigger(eBreathTriggerReason triggerReason, uint32_t nowMs)
{
    uint32_t lExpirationElapsedMs;

    if ((triggerReason != BREATH_TRIGGER_REASON_PRESSURE) &&
        (triggerReason != BREATH_TRIGGER_REASON_FLOW) &&
        (triggerReason != BREATH_TRIGGER_REASON_APNEA_BACKUP)) {
        return PHASE_CONTROL_ERROR_PARAM;
    }
    if ((gPhaseController.planValid == 0U) ||
        (gPhaseController.runState != PHASE_EXP) ||
        (gPhaseController.expirationCaptureComplete == 0U)) {
        return PHASE_CONTROL_ERROR_STATE;
    }
    if (((triggerReason == BREATH_TRIGGER_REASON_PRESSURE) &&
         (gPhaseController.activePlan.allowedTriggerType != VENT_TRIGGER_PRESSURE)) ||
        ((triggerReason == BREATH_TRIGGER_REASON_FLOW) &&
         (gPhaseController.activePlan.allowedTriggerType != VENT_TRIGGER_FLOW))) {
        return PHASE_CONTROL_ERROR_STATE;
    }
    lExpirationElapsedMs = nowMs - gPhaseController.expirationStartedMs;
    if (lExpirationElapsedMs < gPhaseController.activePlan.minimumExpiratoryTimeMs) {
        return PHASE_CONTROL_ERROR_STATE;
    }
    return phaseControllerInspirationStart(triggerReason, nowMs);
}

/** Begin expiration and preserve the reason that ended inspiration. */
static void phaseControllerExpirationStart(eBreathCycleReason cycleReason,
                                           uint32_t nowMs)
{
    float lPeakPressure = gPhaseController.activePlan.inspiratoryPressureCmh2o;

    if (lPeakPressure < gPhaseController.activePlan.peepCmh2o) {
        lPeakPressure = gPhaseController.activePlan.peepCmh2o;
    }
    gPhaseController.cycleReason = cycleReason;
    gPhaseController.expirationStartedMs = nowMs;
    gPhaseController.expirationCaptureComplete = 0U;
    (void)phaseControlSet(PHASE_REF_PRESSURE, lPeakPressure);
    gPhaseController.runState = PHASE_EXP;
}

int8_t phaseControllerExpirationCaptureNotify(void) {
    if ((gPhaseController.planValid == 0U) ||
        (gPhaseController.runState != PHASE_EXP)) {
        return PHASE_CONTROL_ERROR_STATE;
    }
    gPhaseController.expirationCaptureComplete = 1U;
    return PHASE_CONTROL_SUCCESS;
}

uint8_t phaseControllerExpirationReadyGet(void)
{
    return gPhaseController.expirationCaptureComplete;
}

int8_t phaseControllerCycle(eBreathCycleReason cycleReason, uint32_t nowMs)
{
    uint32_t lInspiratoryElapsedMs;

    if ((cycleReason != BREATH_CYCLE_REASON_FLOW) &&
        (cycleReason != BREATH_CYCLE_REASON_MAX_INSPIRATORY_TIME)) {
        return PHASE_CONTROL_ERROR_PARAM;
    }
    if ((gPhaseController.planValid == 0U) ||
        (gPhaseController.activePlan.cycleType != BREATH_CYCLE_TYPE_FLOW) ||
        (gPhaseController.activePlan.breathType !=
         BREATH_TYPE_SPONTANEOUS_PRESSURE_SUPPORT) ||
        (gPhaseController.runState != PHASE_INSP)) {
        return PHASE_CONTROL_ERROR_STATE;
    }
    lInspiratoryElapsedMs = nowMs - gPhaseController.inspirationStartedMs;
    if ((cycleReason == BREATH_CYCLE_REASON_FLOW) &&
        (lInspiratoryElapsedMs <
         gPhaseController.activePlan.minimumInspiratoryTimeMs)) {
        return PHASE_CONTROL_ERROR_STATE;
    }
    phaseControllerExpirationStart(cycleReason, nowMs);
    return PHASE_CONTROL_SUCCESS;
}

eBreathCycleReason phaseControllerCycleReasonGet(void)
{
    return gPhaseController.cycleReason;
}

/** Process the flow-controlled delivery interval. */
static void phaseControllerVolumeInspirationProcess(uint32_t nowMs)
{
    if ((nowMs - gPhaseController.inspirationStartedMs) <
        gPhaseController.activePlan.riseTimeMs) {
        (void)phaseControlSet(PHASE_REF_FLOW,
                              gPhaseController.activePlan.inspiratoryFlowLpm);
    } else {
        (void)phaseControlSet(PHASE_REF_FLOW, 0.0F);
    }
}

/** Process the pressure reference fall from peak pressure to PEEP. */
static void phaseControllerPressureFallProcess(uint32_t expirationElapsedMs)
{
    float lCurveRemaining;
    float lDeltaPressure = gPhaseController.activePlan.inspiratoryPressureCmh2o -
                           gPhaseController.activePlan.peepCmh2o;
    float lRemaining;
    float lTimeProgress;

    if ((gPhaseController.breathStarted == 0U) ||
        (lDeltaPressure <= 0.0F) ||
        (expirationElapsedMs >= PHASE_PRESSURE_FALL_TIME_MS)) {
        (void)phaseControlSet(PHASE_REF_PRESSURE,
                              gPhaseController.activePlan.peepCmh2o);
        return;
    }

    lTimeProgress = (float)expirationElapsedMs /
                    (float)PHASE_PRESSURE_FALL_TIME_MS;
    lRemaining = 1.0F - lTimeProgress;
    /* Lightweight discharge curve: about 39% at T/3 and 21% at T/2. */
    lCurveRemaining = (lRemaining * lRemaining) *
                      (0.65F + (0.35F * lRemaining));
    (void)phaseControlSet(PHASE_REF_PRESSURE,
                          gPhaseController.activePlan.peepCmh2o +
                          (lDeltaPressure * lCurveRemaining));
}

void phaseControllerProcess(uint32_t nowMs)
{
    uint32_t lExpirationElapsedMs;

    if (breathSchedulerRunningGet() == 0U) {
        phaseControllerIdleEnter();
        return;
    }

    switch (gPhaseController.runState) {
        case PHASE_IDLE:
            if (phaseControllerInitialExpirationStart(nowMs) != PHASE_CONTROL_SUCCESS) {
                phaseControllerIdleEnter();
            }
            break;

        case PHASE_INSP:
            if (gPhaseController.activePlan.breathType == BREATH_TYPE_MANDATORY_VOLUME) {
                phaseControllerVolumeInspirationProcess(nowMs);
            }
            if ((gPhaseController.activePlan.cycleType == BREATH_CYCLE_TYPE_TIME) &&
                ((nowMs - gPhaseController.inspirationStartedMs) >=
                 gPhaseController.activePlan.maximumInspiratoryTimeMs)) {
                phaseControllerExpirationStart(BREATH_CYCLE_REASON_TIME, nowMs);
            }
            break;

        case PHASE_EXP:
            lExpirationElapsedMs = nowMs - gPhaseController.expirationStartedMs;
            phaseControllerPressureFallProcess(lExpirationElapsedMs);
            (void)phaseControlSet(PHASE_REF_FLOW, 0.0F);
            if ((gPhaseController.activePlan.timeTriggerEnabled != 0U) &&
                (lExpirationElapsedMs >= gPhaseController.activePlan.expiratoryTimeMs)) {
                if (phaseControllerInspirationStart(BREATH_TRIGGER_REASON_TIME, nowMs) !=
                    PHASE_CONTROL_SUCCESS) {
                    phaseControllerIdleEnter();
                }
            }
            break;

        default:
            phaseControllerIdleEnter();
            break;
    }
}

/**************************End of file********************************/
