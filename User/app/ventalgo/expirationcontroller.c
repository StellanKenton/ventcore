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
static uint32_t gExpirationPlanSequence;
static float gExpirationPressureHistory[EXPIRATION_CONTROLLER_PRESSURE_HISTORY_COUNT];
static uint8_t gExpirationPressureHistoryIndex;
static uint16_t gExpirationCaptureElapsedMs;
static uint8_t gExpirationCaptureStableCount;
static float gExpirationCaptureMargin;

/* Directly learned PEEP feedforward for the active PEEP setting. */
static float gExpirationPeepAdaptiveFeedforward;
static float gExpirationPeepAdaptiveBase;
static float gExpirationPeepAdaptiveTarget;
static uint8_t gExpirationPeepAdaptiveValid;

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

/** Absolute value helper without requiring the math library. */
static float expirationControllerAbs(float value)
{
    return (value < 0.0F) ? -value : value;
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

/**
 * Get the active learned PEEP feedforward.
 *
 * The calibration table is only the initial estimate. For the same configured
 * PEEP, the learned feedforward is retained across breaths. A material PEEP
 * setting change starts learning again from the new calibrated FF.
 */
static int8_t expirationControllerPeepFeedforwardGet(float peepCmh2o,
                                                      float *feedforward)
{
    float lBaseSpeed;
    float lBaseTarget;

    if (calibtransPrsSpeed(peepCmh2o, &lBaseSpeed) != CALIBTRANS_STATUS_OK) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }
    lBaseTarget = expirationControllerClamp(
        lBaseSpeed * 10.0F,
        0.0F,
        (float)EXPIRATION_CONTROLLER_BLOWER_TARGET_MAX);

    if ((gExpirationPeepAdaptiveValid == 0U) ||
        (expirationControllerAbs(peepCmh2o - gExpirationPeepAdaptiveTarget) >=
         EXPIRATION_CONTROLLER_PEEP_ADAPT_TARGET_CHANGE_RESET)) {
        gExpirationPeepAdaptiveTarget = peepCmh2o;
        gExpirationPeepAdaptiveBase = lBaseTarget;
        gExpirationPeepAdaptiveFeedforward = lBaseTarget;
        gExpirationPeepAdaptiveValid = 1U;
    } else {
        /* Keep the current calibration value as the safety reference for limits. */
        gExpirationPeepAdaptiveBase = lBaseTarget;
    }

    *feedforward = gExpirationPeepAdaptiveFeedforward;
    return ACTUATOR_REQUEST_SUCCESS;
}

/**
 * Directly learn the feedforward value from stable PEEP pressure error.
 *
 * P < PEEP: increase FF.
 * P > PEEP: decrease FF.
 *
 * Learning is deliberately frozen while pressure is moving quickly or while the
 * PEEP entry transient is still active. The learned FF itself is the new output
 * baseline; there is no separate trim or offset added later.
 */
static void expirationControllerPeepFeedforwardLearn(const stBreathPlan *plan,
                                                      float patientPressure,
                                                      float pressureSlope,
                                                      float *feedforward)
{
    float lDelta;
    float lError;
    float lMaximum;
    float lMinimum;

    if ((gExpirationPeepAdaptiveValid == 0U) ||
        (gExpirationPeepEntryElapsedMs <
         EXPIRATION_CONTROLLER_PEEP_ADAPT_SETTLE_TIME_MS)) {
        return;
    }

    lError = plan->peepCmh2o - patientPressure;
    if ((expirationControllerAbs(lError) >
         EXPIRATION_CONTROLLER_PEEP_ADAPT_PRESSURE_WINDOW) ||
        (expirationControllerAbs(lError) <=
         EXPIRATION_CONTROLLER_PEEP_ADAPT_PRESSURE_DEADBAND) ||
        (expirationControllerAbs(pressureSlope) >
         EXPIRATION_CONTROLLER_PEEP_ADAPT_SLOPE_MAX)) {
        return;
    }

    lDelta = lError * EXPIRATION_CONTROLLER_PEEP_ADAPT_GAIN_PER_CYCLE;
    lDelta = expirationControllerClamp(
        lDelta,
        -EXPIRATION_CONTROLLER_PEEP_ADAPT_MAX_STEP_PER_CYCLE,
        EXPIRATION_CONTROLLER_PEEP_ADAPT_MAX_STEP_PER_CYCLE);

    lMinimum = gExpirationPeepAdaptiveBase *
               EXPIRATION_CONTROLLER_PEEP_ADAPT_MIN_RATIO;
    lMaximum = gExpirationPeepAdaptiveBase *
               EXPIRATION_CONTROLLER_PEEP_ADAPT_MAX_RATIO;
    lMinimum = expirationControllerClamp(
        lMinimum,
        0.0F,
        (float)EXPIRATION_CONTROLLER_BLOWER_TARGET_MAX);
    lMaximum = expirationControllerClamp(
        lMaximum,
        lMinimum,
        (float)EXPIRATION_CONTROLLER_BLOWER_TARGET_MAX);

    gExpirationPeepAdaptiveFeedforward = expirationControllerClamp(
        gExpirationPeepAdaptiveFeedforward + lDelta,
        lMinimum,
        lMaximum);
    *feedforward = gExpirationPeepAdaptiveFeedforward;
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
    gExpirationPlanSequence = 0U;
    gExpirationPressureHistoryIndex = 0U;
    gExpirationCaptureElapsedMs = 0U;
    gExpirationCaptureStableCount = 0U;
    gExpirationCaptureMargin = EXPIRATION_CONTROLLER_CAPTURE_MARGIN_MIN;
    gExpirationPeepAdaptiveFeedforward = 0.0F;
    gExpirationPeepAdaptiveBase = 0.0F;
    gExpirationPeepAdaptiveTarget = 0.0F;
    gExpirationPeepAdaptiveValid = 0U;
}

/** Initialize patient-pressure history for pressure-slope calculation. */
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
        /* RELEASE has already brought the blower toward the learned PEEP FF. */
        gExpirationValveDuty = EXPIRATION_CONTROLLER_EXP_VALVE_OPEN_DUTY;
        gExpirationCaptureElapsedMs = 0U;
        gExpirationCaptureStableCount = 0U;
    } else if (state == EXPIRATION_CONTROLLER_PEEP) {
        (void)pidReset(&gExpirationCapturePid);
        (void)pidReset(&gExpirationPeepPid);
        gExpirationPeepEntryElapsedMs = 0U;
        gExpirationPeepEntryValveStartDuty = gExpirationValveDuty;
    } else {
        (void)pidReset(&gExpirationCapturePid);
        (void)pidReset(&gExpirationPeepPid);
        gExpirationBlowerTarget = 0U;
        gExpirationValveDuty = EXPIRATION_CONTROLLER_EXP_VALVE_OPEN_DUTY;
    }
}

/** Produce the controlled capture request while approaching PEEP. */
static int8_t expirationControllerCaptureProcess(const stBreathPlan *plan,
                                                  stActuatorRequest *request)
{
    float lBlowerFeedforward;
    float lBlowerTarget;
    float lCaptureCorrectionScale;
    float lCaptureEffort;
    float lCaptureProgress;
    float lDesiredSlope;
    float lPatientPressure = controlDataGet(PAT_REAL_PRS);
    float lPositivePressureError;
    float lPressureError = lPatientPressure - plan->peepCmh2o;
    float lPressureSlope = expirationControllerPressureSlopeGet(lPatientPressure);
    float lSlopeBlend;
    float lSlopeGain;
    float lSlopeOverspeed;
    float lSlopeUnderspeed;
    float lSmoothProgress;
    float lValveCloseStep;
    float lValveDuty;
    float lValveFeedforward;
    float lValveOpenStep;
    float lValveTarget;

    if (expirationControllerPeepFeedforwardGet(plan->peepCmh2o,
                                               &lBlowerFeedforward) !=
        ACTUATOR_REQUEST_SUCCESS) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }

    lPositivePressureError = expirationControllerClamp(
        lPressureError,
        0.0F,
        gExpirationCaptureMargin);

    /* Faster far from PEEP, progressively gentler inside the last 6 cmH2O. */
    lSlopeBlend = expirationControllerClamp(
        lPositivePressureError /
            EXPIRATION_CONTROLLER_CAPTURE_SLOPE_K_BLEND_ERROR,
        0.0F,
        1.0F);
    lSlopeGain = EXPIRATION_CONTROLLER_CAPTURE_SLOPE_K_NEAR +
                 ((EXPIRATION_CONTROLLER_CAPTURE_SLOPE_K_FAR -
                   EXPIRATION_CONTROLLER_CAPTURE_SLOPE_K_NEAR) *
                  lSlopeBlend);
    lDesiredSlope = -(lSlopeGain * lPositivePressureError);
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

    lCaptureProgress = 1.0F -
                       (lPositivePressureError / gExpirationCaptureMargin);
    lSmoothProgress = expirationControllerSmoothStep(lCaptureProgress);
    lValveTarget = expirationControllerClamp(
        EXPIRATION_CONTROLLER_CAPTURE_VALVE_BASE_DUTY + plan->peepCmh2o,
        EXPIRATION_CONTROLLER_CAPTURE_VALVE_DUTY_MIN,
        EXPIRATION_CONTROLLER_CAPTURE_VALVE_DUTY_MAX);
    lValveFeedforward = lValveTarget * lSmoothProgress;

    /* Stronger braking than reopening suppresses the previous underdamped snap. */
    lCaptureCorrectionScale =
        (lCaptureEffort >= 0.0F) ?
            EXPIRATION_CONTROLLER_CAPTURE_VALVE_CORRECTION_CLOSE_SCALE :
            EXPIRATION_CONTROLLER_CAPTURE_VALVE_CORRECTION_OPEN_SCALE;
    lValveDuty = lValveFeedforward +
                 (lCaptureEffort * lCaptureCorrectionScale);
    lValveDuty = expirationControllerClamp(
        lValveDuty,
        (float)EXPIRATION_CONTROLLER_EXP_VALVE_OPEN_DUTY,
        (float)EXPIRATION_CONTROLLER_EXP_VALVE_CLOSED_DUTY);

    /* The slow blower stays at the learned PEEP FF throughout capture. */
    lBlowerTarget = lBlowerFeedforward;

    lSlopeOverspeed = expirationControllerClamp(
        lDesiredSlope - lPressureSlope,
        0.0F,
        EXPIRATION_CONTROLLER_CAPTURE_SLOPE_OVERSPEED_MAX);
    lValveCloseStep =
        EXPIRATION_CONTROLLER_CAPTURE_VALVE_CLOSE_STEP_BASE +
        (lSlopeOverspeed *
         EXPIRATION_CONTROLLER_CAPTURE_VALVE_CLOSE_STEP_GAIN);
    lValveCloseStep = expirationControllerClamp(
        lValveCloseStep,
        EXPIRATION_CONTROLLER_CAPTURE_VALVE_CLOSE_STEP_BASE,
        EXPIRATION_CONTROLLER_CAPTURE_VALVE_CLOSE_STEP_MAX);

    lSlopeUnderspeed = expirationControllerClamp(
        lPressureSlope - lDesiredSlope,
        0.0F,
        EXPIRATION_CONTROLLER_CAPTURE_SLOPE_UNDERSPEED_MAX);
    lValveOpenStep =
        EXPIRATION_CONTROLLER_CAPTURE_VALVE_OPEN_STEP_BASE +
        (lSlopeUnderspeed *
         EXPIRATION_CONTROLLER_CAPTURE_VALVE_OPEN_STEP_GAIN);
    lValveOpenStep = expirationControllerClamp(
        lValveOpenStep,
        EXPIRATION_CONTROLLER_CAPTURE_VALVE_OPEN_STEP_BASE,
        EXPIRATION_CONTROLLER_CAPTURE_VALVE_OPEN_STEP_MAX);

    if (lPressureError <= 0.0F) {
        lValveDuty = (float)EXPIRATION_CONTROLLER_EXP_VALVE_CLOSED_DUTY;
        lBlowerTarget = lBlowerFeedforward;
        lValveCloseStep = EXPIRATION_CONTROLLER_CAPTURE_VALVE_CLOSE_STEP_MAX;
    }

    gExpirationBlowerTarget = (uint16_t)expirationControllerMoveTowards(
        (float)gExpirationBlowerTarget,
        lBlowerTarget,
        EXPIRATION_CONTROLLER_CAPTURE_BLOWER_MAX_STEP);
    if (lValveDuty < (float)gExpirationValveDuty) {
        gExpirationValveDuty = (uint8_t)expirationControllerMoveTowards(
            (float)gExpirationValveDuty,
            lValveDuty,
            lValveOpenStep);
    } else {
        gExpirationValveDuty = (uint8_t)expirationControllerMoveTowards(
            (float)gExpirationValveDuty,
            lValveDuty,
            lValveCloseStep);
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
               (expirationControllerAbs(lPressureSlope) <=
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

/** Produce unrestricted release until the dynamic braking distance is reached. */
static int8_t expirationControllerReleaseProcess(const stBreathPlan *plan,
                                                  stActuatorRequest *request)
{
    float lBlowerFeedforward;
    float lCaptureMargin;
    float lPatientPressure = controlDataGet(PAT_REAL_PRS);
    float lPressureError = lPatientPressure - plan->peepCmh2o;
    float lPressureSlope = expirationControllerPressureSlopeGet(lPatientPressure);

    if (expirationControllerPeepFeedforwardGet(plan->peepCmh2o,
                                               &lBlowerFeedforward) !=
        ACTUATOR_REQUEST_SUCCESS) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }

    lCaptureMargin = EXPIRATION_CONTROLLER_PEEP_BASE_MARGIN +
                     ((-lPressureSlope) * EXPIRATION_CONTROLLER_BRAKE_TIME_S);
    lCaptureMargin = expirationControllerClamp(
        lCaptureMargin,
        EXPIRATION_CONTROLLER_CAPTURE_MARGIN_MIN,
        EXPIRATION_CONTROLLER_CAPTURE_MARGIN_MAX);

    if (lPressureError > lCaptureMargin) {
        /* Valve fully open for speed; blower already moves to the learned PEEP FF. */
        gExpirationBlowerTarget = (uint16_t)expirationControllerMoveTowards(
            (float)gExpirationBlowerTarget,
            lBlowerFeedforward,
            EXPIRATION_CONTROLLER_BLOWER_MAX_STEP);
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

/** Produce the closed-loop PEEP request around the directly learned FF. */
static int8_t expirationControllerPeepProcess(const stBreathPlan *plan,
                                              stActuatorRequest *request)
{
    float lBlowerFeedforward;
    float lBlowerTarget;
    float lEffort;
    float lEntryProgress;
    float lEntryValveDuty;
    float lExcessPressure;
    float lPatientPressure;
    float lPressureSlope;
    float lSlopeOpening = 0.0F;
    float lValveDuty;
    float lValveDutyMaximum;
    float lValveOpenStep;
    float lValveOpening;

    if (expirationControllerPeepFeedforwardGet(plan->peepCmh2o,
                                               &lBlowerFeedforward) !=
        ACTUATOR_REQUEST_SUCCESS) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }

    lPatientPressure = controlDataGet(PAT_REAL_PRS);
    lPressureSlope = expirationControllerPressureSlopeGet(lPatientPressure);

    /* Learn the FF itself only after the PEEP transient has settled. */
    expirationControllerPeepFeedforwardLearn(plan,
                                             lPatientPressure,
                                             lPressureSlope,
                                             &lBlowerFeedforward);

    /* P-only feedback handles fast disturbances; no integral competes with FF learning. */
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

    /* Static pressure relief. */
    lExcessPressure = lPatientPressure - plan->peepCmh2o;
    lValveOpening = (lExcessPressure - EXPIRATION_CONTROLLER_PEEP_RELIEF_DEADBAND) *
                    EXPIRATION_CONTROLLER_PEEP_RELIEF_GAIN;
    lValveOpening = expirationControllerClamp(
        lValveOpening,
        0.0F,
        EXPIRATION_CONTROLLER_PEEP_RELIEF_MAX_OPENING);

    /*
     * Dynamic damping: when pressure is already close to PEEP and rising fast,
     * vent early instead of waiting for a large positive pressure error.
     */
    if ((lPatientPressure >=
         (plan->peepCmh2o -
          EXPIRATION_CONTROLLER_PEEP_RELIEF_SLOPE_ENABLE_MARGIN)) &&
        (lPressureSlope > EXPIRATION_CONTROLLER_PEEP_RELIEF_SLOPE_DEADBAND)) {
        lSlopeOpening =
            (lPressureSlope -
             EXPIRATION_CONTROLLER_PEEP_RELIEF_SLOPE_DEADBAND) *
            EXPIRATION_CONTROLLER_PEEP_RELIEF_SLOPE_GAIN;
        lSlopeOpening = expirationControllerClamp(
            lSlopeOpening,
            0.0F,
            EXPIRATION_CONTROLLER_PEEP_RELIEF_SLOPE_MAX_OPENING);
        lValveOpening += lSlopeOpening;
    }
    lValveOpening = expirationControllerClamp(
        lValveOpening,
        0.0F,
        EXPIRATION_CONTROLLER_PEEP_RELIEF_MAX_OPENING);
    lValveDuty = (float)EXPIRATION_CONTROLLER_EXP_VALVE_CLOSED_DUTY -
                 lValveOpening;

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

    if (lPatientPressure <= plan->peepCmh2o) {
        lValveDutyMaximum =
            (float)EXPIRATION_CONTROLLER_EXP_VALVE_CLOSED_DUTY;
    }
    lValveDuty = expirationControllerClamp(lValveDuty, 0.0F, lValveDutyMaximum);

    gExpirationBlowerTarget = (uint16_t)expirationControllerMoveTowards(
        (float)gExpirationBlowerTarget,
        lBlowerTarget,
        EXPIRATION_CONTROLLER_BLOWER_MAX_STEP);

    /* The faster pressure rises, the faster the valve is allowed to open for damping. */
    lValveOpenStep = EXPIRATION_CONTROLLER_PEEP_VALVE_OPEN_STEP_BASE +
                     (expirationControllerClamp(lPressureSlope, 0.0F, 100.0F) *
                      EXPIRATION_CONTROLLER_PEEP_VALVE_OPEN_SLOPE_GAIN);
    lValveOpenStep = expirationControllerClamp(
        lValveOpenStep,
        EXPIRATION_CONTROLLER_PEEP_VALVE_OPEN_STEP_BASE,
        EXPIRATION_CONTROLLER_PEEP_VALVE_OPEN_STEP_MAX);

    if (lValveDuty < (float)gExpirationValveDuty) {
        gExpirationValveDuty = (uint8_t)expirationControllerMoveTowards(
            (float)gExpirationValveDuty,
            lValveDuty,
            lValveOpenStep);
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
