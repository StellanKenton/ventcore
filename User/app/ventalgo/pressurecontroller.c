/************************************************************************************
* @file     : pressurecontroller.c
* @brief    : Inspiratory pressure controller.
* @details  : Produces bounded blower and expiratory-valve actuator requests.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "pressurecontroller.h"

#include <stddef.h>

#include "calibtrans.h"
#include "controldata.h"
#include "phasecontroller.h"
#include "pid.h"

static stPid gPressureOuterPid;
static stPid gPressureInnerPid;
static uint8_t gPressureControllerReady;
static ePressureControllerState gPressureControllerState;
static stPressureControllerDiagnostic gPressureDiagnostic;
static float gPressureFlowCompensation;
static float gPressureRiseStartPressure;
static uint32_t gPressureRiseElapsedMs;
static uint32_t gPressurePlanSequence;

/** Clamp a pressure-controller value to a configured range. */
static float pressureControllerClamp(float value, float minimum, float maximum)
{
    if (value > maximum) {
        return maximum;
    }
    if (value < minimum) {
        return minimum;
    }
    return value;
}

/** Clear the latest inspiratory diagnostic values. */
static void pressureControllerDiagnosticClear(void)
{
    gPressureDiagnostic.inspTarget = 0.0F;
    gPressureDiagnostic.flowCompensation = 0.0F;
    gPressureDiagnostic.patientCorrection = 0.0F;
    gPressureDiagnostic.innerEffort = 0.0F;
    gPressureDiagnostic.blowerFeedforward = 0.0F;
}

/** Prepare a complete but invalid request before controller calculations. */
static void pressureControllerRequestClear(stActuatorRequest *request)
{
    request->blowerTarget = 0U;
    request->expiratoryValveDuty = 0U;
    request->oxygenValveDuty = 0U;
    request->reliefValveDuty = PRESSURE_CONTROLLER_RELIEF_CLOSED_DUTY;
    request->validMask = 0U;
}

void pressureControllerInit(void)
{
    int8_t lInnerStatus;
    int8_t lOuterStatus;

    lOuterStatus = pidInit(&gPressureOuterPid,
                           PRESSURE_CONTROLLER_OUTER_KP,
                           PRESSURE_CONTROLLER_OUTER_KI,
                           PRESSURE_CONTROLLER_OUTER_KD,
                           PRESSURE_CONTROLLER_SAMPLE_PERIOD_S,
                           PRESSURE_CONTROLLER_OUTER_CORRECTION_MIN,
                           PRESSURE_CONTROLLER_OUTER_CORRECTION_MAX);
    lInnerStatus = pidInit(&gPressureInnerPid,
                           PRESSURE_CONTROLLER_INNER_KP,
                           PRESSURE_CONTROLLER_INNER_KI,
                           PRESSURE_CONTROLLER_INNER_KD,
                           PRESSURE_CONTROLLER_SAMPLE_PERIOD_S,
                           PRESSURE_CONTROLLER_EFFORT_MIN,
                           PRESSURE_CONTROLLER_EFFORT_MAX);
    gPressureControllerReady = (uint8_t)((lOuterStatus == PID_STATUS_OK) &&
                                         (lInnerStatus == PID_STATUS_OK));
    gPressureControllerState = PRESSURE_CONTROLLER_IDLE;
    gPressureFlowCompensation = 0.0F;
    gPressureRiseStartPressure = 0.0F;
    gPressureRiseElapsedMs = 0U;
    gPressurePlanSequence = 0U;
    pressureControllerDiagnosticClear();
}

ePressureControllerState pressureControllerStateGet(void)
{
    return gPressureControllerState;
}

/** Reset inspiration state when a new rise begins. */
static void pressureControllerStateEnter(ePressureControllerState state)
{
    if (gPressureControllerState == state) {
        return;
    }
    gPressureControllerState = state;
    if (state == PRESSURE_CONTROLLER_INSP_RISE) {
        (void)pidReset(&gPressureOuterPid);
        (void)pidReset(&gPressureInnerPid);
        gPressureFlowCompensation = 0.0F;
        gPressureRiseStartPressure = controlDataGet(PAT_REAL_PRS);
        gPressureRiseElapsedMs = 0U;
    } else if (state == PRESSURE_CONTROLLER_IDLE) {
        (void)pidReset(&gPressureOuterPid);
        (void)pidReset(&gPressureInnerPid);
        pressureControllerDiagnosticClear();
    }
}

/** Calculate the inspiratory target through the patient-pressure outer loop. */
static int8_t pressureControllerOuterLoopProcess(const stBreathPlan *plan,
                                                 ePressureControllerState state,
                                                 float *inspTarget)
{
    float lFlow;
    float lFlowCompensation;
    float lFlowCompensationMaximum;
    float lInspCorrection;
    float lPatientPressure;
    float lPatientReference;
    float lPressureLimit;
    int8_t lStatus;

    if ((plan == NULL) || (inspTarget == NULL)) {
        return PID_ERROR_PARAM;
    }

    lPressureLimit = plan->limitSettings->pressureHigh;
    if (((plan->mode == VENT_MD_CPAP_PSV) || (plan->mode == VENT_MD_PSV_ST)) &&
        (plan->pressureLimitCmh2o < lPressureLimit)) {
        lPressureLimit = plan->pressureLimitCmh2o;
    }
    lPatientReference = pressureControllerClamp(phaseControlGet(PHASE_REF_PRESSURE),
                                                 plan->limitSettings->pressureLow,
                                                 lPressureLimit);
    lPatientPressure = controlDataGet(PAT_REAL_PRS);
    lFlow = controlDataGet(INSP_FLOW_FILTERED) * PRESSURE_CONTROLLER_FLOW_INPUT_SCALE;
    lFlow = pressureControllerClamp(lFlow, 0.0F, PRESSURE_CONTROLLER_FLOW_INPUT_MAX);
    lFlowCompensation = (PRESSURE_CONTROLLER_FLOW_FF_LINEAR * lFlow) +
                        (PRESSURE_CONTROLLER_FLOW_FF_QUADRATIC * lFlow * lFlow);
    lFlowCompensationMaximum = (plan->inspiratoryPressureCmh2o - plan->peepCmh2o) *
                               PRESSURE_CONTROLLER_FLOW_FF_DELTA_RATIO;
    lFlowCompensationMaximum = pressureControllerClamp(lFlowCompensationMaximum,
                                                        0.0F,
                                                        PRESSURE_CONTROLLER_FLOW_FF_MAX);
    lFlowCompensation = pressureControllerClamp(lFlowCompensation,
                                                0.0F,
                                                lFlowCompensationMaximum);
    if ((state == PRESSURE_CONTROLLER_INSP_HOLD) &&
        (lFlowCompensation > gPressureFlowCompensation)) {
        lFlowCompensation = gPressureFlowCompensation;
    }
    if (state == PRESSURE_CONTROLLER_INSP_HOLD) {
        gPressureFlowCompensation += PRESSURE_CONTROLLER_FLOW_FF_HOLD_FILTER_GAIN *
                                     (lFlowCompensation - gPressureFlowCompensation);
    } else {
        gPressureFlowCompensation += PRESSURE_CONTROLLER_FLOW_FF_RISE_FILTER_GAIN *
                                     (lFlowCompensation - gPressureFlowCompensation);
    }

    lStatus = pidUpdate(&gPressureOuterPid,
                        lPatientReference,
                        lPatientPressure,
                        &lInspCorrection);
    if (lStatus != PID_STATUS_OK) {
        return lStatus;
    }

    *inspTarget = pressureControllerClamp(lPatientReference +
                                          gPressureFlowCompensation +
                                          lInspCorrection,
                                          PRESSURE_CONTROLLER_INSP_TARGET_MIN,
                                          pressureControllerClamp(lPressureLimit,
                                                                  PRESSURE_CONTROLLER_INSP_TARGET_MIN,
                                                                  PRESSURE_CONTROLLER_INSP_TARGET_MAX));
    gPressureDiagnostic.inspTarget = *inspTarget;
    gPressureDiagnostic.flowCompensation = gPressureFlowCompensation;
    gPressureDiagnostic.patientCorrection = lInspCorrection;
    return PID_STATUS_OK;
}

/** Calculate the actuator effort through the inspiratory-pressure inner loop. */
static int8_t pressureControllerInnerLoopProcess(float inspTarget, float *effort)
{
    return pidUpdate(&gPressureInnerPid,
                     inspTarget,
                     controlDataGet(INSP_REAL_PRS),
                     effort);
}

/** Vent bounded excess pressure during inspiratory hold. */
static void pressureControllerHoldReliefProcess(stActuatorRequest *request)
{
    float lExcessPressure;
    float lValveOpening;

    lExcessPressure = controlDataGet(PAT_REAL_PRS) -
                      phaseControlGet(PHASE_REF_PRESSURE);
    if (lExcessPressure <= PRESSURE_CONTROLLER_HOLD_RELIEF_DEADBAND) {
        return;
    }
    lValveOpening = (lExcessPressure - PRESSURE_CONTROLLER_HOLD_RELIEF_DEADBAND) *
                    PRESSURE_CONTROLLER_HOLD_RELIEF_GAIN;
    lValveOpening = pressureControllerClamp(lValveOpening,
                                             0.0F,
                                             PRESSURE_CONTROLLER_HOLD_RELIEF_MAX_OPENING);
    request->expiratoryValveDuty =
        (uint8_t)((float)PRESSURE_CONTROLLER_EXP_VALVE_CLOSED_DUTY - lValveOpening);
}

/** Run the shared pressure loops and produce the base inspiratory request. */
static int8_t pressureControllerClosedLoopProcess(const stBreathPlan *plan,
                                                  ePressureControllerState state,
                                                  stActuatorRequest *request)
{
    float lBlowerFeedforward;
    float lEffort;
    float lInspTarget;

    if (pressureControllerOuterLoopProcess(plan, state, &lInspTarget) !=
        PID_STATUS_OK) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }
    if (pressureControllerInnerLoopProcess(lInspTarget, &lEffort) != PID_STATUS_OK) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }
    if (calibtransPrsSpeed(lInspTarget, &lBlowerFeedforward) !=
        CALIBTRANS_STATUS_OK) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }

    gPressureDiagnostic.innerEffort = lEffort;
    gPressureDiagnostic.blowerFeedforward = lBlowerFeedforward;
    lEffort = (lEffort * PRESSURE_CONTROLLER_BLOWER_SPEED_SCALE) +
              lBlowerFeedforward;
    lEffort = pressureControllerClamp(lEffort,
                                      0.0F,
                                      (float)PRESSURE_CONTROLLER_BLOWER_SPEED_SCALE);
    request->blowerTarget = (uint16_t)lEffort;
    request->expiratoryValveDuty = PRESSURE_CONTROLLER_EXP_VALVE_CLOSED_DUTY;
    request->validMask = ACTUATOR_REQUEST_VALID_BREATH_OUTPUTS;
    return ACTUATOR_REQUEST_SUCCESS;
}

/** Produce the steady inspiratory-hold request. */
static int8_t pressureControllerHoldProcess(const stBreathPlan *plan,
                                            stActuatorRequest *request)
{
    int8_t lStatus;

    (void)phaseControlSet(PHASE_REF_PRESSURE,
                          plan->inspiratoryPressureCmh2o);
    lStatus = pressureControllerClosedLoopProcess(plan,
                                                  PRESSURE_CONTROLLER_INSP_HOLD,
                                                  request);
    if (lStatus == ACTUATOR_REQUEST_SUCCESS) {
        pressureControllerHoldReliefProcess(request);
    }
    return lStatus;
}

/** Produce the rising inspiratory-pressure request. */
static int8_t pressureControllerRiseProcess(const stBreathPlan *plan,
                                            stActuatorRequest *request)
{
    float lCurveProgress;
    float lPressureRange;
    float lRemaining;
    float lTimeProgress;

    if ((plan->riseTimeMs == 0U) ||
        (gPressureRiseElapsedMs >= plan->riseTimeMs)) {
        pressureControllerStateEnter(PRESSURE_CONTROLLER_INSP_HOLD);
        return pressureControllerHoldProcess(plan, request);
    }

    lTimeProgress = (float)gPressureRiseElapsedMs / (float)plan->riseTimeMs;
    lRemaining = 1.0F - lTimeProgress;
    /* Lightweight charging curve: about 61% at T/3 and 79% at T/2. */
    lCurveProgress = 1.0F - ((lRemaining * lRemaining) *
                            (0.65F + (0.35F * lRemaining)));
    lPressureRange = plan->inspiratoryPressureCmh2o -
                     gPressureRiseStartPressure;
    (void)phaseControlSet(PHASE_REF_PRESSURE,
                          gPressureRiseStartPressure +
                          (lPressureRange * lCurveProgress));
    gPressureRiseElapsedMs += PRESSURE_CONTROLLER_SAMPLE_PERIOD_MS;
    return pressureControllerClosedLoopProcess(plan,
                                               PRESSURE_CONTROLLER_INSP_RISE,
                                               request);
}

int8_t pressureControllerProcess(const stBreathPlan *plan, stActuatorRequest *request)
{
    ePhaseControllerState lPhase;

    if ((plan == NULL) || (request == NULL)) {
        return ACTUATOR_REQUEST_ERROR_PARAM;
    }
    pressureControllerRequestClear(request);
    if (gPressureControllerReady == 0U) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }
    if (plan->limitSettings == NULL) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }
    if ((plan->breathType != BREATH_TYPE_MANDATORY_PRESSURE) &&
        (plan->breathType != BREATH_TYPE_SPONTANEOUS_PRESSURE_SUPPORT)) {
        return ACTUATOR_REQUEST_ERROR_PARAM;
    }

    lPhase = phaseControllerStateGet();
    if (plan->sequence != gPressurePlanSequence) {
        gPressurePlanSequence = plan->sequence;
        gPressureControllerState = PRESSURE_CONTROLLER_IDLE;
    }
    switch (lPhase) {
        case PHASE_INSP:
            break;
        case PHASE_IDLE:
        case PHASE_EXP:
        default:
            pressureControllerStateEnter(PRESSURE_CONTROLLER_IDLE);
            return ACTUATOR_REQUEST_ERROR_STATE;
    }
    switch (gPressureControllerState) {
        case PRESSURE_CONTROLLER_IDLE:
            pressureControllerStateEnter(PRESSURE_CONTROLLER_INSP_RISE);
            return pressureControllerRiseProcess(plan, request);
        case PRESSURE_CONTROLLER_INSP_RISE:
            return pressureControllerRiseProcess(plan, request);
        case PRESSURE_CONTROLLER_INSP_HOLD:
            return pressureControllerHoldProcess(plan, request);
        default:
            pressureControllerStateEnter(PRESSURE_CONTROLLER_IDLE);
            return ACTUATOR_REQUEST_ERROR_STATE;
    }
}

void pressureControllerDiagnosticGet(stPressureControllerDiagnostic *diagnostic)
{
    if (diagnostic == NULL) {
        return;
    }
    *diagnostic = gPressureDiagnostic;
}

/**************************End of file********************************/
