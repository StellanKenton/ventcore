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
    float lBlowerFeedforward;
    float lBlowerMaximum;
    float lEffort;
    float lFeedforwardPressure;
    float lFlowReference;
    float lFlowTarget;
    float lMeasuredFlow;
    float lPatientPressure;
    float lPressureLimit;

    if ((plan == NULL) || (request == NULL)) {
        return ACTUATOR_REQUEST_ERROR_PARAM;
    }
    flowControllerRequestClear(request);
    if ((gFlowControllerReady == 0U) ||
        (plan->breathType != BREATH_TYPE_MANDATORY_VOLUME) ||
        (plan->limitSettings == NULL)) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }

    lPhase = phaseControllerStateGet();
    if (plan->sequence != gFlowPlanSequence) {
        gFlowPlanSequence = plan->sequence;
        gFlowControllerState = FLOW_CONTROLLER_IDLE;
    }
    if (lPhase != PHASE_INSP) {
        flowControllerStateEnter(FLOW_CONTROLLER_IDLE);
        return ACTUATOR_REQUEST_ERROR_STATE;
    }
    lFlowReference = flowControllerClamp(phaseControlGet(PHASE_REF_FLOW),
                                         FLOW_CONTROLLER_FLOW_TARGET_MIN,
                                         FLOW_CONTROLLER_FLOW_TARGET_MAX);
    lFlowTarget = flowControllerClamp(plan->inspiratoryFlowLpm,
                                      FLOW_CONTROLLER_FLOW_TARGET_MIN,
                                      FLOW_CONTROLLER_FLOW_TARGET_MAX);
    if (lFlowReference <= FLOW_CONTROLLER_FLOW_TARGET_MIN) {
        lState = FLOW_CONTROLLER_INSP_PAUSE;
    } else if (lFlowReference < lFlowTarget) {
        lState = FLOW_CONTROLLER_INSP_RISE;
    } else {
        lState = FLOW_CONTROLLER_INSP_HOLD;
    }
    flowControllerStateEnter(lState);

    if (lState == FLOW_CONTROLLER_INSP_PAUSE) {
        /* Inspiratory pause is part of Ti but must not add delivered volume. */
        request->blowerTarget = 0U;
        request->expiratoryValveDuty = FLOW_CONTROLLER_EXP_VALVE_CLOSED_DUTY;
        request->validMask = ACTUATOR_REQUEST_VALID_BREATH_OUTPUTS;
        return ACTUATOR_REQUEST_SUCCESS;
    }

    lMeasuredFlow = controlDataGet(INSP_FLOW_FILTERED) * FLOW_CONTROLLER_FLOW_INPUT_SCALE;
    if (pidUpdate(&gFlowPid, lFlowReference, lMeasuredFlow, &lEffort) != PID_STATUS_OK) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }
    if (lState == FLOW_CONTROLLER_INSP_HOLD) {
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
                           (FLOW_CONTROLLER_FLOW_FF_LINEAR * lFlowReference) +
                           (FLOW_CONTROLLER_FLOW_FF_QUADRATIC *
                            lFlowReference * lFlowReference);
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

/**************************End of file********************************/
