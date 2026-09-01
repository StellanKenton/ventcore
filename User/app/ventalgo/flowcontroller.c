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
    if ((state == FLOW_CONTROLLER_INSP_RISE) ||
        (state == FLOW_CONTROLLER_IDLE)) {
        (void)pidReset(&gFlowPid);
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
    float lMeasuredFlow;
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
    lState = (phaseControlGet(PHASE_REF_FLOW) > 0.0F) ?
             FLOW_CONTROLLER_INSP_RISE : FLOW_CONTROLLER_INSP_HOLD;
    flowControllerStateEnter(lState);

    if (lState == FLOW_CONTROLLER_INSP_HOLD) {
        /* Inspiratory pause is part of Ti but must not add delivered volume. */
        request->blowerTarget = 0U;
        request->expiratoryValveDuty = FLOW_CONTROLLER_EXP_VALVE_CLOSED_DUTY;
        request->validMask = ACTUATOR_REQUEST_VALID_BREATH_OUTPUTS;
        return ACTUATOR_REQUEST_SUCCESS;
    }

    lFlowReference = flowControllerClamp(phaseControlGet(PHASE_REF_FLOW),
                                         FLOW_CONTROLLER_FLOW_TARGET_MIN,
                                         FLOW_CONTROLLER_FLOW_TARGET_MAX);
    lMeasuredFlow = controlDataGet(INSP_FLOW_FILTERED) * FLOW_CONTROLLER_FLOW_INPUT_SCALE;
    if (pidUpdate(&gFlowPid, lFlowReference, lMeasuredFlow, &lEffort) != PID_STATUS_OK) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }

    lPressureLimit = plan->limitSettings->pressureHigh;
    lFeedforwardPressure = plan->peepCmh2o +
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
              (lEffort * FLOW_CONTROLLER_BLOWER_SPEED_SCALE);
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
