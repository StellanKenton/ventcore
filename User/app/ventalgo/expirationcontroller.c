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
static stPid gExpirationCapturePid;
static uint8_t gExpirationControllerReady;
static eExpirationControllerState gExpirationControllerState;
static uint16_t gExpirationBlowerTarget;
static uint8_t gExpirationValveDuty;
static uint16_t gExpirationPeepEntryElapsedMs;
static uint8_t gExpirationPeepEntryValveStartDuty;
static uint8_t gExpirationPeepTrackPending;
static uint32_t gExpirationPlanSequence;
static float gExpirationPressureHistory[EXPIRATION_CONTROLLER_PRESSURE_HISTORY_COUNT];
static uint8_t gExpirationPressureHistoryIndex;
static uint16_t gExpirationCaptureElapsedMs;
static uint8_t gExpirationCaptureStableCount;
static float gExpirationCaptureMargin;

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

/** Smooth a normalized 0..1 transition while keeping zero slope at both ends. */
static float expirationControllerSmoothStep(float value)
{
    value = expirationControllerClamp(value, 0.0F, 1.0F);
    return value * value * (3.0F - (2.0F * value));
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
    int8_t lCaptureStatus;
    int8_t lPeepStatus;

    lPeepStatus = pidInit(&gExpirationPeepPid,
                          EXPIRATION_CONTROLLER_PEEP_KP,
                          EXPIRATION_CONTROLLER_PEEP_KI,
                          EXPIRATION_CONTROLLER_PEEP_KD,
                          EXPIRATION_CONTROLLER_SAMPLE_PERIOD_S,
                          EXPIRATION_CONTROLLER_PEEP_EFFORT_MIN,
                          EXPIRATION_CONTROLLER_PEEP_EFFORT_MAX);
    lCaptureStatus = pidInit(&gExpirationCapturePid,
                             EXPIRATION_CONTROLLER_CAPTURE_PID_KP,
                             EXPIRATION_CONTROLLER_CAPTURE_PID_KI,
                             EXPIRATION_CONTROLLER_CAPTURE_PID_KD,
                             EXPIRATION_CONTROLLER_SAMPLE_PERIOD_S,
                             EXPIRATION_CONTROLLER_CAPTURE_PID_EFFORT_MIN,
                             EXPIRATION_CONTROLLER_CAPTURE_PID_EFFORT_MAX);
    gExpirationControllerReady =
        (uint8_t)((lPeepStatus == PID_STATUS_OK) &&
                  (lCaptureStatus == PID_STATUS_OK));
    gExpirationControllerState = EXPIRATION_CONTROLLER_IDLE;
    gExpirationBlowerTarget = 0U;
    gExpirationValveDuty = EXPIRATION_CONTROLLER_EXP_VALVE_OPEN_DUTY;
    gExpirationPeepEntryElapsedMs = 0U;
    gExpirationPeepEntryValveStartDuty = EXPIRATION_CONTROLLER_EXP_VALVE_OPEN_DUTY;
    gExpirationPeepTrackPending = 0U;
    gExpirationPlanSequence = 0U;
    gExpirationPressureHistoryIndex = 0U;
    gExpirationCaptureElapsedMs = 0U;
    gExpirationCaptureStableCount = 0U;
    gExpirationCaptureMargin = EXPIRATION_CONTROLLER_CAPTURE_MARGIN_MIN;
}

/** Initialize the filtered patient-pressure history for release slope calculation. */
static void expirationControllerPressureHistoryInit(float pressure)
{
    uint8_t lIndex;

    for (lIndex = 0U;
         lIndex < EXPIRATION_CONTROLLER_PRESSURE_HISTORY_COUNT;
         lIndex++) {
        gExpirationPressureHistory[lIndex] = pressure;
    }
    gExpirationPressureHistoryIndex = 0U;
}

/** Calculate patient-pressure slope from filtered samples 24 ms apart. */
static float expirationControllerPressureSlopeGet(float pressure)
{
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
        (void)pidReset(&gExpirationCapturePid);
        expirationControllerPressureHistoryInit(lPatientPressure);
        gExpirationCaptureElapsedMs = 0U;
        gExpirationCaptureStableCount = 0U;
        gExpirationCaptureMargin = EXPIRATION_CONTROLLER_CAPTURE_MARGIN_MIN;
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
        (void)pidReset(&gExpirationCapturePid);
        gExpirationBlowerTarget = 0U;
        gExpirationValveDuty = EXPIRATION_CONTROLLER_EXP_VALVE_OPEN_DUTY;
        gExpirationCaptureElapsedMs = 0U;
        gExpirationCaptureStableCount = 0U;
        gExpirationPeepTrackPending = 0U;
    } else if (state == EXPIRATION_CONTROLLER_PEEP) {
        (void)pidReset(&gExpirationCapturePid);
        (void)pidReset(&gExpirationPeepPid);
        gExpirationPeepEntryElapsedMs = 0U;
        gExpirationPeepEntryValveStartDuty = gExpirationValveDuty;
        gExpirationPeepTrackPending =
            (uint8_t)((lPreviousState == EXPIRATION_CONTROLLER_RELEASE) ||
                      (lPreviousState == EXPIRATION_CONTROLLER_CAPTURE));
    } else {
        (void)pidReset(&gExpirationCapturePid);
        (void)pidReset(&gExpirationPeepPid);
        gExpirationBlowerTarget = 0U;
        gExpirationValveDuty = EXPIRATION_CONTROLLER_EXP_VALVE_OPEN_DUTY;
        gExpirationPeepTrackPending = 0U;
    }
}

/**
 * Produce the controlled capture request while approaching PEEP.
 *
 * Pressure error defines the desired pressure-fall trajectory:
 *     desired dP/dt = -K * (Ppatient - PEEP)
 *
 * A dedicated P controller compares that desired slope with the measured slope
 * and corrects expiratory-valve duty. A pressure-position feedforward term
 * supplies the nominal valve-closing trajectory, while the blower is pre-spooled
 * toward the PEEP feedforward target.
 */
static int8_t expirationControllerCaptureProcess(const stBreathPlan *plan,
                                                  stActuatorRequest *request)
{
    float lBlowerFeedforward;
    float lBlowerProgress;
    float lBlowerTarget;
    float lCaptureEffort;
    float lCaptureProgress;
    float lDesiredSlope;
    float lPatientPressure = controlDataGet(PAT_REAL_PRS);
    float lPositivePressureError;
    float lPressureError = lPatientPressure - plan->peepCmh2o;
    float lPressureSlope = expirationControllerPressureSlopeGet(lPatientPressure);
    float lSmoothProgress;
    float lValveDuty;
    float lValveFeedforward;
    float lValveTarget;

    if (calibtransPrsSpeed(plan->peepCmh2o, &lBlowerFeedforward) !=
        CALIBTRANS_STATUS_OK) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }
    lBlowerFeedforward = expirationControllerClamp(
        lBlowerFeedforward * 0.8F * 10.0F,
        0.0F,
        (float)EXPIRATION_CONTROLLER_BLOWER_TARGET_MAX);

    lPositivePressureError = expirationControllerClamp(
        lPressureError,
        0.0F,
        gExpirationCaptureMargin);
    lDesiredSlope = -(EXPIRATION_CONTROLLER_CAPTURE_SLOPE_K *
                      lPositivePressureError);
    lDesiredSlope = expirationControllerClamp(
        lDesiredSlope,
        -EXPIRATION_CONTROLLER_CAPTURE_MAX_FALL_RATE,
        0.0F);
    if (pidUpdate(&gExpirationCapturePid,
                  lDesiredSlope,
                  lPressureSlope,
                  &lCaptureEffort) != PID_STATUS_OK) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }

    /*
     * Convert distance-to-PEEP into a smooth 0..1 capture progress. This is
     * feedforward only; the slope controller adds/subtracts braking as needed.
     */
    lCaptureProgress = 1.0F -
                       (lPositivePressureError / gExpirationCaptureMargin);
    lSmoothProgress = expirationControllerSmoothStep(lCaptureProgress);
    lValveTarget = expirationControllerClamp(
        EXPIRATION_CONTROLLER_CAPTURE_VALVE_BASE_DUTY + plan->peepCmh2o,
        EXPIRATION_CONTROLLER_CAPTURE_VALVE_DUTY_MIN,
        EXPIRATION_CONTROLLER_CAPTURE_VALVE_DUTY_MAX);
    lValveFeedforward = lValveTarget * lSmoothProgress;
    lValveDuty = lValveFeedforward +
                 (lCaptureEffort *
                  EXPIRATION_CONTROLLER_CAPTURE_VALVE_CORRECTION_SCALE);
    lValveDuty = expirationControllerClamp(
        lValveDuty,
        (float)EXPIRATION_CONTROLLER_EXP_VALVE_OPEN_DUTY,
        (float)EXPIRATION_CONTROLLER_EXP_VALVE_CLOSED_DUTY);

    /*
     * The blower is intentionally pre-spooled as soon as CAPTURE starts. The
     * expiratory valve remains the fast braking actuator; blower only prepares
     * the pressure source required by the following PEEP controller.
     */
    lBlowerProgress =
        EXPIRATION_CONTROLLER_CAPTURE_BLOWER_MIN_PROGRESS +
        ((1.0F - EXPIRATION_CONTROLLER_CAPTURE_BLOWER_MIN_PROGRESS) *
         lSmoothProgress);
    lBlowerTarget = lBlowerFeedforward * lBlowerProgress;

    /*
     * If pressure has already crossed PEEP, stop releasing immediately. This
     * avoids staying in CAPTURE below target while waiting for a stable window.
     */
    if (lPressureError <= 0.0F) {
        lValveDuty = (float)EXPIRATION_CONTROLLER_EXP_VALVE_CLOSED_DUTY;
        lBlowerTarget = lBlowerFeedforward;
    }

    gExpirationBlowerTarget = (uint16_t)expirationControllerMoveTowards(
        (float)gExpirationBlowerTarget,
        lBlowerTarget,
        EXPIRATION_CONTROLLER_CAPTURE_BLOWER_MAX_STEP);
    if (lValveDuty < (float)gExpirationValveDuty) {
        gExpirationValveDuty = (uint8_t)lValveDuty;
    } else {
        gExpirationValveDuty = (uint8_t)expirationControllerMoveTowards(
            (float)gExpirationValveDuty,
            lValveDuty,
            EXPIRATION_CONTROLLER_CAPTURE_VALVE_CLOSE_MAX_STEP);
    }

    if (gExpirationCaptureElapsedMs < EXPIRATION_CONTROLLER_CAPTURE_TIMEOUT_MS) {
        gExpirationCaptureElapsedMs +=
            (uint16_t)(EXPIRATION_CONTROLLER_SAMPLE_PERIOD_S * 1000.0F);
    }

    if (lPressureError <= 0.0F) {
        if (phaseControllerExpirationCaptureNotify() != PHASE_CONTROL_SUCCESS) {
            return ACTUATOR_REQUEST_ERROR_STATE;
        }
    } else if ((lPressureError <=
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
        }
    } else {
        gExpirationCaptureStableCount = 0U;
        if (gExpirationCaptureElapsedMs >=
            EXPIRATION_CONTROLLER_CAPTURE_TIMEOUT_MS) {
            if (phaseControllerExpirationCaptureNotify() != PHASE_CONTROL_SUCCESS) {
                return ACTUATOR_REQUEST_ERROR_STATE;
            }
        }
    }

    request->blowerTarget = gExpirationBlowerTarget;
    request->expiratoryValveDuty = gExpirationValveDuty;
    request->validMask = ACTUATOR_REQUEST_VALID_BREATH_OUTPUTS;
    return ACTUATOR_REQUEST_SUCCESS;
}

/** Produce the unrestricted fast-release request until braking should start. */
static int8_t expirationControllerReleaseProcess(const stBreathPlan *plan,
                                                  stActuatorRequest *request)
{
    float lCaptureMargin;
    float lPatientPressure = controlDataGet(PAT_REAL_PRS);
    float lPressureError = lPatientPressure - plan->peepCmh2o;
    float lPressureSlope = expirationControllerPressureSlopeGet(lPatientPressure);

    lCaptureMargin = EXPIRATION_CONTROLLER_PEEP_BASE_MARGIN +
                     ((-lPressureSlope) * EXPIRATION_CONTROLLER_BRAKE_TIME_S);
    lCaptureMargin = expirationControllerClamp(
        lCaptureMargin,
        EXPIRATION_CONTROLLER_CAPTURE_MARGIN_MIN,
        EXPIRATION_CONTROLLER_CAPTURE_MARGIN_MAX);
    if (lPressureError > lCaptureMargin) {
        gExpirationBlowerTarget = 0U;
        gExpirationValveDuty = EXPIRATION_CONTROLLER_EXP_VALVE_OPEN_DUTY;
        request->blowerTarget = gExpirationBlowerTarget;
        request->expiratoryValveDuty = gExpirationValveDuty;
        request->validMask = ACTUATOR_REQUEST_VALID_BREATH_OUTPUTS;
        return ACTUATOR_REQUEST_SUCCESS;
    }

    gExpirationCaptureMargin = lCaptureMargin;
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
    lBlowerFeedforward = lBlowerFeedforward * 0.8F;
    lPatientPressure = controlDataGet(PAT_REAL_PRS);
    if (gExpirationPeepTrackPending != 0U) {
        lDesiredEffort = ((float)gExpirationBlowerTarget -
                          (lBlowerFeedforward * 10.0F)) /
                         EXPIRATION_CONTROLLER_BLOWER_CORRECTION_SCALE;
        /*
         * Never track a below-feedforward blower command after PEEP has already
         * been crossed; doing so would intentionally preserve the undershoot.
         */
        if ((lPatientPressure <= plan->peepCmh2o) &&
            (lDesiredEffort < 0.0F)) {
            lDesiredEffort = 0.0F;
        }
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
    if ((float)gExpirationPeepEntryValveStartDuty > lEntryValveDuty) {
        lEntryValveDuty = (float)gExpirationPeepEntryValveStartDuty;
    }
    lValveDutyMaximum = lEntryValveDuty +
                        (((float)EXPIRATION_CONTROLLER_EXP_VALVE_CLOSED_DUTY -
                          lEntryValveDuty) * lEntryProgress);

    /*
     * Once pressure is at or below PEEP, do not let the entry ramp prevent the
     * expiratory valve from closing. Recovery must take priority over ramping.
     */
    if (lPatientPressure <= plan->peepCmh2o) {
        lValveDutyMaximum =
            (float)EXPIRATION_CONTROLLER_EXP_VALVE_CLOSED_DUTY;
    }
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
