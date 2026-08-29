/************************************************************************************
* @file     : expirationcontroller.c
* @brief    : Shared expiration controller.
* @details  : Produces release and PEEP requests for every normal breath type.
* @author   :
* @date     : 2026-08-26
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "expirationcontroller.h"

#include <stddef.h>

#include "calibtrans.h"
#include "controldata.h"
#include "pid.h"

static stPid gExpirationPeepPid;
static uint8_t gExpirationControllerReady;
static eExpirationControllerState gExpirationControllerState;
static uint16_t gExpirationBlowerTarget;
static uint8_t gExpirationValveDuty;
static uint16_t gExpirationPeepEntryElapsedMs;
static uint8_t gExpirationPeepTrackPending;
static uint32_t gExpirationPlanSequence;
static float gExpirationReleaseStartPressure;

/** Clamp an expiration-controller value to a configured range. */
static float expirationControllerClamp(float value, float minimum, float maximum)
{
    if (value > maximum) {
        return maximum;
    }
    if (value < minimum) {
        return minimum;
    }
    return value;
}

/** Move an actuator command toward its target by a bounded amount. */
static float expirationControllerMoveTowards(float current, float target, float maximumStep)
{
    if (target > (current + maximumStep)) {
        return current + maximumStep;
    }
    if (target < (current - maximumStep)) {
        return current - maximumStep;
    }
    return target;
}

/** Prepare a complete but invalid expiration request. */
static void expirationControllerRequestClear(stActuatorRequest *request)
{
    request->blowerTarget = 0U;
    request->expiratoryValveDuty = EXPIRATION_CONTROLLER_EXP_VALVE_OPEN_DUTY;
    request->oxygenValveDuty = 0U;
    request->reliefValveDuty = EXPIRATION_CONTROLLER_RELIEF_CLOSED_DUTY;
    request->validMask = 0U;
}

void expirationControllerInit(void)
{
    gExpirationControllerReady = (uint8_t)(pidInit(&gExpirationPeepPid,
                                                    EXPIRATION_CONTROLLER_PEEP_KP,
                                                    EXPIRATION_CONTROLLER_PEEP_KI,
                                                    EXPIRATION_CONTROLLER_PEEP_KD,
                                                    EXPIRATION_CONTROLLER_SAMPLE_PERIOD_S,
                                                    EXPIRATION_CONTROLLER_PEEP_EFFORT_MIN,
                                                    EXPIRATION_CONTROLLER_PEEP_EFFORT_MAX) ==
                                            PID_STATUS_OK);
    gExpirationControllerState = EXPIRATION_CONTROLLER_IDLE;
    gExpirationBlowerTarget = 0U;
    gExpirationValveDuty = EXPIRATION_CONTROLLER_EXP_VALVE_OPEN_DUTY;
    gExpirationPeepEntryElapsedMs = 0U;
    gExpirationPeepTrackPending = 0U;
    gExpirationPlanSequence = 0U;
    gExpirationReleaseStartPressure = 0.0F;
}

/** Initialize state on an expiration phase transition. */
static void expirationControllerStateEnter(eExpirationControllerState state,
                                           const stActuatorRequest *previousRequest)
{
    eExpirationControllerState lPreviousState = gExpirationControllerState;

    if (gExpirationControllerState == state) {
        return;
    }
    gExpirationControllerState = state;
    if (state == EXPIRATION_CONTROLLER_RELEASE) {
        (void)pidReset(&gExpirationPeepPid);
        gExpirationReleaseStartPressure = controlDataGet(PAT_REAL_PRS);
        if ((previousRequest != NULL) &&
            ((previousRequest->validMask & ACTUATOR_REQUEST_VALID_BREATH_OUTPUTS) ==
             ACTUATOR_REQUEST_VALID_BREATH_OUTPUTS)) {
            gExpirationBlowerTarget = previousRequest->blowerTarget;
            gExpirationValveDuty = previousRequest->expiratoryValveDuty;
        } else {
            gExpirationBlowerTarget = 0U;
            gExpirationValveDuty = EXPIRATION_CONTROLLER_EXP_VALVE_OPEN_DUTY;
        }
    } else if (state == EXPIRATION_CONTROLLER_PEEP) {
        (void)pidReset(&gExpirationPeepPid);
        gExpirationPeepEntryElapsedMs = 0U;
        gExpirationPeepTrackPending =
            (uint8_t)((lPreviousState == EXPIRATION_CONTROLLER_RELEASE) ||
                      (lPreviousState == EXPIRATION_CONTROLLER_CAPTURE));
    } else {
        (void)pidReset(&gExpirationPeepPid);
        gExpirationBlowerTarget = 0U;
        gExpirationValveDuty = EXPIRATION_CONTROLLER_EXP_VALVE_OPEN_DUTY;
        gExpirationPeepTrackPending = 0U;
    }
}

/** Produce the capture request while approaching PEEP. */
static int8_t expirationControllerCaptureProcess(const stBreathPlan *plan,
                                                  stActuatorRequest *request)
{
    (void)plan;

    gExpirationBlowerTarget = 0U;
    gExpirationValveDuty = EXPIRATION_CONTROLLER_EXP_VALVE_OPEN_DUTY;
    request->blowerTarget = gExpirationBlowerTarget;
    request->expiratoryValveDuty = gExpirationValveDuty;
    request->validMask = ACTUATOR_REQUEST_VALID_BREATH_OUTPUTS;
    return ACTUATOR_REQUEST_SUCCESS;
}

/** Produce the fast-release request while approaching PEEP. */
static int8_t expirationControllerReleaseProcess(const stBreathPlan *plan, stActuatorRequest *request)
{
    float lPatientPressure = controlDataGet(PAT_REAL_PRS);
    float lPressureError = lPatientPressure - plan->peepCmh2o;
    float lPeepSoftMargin;
    float lSoftMarginMaximum;
    float lSoftMarginRatio;
    float lSoftMargin;
    float lInspiratoryPressure = plan->inspiratoryPressureCmh2o;

    if (gExpirationReleaseStartPressure > lInspiratoryPressure) {
        lInspiratoryPressure = gExpirationReleaseStartPressure;
    }
    if (lInspiratoryPressure < plan->peepCmh2o) {
        lInspiratoryPressure = plan->peepCmh2o;
    }

    lSoftMarginRatio = expirationControllerClamp(
        plan->peepCmh2o * EXPIRATION_CONTROLLER_RELEASE_SOFT_MARGIN_RATIO_PEEP_SCALE,
        EXPIRATION_CONTROLLER_RELEASE_SOFT_MARGIN_RATIO_MIN,
        1.0F);
    lSoftMargin = (lInspiratoryPressure - plan->peepCmh2o) *lSoftMarginRatio;
    lPeepSoftMargin = plan->peepCmh2o * EXPIRATION_CONTROLLER_RELEASE_PEEP_MARGIN_RATIO;
    if (lSoftMargin < lPeepSoftMargin) {
        lSoftMargin = lPeepSoftMargin;
    }
    lSoftMarginMaximum = expirationControllerClamp(
        plan->peepCmh2o * EXPIRATION_CONTROLLER_RELEASE_SOFT_MARGIN_MAX_PEEP_RATIO,
        EXPIRATION_CONTROLLER_RELEASE_SOFT_MARGIN_MAX_MIN,
        EXPIRATION_CONTROLLER_RELEASE_SOFT_MARGIN_MAX_LIMIT);
    lSoftMargin = expirationControllerClamp(lSoftMargin,
                                             EXPIRATION_CONTROLLER_RELEASE_SOFT_MARGIN_MIN,
                                             lSoftMarginMaximum);
    if (lPressureError >= lSoftMargin) {
        gExpirationBlowerTarget = 0U;
        gExpirationValveDuty = EXPIRATION_CONTROLLER_EXP_VALVE_OPEN_DUTY;
        request->blowerTarget = gExpirationBlowerTarget;
        request->expiratoryValveDuty = gExpirationValveDuty;
        request->validMask = ACTUATOR_REQUEST_VALID_BREATH_OUTPUTS;
        return ACTUATOR_REQUEST_SUCCESS;
    }

    expirationControllerStateEnter(EXPIRATION_CONTROLLER_CAPTURE, NULL);
    return expirationControllerCaptureProcess(plan, request);
}

/** Produce the closed-loop PEEP request. */
static int8_t expirationControllerPeepProcess(const stBreathPlan *plan,
                                              stActuatorRequest *request)
{
    float lBlowerFeedforward;
    float lBlowerTarget;
    float lDesiredEffort;
    float lEffort;
    float lEntryProgress;
    float lEntryValveDuty;
    float lExcessPressure;
    float lValveDuty;
    float lValveDutyMaximum;
    float lValveOpening;
    float lPatientPressure;

    if (calibtransPrsSpeed(plan->peepCmh2o, &lBlowerFeedforward) !=
        CALIBTRANS_STATUS_OK) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }
    lBlowerFeedforward = lBlowerFeedforward*0.8;
    lPatientPressure = controlDataGet(PAT_REAL_PRS);
    if (gExpirationPeepTrackPending != 0U) {
        lDesiredEffort = ((float)gExpirationBlowerTarget -
                          (lBlowerFeedforward * 10.0F)) /
                         EXPIRATION_CONTROLLER_BLOWER_CORRECTION_SCALE;
        if (pidTrackOutput(&gExpirationPeepPid,
                           plan->peepCmh2o,
                           lPatientPressure,
                           lDesiredEffort) != PID_STATUS_OK) {
            return ACTUATOR_REQUEST_ERROR_STATE;
        }
        gExpirationPeepTrackPending = 0U;
    }
    if (pidUpdate(&gExpirationPeepPid,
                  plan->peepCmh2o,
                  lPatientPressure,
                  &lEffort) != PID_STATUS_OK) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }

    lBlowerTarget = (lBlowerFeedforward * 10.0F) +
                    (lEffort * EXPIRATION_CONTROLLER_BLOWER_CORRECTION_SCALE);
    lBlowerTarget = expirationControllerClamp(
        lBlowerTarget,
        0.0F,
        (float)EXPIRATION_CONTROLLER_BLOWER_TARGET_MAX);
    lExcessPressure = lPatientPressure - plan->peepCmh2o;
    lValveOpening = (lExcessPressure - EXPIRATION_CONTROLLER_PEEP_RELIEF_DEADBAND) *
                    EXPIRATION_CONTROLLER_PEEP_RELIEF_GAIN;
    lValveOpening = expirationControllerClamp(
        lValveOpening,
        0.0F,
        EXPIRATION_CONTROLLER_PEEP_RELIEF_MAX_OPENING);
    lValveDuty = (float)EXPIRATION_CONTROLLER_EXP_VALVE_CLOSED_DUTY - lValveOpening;
    lEntryProgress = expirationControllerClamp(
        (float)gExpirationPeepEntryElapsedMs /
        EXPIRATION_CONTROLLER_PEEP_ENTRY_RAMP_TIME_MS,
        0.0F,
        1.0F);
    lEntryValveDuty = expirationControllerClamp(
        EXPIRATION_CONTROLLER_PEEP_ENTRY_VALVE_BASE_DUTY + plan->peepCmh2o,
        EXPIRATION_CONTROLLER_PEEP_ENTRY_VALVE_DUTY_MIN,
        EXPIRATION_CONTROLLER_PEEP_ENTRY_VALVE_DUTY_MAX);
    lValveDutyMaximum = lEntryValveDuty +
                        (((float)EXPIRATION_CONTROLLER_EXP_VALVE_CLOSED_DUTY -
                          lEntryValveDuty) * lEntryProgress);
    lValveDuty = expirationControllerClamp(lValveDuty, 0.0F, lValveDutyMaximum);
    gExpirationBlowerTarget = (uint16_t)expirationControllerMoveTowards(
        (float)gExpirationBlowerTarget,
        lBlowerTarget,
        EXPIRATION_CONTROLLER_BLOWER_MAX_STEP);
    if (lValveDuty < (float)gExpirationValveDuty) {
        gExpirationValveDuty = (uint8_t)lValveDuty;
    } else {
        gExpirationValveDuty = (uint8_t)expirationControllerMoveTowards(
            (float)gExpirationValveDuty,
            lValveDuty,
            EXPIRATION_CONTROLLER_EXP_VALVE_CLOSE_MAX_STEP);
    }
    if (gExpirationPeepEntryElapsedMs <
        (uint16_t)EXPIRATION_CONTROLLER_PEEP_ENTRY_RAMP_TIME_MS) {
        gExpirationPeepEntryElapsedMs +=
            (uint16_t)(EXPIRATION_CONTROLLER_SAMPLE_PERIOD_S * 1000.0F);
    }
    request->blowerTarget = gExpirationBlowerTarget;
    request->expiratoryValveDuty = gExpirationValveDuty;
    request->validMask = ACTUATOR_REQUEST_VALID_BREATH_OUTPUTS;
    return ACTUATOR_REQUEST_SUCCESS;
}

int8_t expirationControllerProcess(const stBreathPlan *plan,
                                   ePhaseControllerState phase,
                                   const stActuatorRequest *previousRequest,
                                   stActuatorRequest *request)
{
    eExpirationControllerState lState;

    expirationControllerRequestClear(request);
    if (gExpirationControllerReady == 0U) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }

    if (plan->sequence != gExpirationPlanSequence) {
        gExpirationPlanSequence = plan->sequence;
        gExpirationControllerState = EXPIRATION_CONTROLLER_IDLE;
    }
    if (phase == PHASE_EXP_RELEASE) {
        lState = (gExpirationControllerState == EXPIRATION_CONTROLLER_CAPTURE) ?
                 EXPIRATION_CONTROLLER_CAPTURE : EXPIRATION_CONTROLLER_RELEASE;
    } else if (phase == PHASE_EXP_PEEP) {
        lState = EXPIRATION_CONTROLLER_PEEP;
    } else {
        expirationControllerStateEnter(EXPIRATION_CONTROLLER_IDLE, previousRequest);
        return ACTUATOR_REQUEST_ERROR_STATE;
    }
    expirationControllerStateEnter(lState, previousRequest);
    switch (lState) {
        case EXPIRATION_CONTROLLER_RELEASE:
            return expirationControllerReleaseProcess(plan, request);
        case EXPIRATION_CONTROLLER_CAPTURE:
            return expirationControllerCaptureProcess(plan, request);
        case EXPIRATION_CONTROLLER_PEEP:
            return expirationControllerPeepProcess(plan, request);
        default:
            return ACTUATOR_REQUEST_ERROR_STATE;
    }
}

/**************************End of file********************************/
