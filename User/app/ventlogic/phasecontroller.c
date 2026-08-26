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
    (void)phaseControlSet(PHASE_REF_FAST_PRESSURE, 0.0F);
    (void)phaseControlSet(PHASE_REF_FLOW, 0.0F);
}

/** Enter idle and discard the active breath plan. */
static void phaseControllerIdleEnter(void)
{
    gPhaseController.runState = PHASE_IDLE;
    gPhaseController.stateStartedMs = 0U;
    gPhaseController.expirationStartedMs = 0U;
    gPhaseController.inspRiseStartPressure = 0.0F;
    gPhaseController.planValid = 0U;
    gPhaseController.breathStarted = 0U;
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
    gPhaseController.stateStartedMs = nowMs;
    gPhaseController.runState = PHASE_EXP_RELEASE;
    return PHASE_CONTROL_SUCCESS;
}

/** Start inspiration using the next plan selected for the trigger reason. */
static int8_t phaseControllerInspirationStart(eBreathTriggerReason triggerReason,
                                              uint32_t nowMs)
{
    float lPatientPressure;

    if (gPhaseController.breathStarted != 0U) {
        if (phaseControllerPlanLoad(triggerReason) != PHASE_CONTROL_SUCCESS) {
            return PHASE_CONTROL_ERROR_STATE;
        }
    } else {
        gPhaseController.activePlan.triggerReason = triggerReason;
    }

    lPatientPressure = controlDataGet(PAT_REAL_PRS);
    gPhaseController.inspRiseStartPressure = lPatientPressure;
    gPhaseController.stateStartedMs = nowMs;
    gPhaseController.breathStarted = 1U;
    (void)phaseControlSet(PHASE_REF_PRESSURE, lPatientPressure);
    (void)phaseControlSet(PHASE_REF_FAST_PRESSURE, lPatientPressure);
    if (gPhaseController.activePlan.breathType == BREATH_TYPE_MANDATORY_VOLUME) {
        (void)phaseControlSet(PHASE_REF_FLOW,
                              gPhaseController.activePlan.inspiratoryFlowLpm);
    } else {
        (void)phaseControlSet(PHASE_REF_FLOW, 0.0F);
    }
    gPhaseController.runState = PHASE_INSP_RISE;
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
        (triggerReason != BREATH_TRIGGER_REASON_FLOW)) {
        return PHASE_CONTROL_ERROR_PARAM;
    }
    if ((gPhaseController.planValid == 0U) ||
        (gPhaseController.runState != PHASE_EXP_PEEP)) {
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

/** Process the pressure-controlled inspiratory rise reference. */
static void phaseControllerPressureRiseProcess(uint32_t nowMs)
{
    float lCurveProgress;
    float lElapsedTime = (float)(nowMs - gPhaseController.stateStartedMs);
    float lFastProgress;
    float lPressureRange;
    float lReferencePressure;
    float lRemaining;
    float lTargetPressure = gPhaseController.activePlan.inspiratoryPressureCmh2o;
    float lTimeProgress;

    if ((gPhaseController.activePlan.riseTimeMs == 0U) ||
        ((uint32_t)lElapsedTime >= gPhaseController.activePlan.riseTimeMs)) {
        (void)phaseControlSet(PHASE_REF_PRESSURE, lTargetPressure);
        (void)phaseControlSet(PHASE_REF_FAST_PRESSURE, lTargetPressure);
        gPhaseController.stateStartedMs = nowMs;
        gPhaseController.runState = PHASE_INSP_HOLD;
        return;
    }

    lTimeProgress = lElapsedTime / (float)gPhaseController.activePlan.riseTimeMs;
    lRemaining = 1.0F - lTimeProgress;
    /* Lightweight charging curve: about 61% at T/3 and 79% at T/2. */
    lCurveProgress = 1.0F - ((lRemaining * lRemaining) *
                            (0.65F + (0.35F * lRemaining)));
    /* Reach 80% at T/3, then rise slowly to the target. */
    if (lTimeProgress <= (1.0F / 3.0F)) {
        lFastProgress = 2.4F * lTimeProgress;
    } else {
        lFastProgress = 0.7F + (0.3F * lTimeProgress);
    }
    lPressureRange = lTargetPressure - gPhaseController.inspRiseStartPressure;
    lReferencePressure = gPhaseController.inspRiseStartPressure +
                         (lPressureRange * lCurveProgress);
    (void)phaseControlSet(PHASE_REF_PRESSURE, lReferencePressure);
    (void)phaseControlSet(PHASE_REF_FAST_PRESSURE,
                          gPhaseController.inspRiseStartPressure +
                          (lPressureRange * lFastProgress));
}

/** Process the flow-controlled delivery interval. */
static void phaseControllerVolumeRiseProcess(uint32_t nowMs)
{
    (void)phaseControlSet(PHASE_REF_FLOW,
                          gPhaseController.activePlan.inspiratoryFlowLpm);
    if ((nowMs - gPhaseController.stateStartedMs) >=
        gPhaseController.activePlan.riseTimeMs) {
        gPhaseController.stateStartedMs = nowMs;
        gPhaseController.runState = PHASE_INSP_HOLD;
    }
}

void phaseControllerProcess(uint32_t nowMs)
{
    uint32_t lExpirationElapsedMs;
    float lPeepPressure;

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

        case PHASE_INSP_RISE:
            if (gPhaseController.activePlan.breathType == BREATH_TYPE_MANDATORY_VOLUME) {
                phaseControllerVolumeRiseProcess(nowMs);
            } else {
                phaseControllerPressureRiseProcess(nowMs);
            }
            break;

        case PHASE_INSP_HOLD:
            if (gPhaseController.activePlan.breathType == BREATH_TYPE_MANDATORY_VOLUME) {
                (void)phaseControlSet(PHASE_REF_FLOW, 0.0F);
            } else {
                (void)phaseControlSet(PHASE_REF_PRESSURE,
                                      gPhaseController.activePlan.inspiratoryPressureCmh2o);
                (void)phaseControlSet(PHASE_REF_FAST_PRESSURE,
                                      gPhaseController.activePlan.inspiratoryPressureCmh2o);
            }
            if ((nowMs - gPhaseController.stateStartedMs) >=
                gPhaseController.activePlan.holdTimeMs) {
                gPhaseController.expirationStartedMs = nowMs;
                gPhaseController.stateStartedMs = nowMs;
                gPhaseController.runState = PHASE_EXP_RELEASE;
            }
            break;

        case PHASE_EXP_RELEASE:
            lPeepPressure = gPhaseController.activePlan.peepCmh2o;
            (void)phaseControlSet(PHASE_REF_PRESSURE, lPeepPressure);
            (void)phaseControlSet(PHASE_REF_FAST_PRESSURE, lPeepPressure);
            (void)phaseControlSet(PHASE_REF_FLOW, 0.0F);
            if ((controlDataGet(PAT_REAL_PRS) <=
                 (lPeepPressure + PHASE_EXP_PEEP_ENTRY_MARGIN)) ||
                ((nowMs - gPhaseController.stateStartedMs) >=
                 PHASE_EXP_RELEASE_MAX_TIME_MS)) {
                gPhaseController.stateStartedMs = nowMs;
                gPhaseController.runState = PHASE_EXP_PEEP;
            }
            break;

        case PHASE_EXP_PEEP:
            (void)phaseControlSet(PHASE_REF_PRESSURE,
                                  gPhaseController.activePlan.peepCmh2o);
            (void)phaseControlSet(PHASE_REF_FAST_PRESSURE,
                                  gPhaseController.activePlan.peepCmh2o);
            (void)phaseControlSet(PHASE_REF_FLOW, 0.0F);
            lExpirationElapsedMs = nowMs - gPhaseController.expirationStartedMs;
            if (lExpirationElapsedMs >= gPhaseController.activePlan.expiratoryTimeMs) {
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
