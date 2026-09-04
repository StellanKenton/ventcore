/************************************************************************************
* @file     : flowcontroller.c
* @brief    : Inspiratory flow controller.
* @details  : Produces a bounded blower request for volume inspirations.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "flowcontroller.h"

#include <stddef.h>

#include "calibtrans.h"
#include "controldata.h"
#include "phasecontroller.h"
#include "pid.h"

static stPid gFlowPid;
static uint8_t gFlowControllerReady;
static eFlowControllerState gFlowControllerState;
static uint32_t gFlowPlanSequence;
static float gFlowFeedforwardPressure;
static uint8_t gFlowFeedforwardPressureReady;
static float gFlowAppliedEffort;

/** Clamp a flow-controller value to a configured range. */
static float flowControllerClamp(float value, float minimum, float maximum)
{
    if (value > maximum) {
        return maximum;
    }
    if (value < minimum) {
        return minimum;
    }
    return value;
}

/** Prepare a complete but invalid request before calculations. */
static void flowControllerRequestClear(stActuatorRequest *request)
{
    request->blowerTarget = 0U;
    request->expiratoryValveDuty = 0U;
    request->oxygenValveDuty = 0U;
    request->reliefValveDuty = FLOW_CONTROLLER_RELIEF_CLOSED_DUTY;
    request->validMask = 0U;
}

/** Reset the flow loop when a new volume inspiration begins. */
static void flowControllerStateEnter(eFlowControllerState state)
{
    if (gFlowControllerState == state) {
        return;
    }
    gFlowControllerState = state;
    if (state == FLOW_CONTROLLER_INSP_RISE) {
        (void)pidSetTunings(&gFlowPid,
                            FLOW_CONTROLLER_FLOW_KP,
                            FLOW_CONTROLLER_FLOW_KI,
                            FLOW_CONTROLLER_FLOW_KD);
        (void)pidReset(&gFlowPid);
    } else if (state == FLOW_CONTROLLER_INSP_HOLD) {
        (void)pidSetTunings(&gFlowPid,
                            FLOW_CONTROLLER_HOLD_KP,
                            FLOW_CONTROLLER_HOLD_KI,
                            FLOW_CONTROLLER_FLOW_KD);
    } else if (state == FLOW_CONTROLLER_IDLE) {
        (void)pidReset(&gFlowPid);
        gFlowFeedforwardPressureReady = 0U;
        gFlowAppliedEffort = 0.0F;
    }
}

/** Select the active inspiratory state and return its bounded flow reference. */
static eFlowControllerState flowControllerActiveStateGet(const stBreathPlan *plan,
                                                         float *flowReference)
{
    float lFlowTarget;

    *flowReference = flowControllerClamp(phaseControlGet(PHASE_REF_FLOW),
                                         FLOW_CONTROLLER_FLOW_TARGET_MIN,
                                         FLOW_CONTROLLER_FLOW_TARGET_MAX);
    lFlowTarget = flowControllerClamp(plan->inspiratoryFlowLpm,
                                      FLOW_CONTROLLER_FLOW_TARGET_MIN,
                                      FLOW_CONTROLLER_FLOW_TARGET_MAX);
    if (*flowReference <= FLOW_CONTROLLER_FLOW_TARGET_MIN) {
        return FLOW_CONTROLLER_INSP_PAUSE;
    }
    if (*flowReference < lFlowTarget) {
        return FLOW_CONTROLLER_INSP_RISE;
    }
    return FLOW_CONTROLLER_INSP_HOLD;
}

/** Produce the inspiratory-pause request without adding delivered volume. */
static int8_t flowControllerPauseProcess(stActuatorRequest *request)
{
    request->blowerTarget = 0U;
    request->expiratoryValveDuty = FLOW_CONTROLLER_EXP_VALVE_CLOSED_DUTY;
    request->validMask = ACTUATOR_REQUEST_VALID_BREATH_OUTPUTS;
    return ACTUATOR_REQUEST_SUCCESS;
}

/** Run the flow loop and produce the active inspiratory request. */
static int8_t flowControllerClosedLoopProcess(const stBreathPlan *plan,
                                              eFlowControllerState state,
                                              float flowReference,
                                              stActuatorRequest *request)
{
    float lBlowerFeedforward;
    float lBlowerMaximum;
    float lEffort;
    float lFeedforwardPressure;
    float lMeasuredFlow;
    float lPatientPressure;
    float lPressureLimit;

    lMeasuredFlow = controlDataGet(INSP_FLOW_FILTERED) * FLOW_CONTROLLER_FLOW_INPUT_SCALE;
    if (pidUpdate(&gFlowPid, flowReference, lMeasuredFlow, &lEffort) != PID_STATUS_OK) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }
    if (state == FLOW_CONTROLLER_INSP_HOLD) {
        gFlowAppliedEffort += FLOW_CONTROLLER_HOLD_EFFORT_ALPHA *
                              (lEffort - gFlowAppliedEffort);
    } else {
        gFlowAppliedEffort = lEffort;
    }

    lPressureLimit = plan->limitSettings->pressureHigh;
    lPatientPressure = flowControllerClamp(controlDataGet(PAT_REAL_PRS),
                                           plan->limitSettings->pressureLow,
                                           lPressureLimit);
    if (gFlowFeedforwardPressureReady == 0U) {
        gFlowFeedforwardPressure = lPatientPressure;
        gFlowFeedforwardPressureReady = 1U;
    } else {
        gFlowFeedforwardPressure +=
            FLOW_CONTROLLER_FEEDFORWARD_PRESSURE_ALPHA *
            (lPatientPressure - gFlowFeedforwardPressure);
    }
    /* Follow lung pressure without feeding its sample-to-sample ripple back. */
    lFeedforwardPressure = gFlowFeedforwardPressure +
                           (FLOW_CONTROLLER_FLOW_FF_LINEAR * flowReference) +
                           (FLOW_CONTROLLER_FLOW_FF_QUADRATIC *
                            flowReference * flowReference);
    lFeedforwardPressure = flowControllerClamp(lFeedforwardPressure,
                                                plan->limitSettings->pressureLow,
                                                lPressureLimit);
    if (calibtransPrsSpeed(lFeedforwardPressure, &lBlowerFeedforward) !=
        CALIBTRANS_STATUS_OK) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }
    if (calibtransPrsSpeed(lPressureLimit, &lBlowerMaximum) !=
        CALIBTRANS_STATUS_OK) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }

    lEffort = lBlowerFeedforward +
              (gFlowAppliedEffort * FLOW_CONTROLLER_BLOWER_SPEED_SCALE);
    lEffort = flowControllerClamp(lEffort,
                                  0.0F,
                                  flowControllerClamp(lBlowerMaximum,
                                                      0.0F,
                                                      (float)FLOW_CONTROLLER_BLOWER_SPEED_SCALE));
    request->blowerTarget = (uint16_t)lEffort;
    request->expiratoryValveDuty = FLOW_CONTROLLER_EXP_VALVE_CLOSED_DUTY;
    request->validMask = ACTUATOR_REQUEST_VALID_BREATH_OUTPUTS;
    return ACTUATOR_REQUEST_SUCCESS;
}

void flowControllerInit(void)
{
    gFlowControllerReady = (uint8_t)(pidInit(&gFlowPid,
                                             FLOW_CONTROLLER_FLOW_KP,
                                             FLOW_CONTROLLER_FLOW_KI,
                                             FLOW_CONTROLLER_FLOW_KD,
                                             FLOW_CONTROLLER_SAMPLE_PERIOD_S,
                                             FLOW_CONTROLLER_EFFORT_MIN,
                                             FLOW_CONTROLLER_EFFORT_MAX) ==
                                     PID_STATUS_OK);
    gFlowControllerState = FLOW_CONTROLLER_IDLE;
    gFlowPlanSequence = 0U;
    gFlowFeedforwardPressure = 0.0F;
    gFlowFeedforwardPressureReady = 0U;
    gFlowAppliedEffort = 0.0F;
}

int8_t flowControllerProcess(const stBreathPlan *plan, stActuatorRequest *request)
{
    eFlowControllerState lState;
    ePhaseControllerState lPhase;
    float lFlowReference;

    if ((plan == NULL) || (request == NULL)) {
        return ACTUATOR_REQUEST_ERROR_PARAM;
    }
    flowControllerRequestClear(request);
    if (gFlowControllerReady == 0U) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }
    if (plan->limitSettings == NULL) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }
    if (plan->breathType != BREATH_TYPE_MANDATORY_VOLUME) {
        return ACTUATOR_REQUEST_ERROR_PARAM;
    }

    lPhase = phaseControllerStateGet();
    if (plan->sequence != gFlowPlanSequence) {
        gFlowPlanSequence = plan->sequence;
        gFlowControllerState = FLOW_CONTROLLER_IDLE;
    }
    switch (lPhase) {
        case PHASE_INSP:
            break;
        case PHASE_IDLE:
        case PHASE_EXP:
        default:
            flowControllerStateEnter(FLOW_CONTROLLER_IDLE);
            return ACTUATOR_REQUEST_ERROR_STATE;
    }

    lState = flowControllerActiveStateGet(plan, &lFlowReference);
    flowControllerStateEnter(lState);
    switch (gFlowControllerState) {
        case FLOW_CONTROLLER_INSP_RISE:
        case FLOW_CONTROLLER_INSP_HOLD:
            return flowControllerClosedLoopProcess(plan,
                                                   gFlowControllerState,
                                                   lFlowReference,
                                                   request);
        case FLOW_CONTROLLER_INSP_PAUSE:
            return flowControllerPauseProcess(request);
        case FLOW_CONTROLLER_IDLE:
        default:
            flowControllerStateEnter(FLOW_CONTROLLER_IDLE);
            return ACTUATOR_REQUEST_ERROR_STATE;
    }
}

/**************************End of file********************************/
