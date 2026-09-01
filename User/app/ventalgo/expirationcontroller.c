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
#include "phasecontroller.h"
#include "pid.h"

static stPid gExpirationPeepPid;
static stPid gExpirationReleaseBlowerPid;
static uint8_t gExpirationControllerReady;
static eExpirationControllerState gExpirationControllerState;
static uint16_t gExpirationBlowerTarget;
static uint8_t gExpirationValveDuty;
static uint16_t gExpirationPeepEntryElapsedMs;
static uint8_t gExpirationPeepTrackPending;
static uint32_t gExpirationPlanSequence;
static float gExpirationPressureHistory[EXPIRATION_CONTROLLER_PRESSURE_HISTORY_COUNT];
static uint8_t gExpirationPressureHistoryIndex;
static uint16_t gExpirationCaptureElapsedMs;
static uint8_t gExpirationCaptureStableCount;
static uint8_t gExpirationPeepEntryValveStartDuty;
static uint16_t gExpirationReleaseStableElapsedMs;

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
    int8_t lPeepStatus;
    int8_t lReleaseBlowerStatus;

    lPeepStatus = pidInit(&gExpirationPeepPid,
                          EXPIRATION_CONTROLLER_PEEP_KP,
                          EXPIRATION_CONTROLLER_PEEP_KI,
                          EXPIRATION_CONTROLLER_PEEP_KD,
                          EXPIRATION_CONTROLLER_SAMPLE_PERIOD_S,
                          EXPIRATION_CONTROLLER_PEEP_EFFORT_MIN,
                          EXPIRATION_CONTROLLER_PEEP_EFFORT_MAX);
    lReleaseBlowerStatus = pidInit(
        &gExpirationReleaseBlowerPid,
        EXPIRATION_CONTROLLER_RELEASE_BLOWER_KP,
        EXPIRATION_CONTROLLER_RELEASE_BLOWER_KI,
        EXPIRATION_CONTROLLER_RELEASE_BLOWER_KD,
        EXPIRATION_CONTROLLER_SAMPLE_PERIOD_S,
        EXPIRATION_CONTROLLER_RELEASE_BLOWER_EFFORT_MIN,
        EXPIRATION_CONTROLLER_RELEASE_BLOWER_EFFORT_MAX);
    gExpirationControllerReady = (uint8_t)(
        (lPeepStatus == PID_STATUS_OK) &&
        (lReleaseBlowerStatus == PID_STATUS_OK));
    gExpirationControllerState = EXPIRATION_CONTROLLER_IDLE;
    gExpirationBlowerTarget = 0U;
    gExpirationValveDuty = EXPIRATION_CONTROLLER_EXP_VALVE_OPEN_DUTY;
    gExpirationPeepEntryElapsedMs = 0U;
    gExpirationPeepTrackPending = 0U;
    gExpirationPlanSequence = 0U;
    gExpirationPressureHistoryIndex = 0U;
    gExpirationCaptureElapsedMs = 0U;
    gExpirationCaptureStableCount = 0U;
    gExpirationPeepEntryValveStartDuty = EXPIRATION_CONTROLLER_EXP_VALVE_OPEN_DUTY;
    gExpirationReleaseStableElapsedMs = 0U;
}

eExpirationControllerState expirationControllerStateGet(void)
{
    return gExpirationControllerState;
}

/** Initialize the filtered patient-pressure history for release slope calculation. */
static void expirationControllerPressureHistoryInit(float pressure) {
    uint8_t lIndex;

    for (lIndex = 0U;
         lIndex < EXPIRATION_CONTROLLER_PRESSURE_HISTORY_COUNT;
         lIndex++) {
        gExpirationPressureHistory[lIndex] = pressure;
    }
    gExpirationPressureHistoryIndex = 0U;
}

/** Calculate patient-pressure slope from filtered samples 24 ms apart. */
static float expirationControllerPressureSlopeGet(float pressure) {
    float lDelayedPressure;
    uint8_t lDelayedIndex = gExpirationPressureHistoryIndex + 1U;

    if (lDelayedIndex >= EXPIRATION_CONTROLLER_PRESSURE_HISTORY_COUNT) {
        lDelayedIndex = 0U;
    }
    lDelayedPressure = gExpirationPressureHistory[lDelayedIndex];
    gExpirationPressureHistory[gExpirationPressureHistoryIndex] = pressure;
    gExpirationPressureHistoryIndex = lDelayedIndex;
    return (pressure - lDelayedPressure) /
           EXPIRATION_CONTROLLER_PRESSURE_SLOPE_INTERVAL_S;
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
        float lPatientPressure = controlDataGet(PAT_REAL_PRS);

        (void)pidReset(&gExpirationPeepPid);
        (void)pidReset(&gExpirationReleaseBlowerPid);
        expirationControllerPressureHistoryInit(lPatientPressure);
        gExpirationCaptureElapsedMs = 0U;
        gExpirationCaptureStableCount = 0U;
        gExpirationReleaseStableElapsedMs = 0U;
        if ((previousRequest != NULL) &&
            ((previousRequest->validMask & ACTUATOR_REQUEST_VALID_BREATH_OUTPUTS) ==
             ACTUATOR_REQUEST_VALID_BREATH_OUTPUTS)) {
            gExpirationBlowerTarget = previousRequest->blowerTarget;
            gExpirationValveDuty = previousRequest->expiratoryValveDuty;
        } else {
            gExpirationBlowerTarget = 0U;
            gExpirationValveDuty = EXPIRATION_CONTROLLER_EXP_VALVE_OPEN_DUTY;
        }
    } else if (state == EXPIRATION_CONTROLLER_CAPTURE) {
        (void)pidReset(&gExpirationPeepPid);
        expirationControllerPressureHistoryInit(controlDataGet(PAT_REAL_PRS));
        gExpirationCaptureElapsedMs = 0U;
        gExpirationCaptureStableCount = 0U;
        gExpirationPeepTrackPending = 0U;
    } else if (state == EXPIRATION_CONTROLLER_PEEP) {
        (void)pidReset(&gExpirationPeepPid);
        gExpirationPeepEntryElapsedMs = 0U;
        gExpirationPeepEntryValveStartDuty = gExpirationValveDuty;
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
    float lBlowerFeedforward;
    float lPatientPressure = controlDataGet(PAT_REAL_PRS);
    float lPressureSlope = expirationControllerPressureSlopeGet(lPatientPressure);
    float lPressureError = lPatientPressure +
                           (lPressureSlope *
                            EXPIRATION_CONTROLLER_CAPTURE_PREDICTION_TIME_S) -
                           plan->peepCmh2o;
    float lValveDuty;

    if (calibtransPrsSpeed(plan->peepCmh2o, &lBlowerFeedforward) !=
        CALIBTRANS_STATUS_OK) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }
    lBlowerFeedforward = expirationControllerClamp(
        lBlowerFeedforward,
        0.0F,
        (float)EXPIRATION_CONTROLLER_BLOWER_TARGET_MAX);
    gExpirationBlowerTarget = lBlowerFeedforward;
    /* Keep regulating during capture so a low entry duty cannot drain PEEP. */
    lValveDuty = EXPIRATION_CONTROLLER_CAPTURE_VALVE_BASE_OFFSET +
                 plan->peepCmh2o +
                 (-lPressureError * EXPIRATION_CONTROLLER_CAPTURE_VALVE_KP);
    lValveDuty = expirationControllerClamp(
        lValveDuty,
        EXPIRATION_CONTROLLER_RELEASE_VALVE_DUTY_MIN,
        (float)EXPIRATION_CONTROLLER_EXP_VALVE_CLOSED_DUTY);
    if (lValveDuty > (float)gExpirationValveDuty) {
        gExpirationValveDuty = (uint8_t)expirationControllerMoveTowards(
            (float)gExpirationValveDuty,
            lValveDuty,
            EXPIRATION_CONTROLLER_CAPTURE_VALVE_MAX_STEP);
    } else {
        gExpirationValveDuty = (uint8_t)expirationControllerMoveTowards(
            (float)gExpirationValveDuty,
            lValveDuty,
            EXPIRATION_CONTROLLER_CAPTURE_VALVE_MAX_STEP);
    }
    /* Confirm both pressure proximity and low motion before handing off to PEEP. */
    if (gExpirationCaptureElapsedMs < EXPIRATION_CONTROLLER_CAPTURE_RAMP_TIME_MS) {
        gExpirationCaptureElapsedMs +=
            (uint16_t)(EXPIRATION_CONTROLLER_SAMPLE_PERIOD_S * 1000.0F);
    } else if ((lPressureError >=
                (-EXPIRATION_CONTROLLER_CAPTURE_PRESSURE_TOLERANCE)) &&
               (lPressureError <=
                EXPIRATION_CONTROLLER_CAPTURE_PRESSURE_TOLERANCE) &&
               (lPressureSlope >=
                (-EXPIRATION_CONTROLLER_CAPTURE_STABLE_SLOPE_MAX)) &&
               (lPressureSlope <=
                EXPIRATION_CONTROLLER_CAPTURE_STABLE_SLOPE_MAX)) {
        if (gExpirationCaptureStableCount <
            EXPIRATION_CONTROLLER_CAPTURE_STABLE_SAMPLE_COUNT) {
            gExpirationCaptureStableCount++;
        }
        if (gExpirationCaptureStableCount >=
            EXPIRATION_CONTROLLER_CAPTURE_STABLE_SAMPLE_COUNT) {
            if (phaseControllerExpirationCaptureNotify() != PHASE_CONTROL_SUCCESS) {
                return ACTUATOR_REQUEST_ERROR_STATE;
            }
            expirationControllerStateEnter(EXPIRATION_CONTROLLER_PEEP, NULL);
        }
    } else {
        gExpirationCaptureStableCount = 0U;
    }
    request->blowerTarget = gExpirationBlowerTarget;
    request->expiratoryValveDuty = gExpirationValveDuty;
    request->validMask = ACTUATOR_REQUEST_VALID_BREATH_OUTPUTS;
    return ACTUATOR_REQUEST_SUCCESS;
}

/** Produce the fast-release request while approaching PEEP. */
static int8_t expirationControllerReleaseProcess(const stBreathPlan *plan, stActuatorRequest *request)
{
    float lBlowerEffort;
    float lBlowerFeedforward;
    float lBlowerTarget;
    float lInspPressure = controlDataGet(INSP_REAL_PRS);
    float lPatientPressure = controlDataGet(PAT_REAL_PRS);
    float lPatientReference = phaseControlGet(PHASE_REF_PRESSURE);
    float lPressureSlope = expirationControllerPressureSlopeGet(lPatientPressure);
    float lPressureError = lPatientPressure +
                           (lPressureSlope *
                            EXPIRATION_CONTROLLER_RELEASE_PREDICTION_TIME_S) -
                           lPatientReference;
    float lPeepTolerance = plan->peepCmh2o *
                           EXPIRATION_CONTROLLER_RELEASE_CAPTURE_TOLERANCE_RATIO;
    float lValveDuty;

    if (lPeepTolerance < 0.0F) {
        lPeepTolerance = -lPeepTolerance;
    }
    if ((calibtransPrsSpeed(lPatientReference, &lBlowerFeedforward) !=
         CALIBTRANS_STATUS_OK) ||
        (pidUpdate(&gExpirationReleaseBlowerPid,
                   lPatientReference,
                   lInspPressure,
                   &lBlowerEffort) != PID_STATUS_OK)) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }

    lBlowerTarget = lBlowerFeedforward +
                    (lBlowerEffort *
                     EXPIRATION_CONTROLLER_RELEASE_BLOWER_EFFORT_SCALE);
    /* Predicted patient pressure brakes the release before pneumatic lag overshoots. */
    lValveDuty = EXPIRATION_CONTROLLER_RELEASE_VALVE_BASE_OFFSET +
                 plan->peepCmh2o +
                 (-lPressureError * EXPIRATION_CONTROLLER_RELEASE_VALVE_KP);
    gExpirationBlowerTarget = (uint16_t)expirationControllerClamp(
        lBlowerTarget,
        0.0F,
        (float)EXPIRATION_CONTROLLER_BLOWER_TARGET_MAX);
    lValveDuty = expirationControllerClamp(
        lValveDuty,
        EXPIRATION_CONTROLLER_RELEASE_VALVE_DUTY_MIN,
        (float)EXPIRATION_CONTROLLER_EXP_VALVE_CLOSED_DUTY);
    gExpirationValveDuty = (uint8_t)expirationControllerMoveTowards(
        (float)gExpirationValveDuty,
        lValveDuty,
        EXPIRATION_CONTROLLER_RELEASE_VALVE_MAX_STEP);

    if ((lPatientPressure >= (plan->peepCmh2o - lPeepTolerance)) &&
        (lPatientPressure <= (plan->peepCmh2o + lPeepTolerance))) {
        if (gExpirationReleaseStableElapsedMs <
            EXPIRATION_CONTROLLER_RELEASE_CAPTURE_STABLE_MS) {
            gExpirationReleaseStableElapsedMs +=
                (uint16_t)(EXPIRATION_CONTROLLER_SAMPLE_PERIOD_S * 1000.0F);
        }
    } else {
        gExpirationReleaseStableElapsedMs = 0U;
    }
    if (gExpirationReleaseStableElapsedMs >=
        EXPIRATION_CONTROLLER_RELEASE_CAPTURE_STABLE_MS) {
        expirationControllerStateEnter(EXPIRATION_CONTROLLER_CAPTURE, NULL);
        return expirationControllerCaptureProcess(plan, request);
    }

    request->blowerTarget = gExpirationBlowerTarget;
    request->expiratoryValveDuty = gExpirationValveDuty;
    request->validMask = ACTUATOR_REQUEST_VALID_BREATH_OUTPUTS;
    return ACTUATOR_REQUEST_SUCCESS;
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
    float lExcessPressure;
    float lValveClosing;
    float lValveDuty;
    float lValveDutyBase;
    float lValveDutyMaximum;
    float lValveOpening;
    float lPatientPressure;

    if (calibtransPrsSpeed(plan->peepCmh2o, &lBlowerFeedforward) !=
        CALIBTRANS_STATUS_OK) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }
    lPatientPressure = controlDataGet(PAT_REAL_PRS);
    if (gExpirationPeepTrackPending != 0U) {
        lDesiredEffort = ((float)gExpirationBlowerTarget -
                          lBlowerFeedforward) /
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

    lBlowerTarget = lBlowerFeedforward +
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
    lValveClosing = ((-lExcessPressure) -
                     EXPIRATION_CONTROLLER_PEEP_SUPPORT_DEADBAND) *
                    EXPIRATION_CONTROLLER_PEEP_SUPPORT_GAIN;
    lValveClosing = expirationControllerClamp(
        lValveClosing,
        0.0F,
        EXPIRATION_CONTROLLER_PEEP_SUPPORT_MAX_CLOSING);
    lValveDutyBase = expirationControllerClamp(
        EXPIRATION_CONTROLLER_PEEP_VALVE_BASE_OFFSET +
        (plan->peepCmh2o * EXPIRATION_CONTROLLER_PEEP_VALVE_PRESSURE_GAIN),
        0.0F,
        (float)EXPIRATION_CONTROLLER_EXP_VALVE_CLOSED_DUTY);
    lValveDuty = expirationControllerClamp(
        lValveDutyBase + lValveClosing - lValveOpening,
        0.0F,
        (float)EXPIRATION_CONTROLLER_EXP_VALVE_CLOSED_DUTY);
    lEntryProgress = expirationControllerClamp(
        (float)gExpirationPeepEntryElapsedMs /
        EXPIRATION_CONTROLLER_PEEP_ENTRY_RAMP_TIME_MS,
        0.0F,
        1.0F);
    lValveDutyMaximum = expirationControllerClamp(
        (float)gExpirationPeepEntryValveStartDuty +
        ((lValveDutyBase - (float)gExpirationPeepEntryValveStartDuty) *
         lEntryProgress) +
        lValveClosing,
        0.0F,
        (float)EXPIRATION_CONTROLLER_EXP_VALVE_CLOSED_DUTY);
    lValveDuty = expirationControllerClamp(lValveDuty, 0.0F, lValveDutyMaximum);
    gExpirationBlowerTarget = (uint16_t)expirationControllerMoveTowards(
        (float)gExpirationBlowerTarget,
        lBlowerTarget,
        EXPIRATION_CONTROLLER_BLOWER_MAX_STEP);
    gExpirationValveDuty = (uint8_t)expirationControllerMoveTowards(
        (float)gExpirationValveDuty,
        lValveDuty,
        EXPIRATION_CONTROLLER_PEEP_VALVE_MAX_STEP);
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
                                   const stActuatorRequest *previousRequest,
                                   stActuatorRequest *request)
{
    if ((plan == NULL) || (request == NULL)) {
        return ACTUATOR_REQUEST_ERROR_PARAM;
    }
    expirationControllerRequestClear(request);
    if (gExpirationControllerReady == 0U) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }

    if (plan->sequence != gExpirationPlanSequence) {
        gExpirationPlanSequence = plan->sequence;
        gExpirationControllerState = EXPIRATION_CONTROLLER_IDLE;
    }
    if (gExpirationControllerState == EXPIRATION_CONTROLLER_IDLE) {
        expirationControllerStateEnter(EXPIRATION_CONTROLLER_RELEASE,
                                       previousRequest);
    }
    switch (gExpirationControllerState) {
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
