/************************************************************************************
* @file     : flowcontroller.c
* @brief    : Ventilation flow controller.
* @details  : Provides flow controller initialization and periodic processing.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "flowcontroller.h"

#include "breathscheduler.h"
#include "calibtrans.h"
#include "controldata.h"
#include "phasecontroller.h"
#include "pid.h"
#include "pressurecontroller.h"

static stPid gFlowPid;
static uint16_t gFlowBlowerTarget;
static uint8_t gFlowExpValveDuty;
static uint8_t gFlowControllerReady;
static eFlowControllerState gFlowControllerState;

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

/** Clear the automatic actuator request. */
static void flowControllerOutputClear(void)
{
    gFlowBlowerTarget = 0U;
    gFlowExpValveDuty = 0U;
}

/** Reset the inspiratory flow loop. */
static void flowControllerInspirationReset(void)
{
    (void)pidReset(&gFlowPid);
}

/** Resolve the active controller state from the shared breath phase. */
static eFlowControllerState flowControllerStateResolve(void)
{
    if (breathControlGet(BREATH_RUN) != 1.0F) {
        return FLOW_CONTROLLER_IDLE;
    }

    switch (phaseControllerStateGet()) {
        case PHASE_INSP_RISE:
            return FLOW_CONTROLLER_INSP_RISE;
        case PHASE_INSP_HOLD:
            return FLOW_CONTROLLER_INSP_HOLD;
        case PHASE_EXP_RELEASE:
            return FLOW_CONTROLLER_EXP_RELEASE;
        case PHASE_EXP_PEEP:
            return FLOW_CONTROLLER_EXP_PEEP;
        case PHASE_IDLE:
        default:
            return FLOW_CONTROLLER_IDLE;
    }
}

/** Initialize controller state on a breath-phase transition. */
static void flowControllerStateEnter(eFlowControllerState state)
{
    if (gFlowControllerState == state) {
        return;
    }

    gFlowControllerState = state;
    if (state == FLOW_CONTROLLER_INSP_RISE) {
        flowControllerInspirationReset();
    } else if (state == FLOW_CONTROLLER_IDLE) {
        flowControllerInspirationReset();
        flowControllerOutputClear();
    }
}

/** Hold the VAC flow required to deliver the target volume within flow time. */
static void flowControllerInspirationProcess(void)
{
    float lBlowerFeedforward;
    float lEffort;
    float lFeedforwardPressure;
    float lFlowReference;
    float lMeasuredFlow;

    if (gFlowControllerState == FLOW_CONTROLLER_INSP_HOLD) {
        /* Inspiratory pause is part of Ti but must not add delivered volume. */
        gFlowBlowerTarget = 0U;
        gFlowExpValveDuty = FLOW_CONTROLLER_EXP_VALVE_CLOSED_DUTY;
        return;
    }

    lFlowReference = flowControllerClamp(phaseControlGet(PHASE_REF_FLOW),
                                         FLOW_CONTROLLER_FLOW_TARGET_MIN,
                                         FLOW_CONTROLLER_FLOW_TARGET_MAX);
    lMeasuredFlow = controlDataGet(INSP_FLOW_FILTERED) * FLOW_CONTROLLER_FLOW_INPUT_SCALE;
    if (pidUpdate(&gFlowPid, lFlowReference, lMeasuredFlow, &lEffort) != PID_STATUS_OK) {
        flowControllerOutputClear();
        return;
    }

    lFeedforwardPressure = breathControlGet(BREATH_PEEP_PRESSURE) +
                           (FLOW_CONTROLLER_FLOW_FF_LINEAR * lFlowReference) +
                           (FLOW_CONTROLLER_FLOW_FF_QUADRATIC * lFlowReference * lFlowReference);
    if (calibtransPrsSpeed(lFeedforwardPressure, &lBlowerFeedforward) !=
        CALIBTRANS_STATUS_OK) {
        flowControllerOutputClear();
        return;
    }

    lEffort = (lBlowerFeedforward * 10.0F) +
              (lEffort * FLOW_CONTROLLER_BLOWER_SPEED_SCALE);
    lEffort = flowControllerClamp(lEffort,
                                  0.0F,
                                  (float)FLOW_CONTROLLER_BLOWER_SPEED_SCALE);
    gFlowBlowerTarget = (uint16_t)lEffort;
    gFlowExpValveDuty = FLOW_CONTROLLER_EXP_VALVE_CLOSED_DUTY;
}

/** Reuse the pressure controller's release and PEEP loops during expiration. */
static void flowControllerExpirationProcess(void)
{
    pressureControllerProcess();
    gFlowBlowerTarget = pressureControllerBlowerTargetGet();
    gFlowExpValveDuty = pressureControllerExpValveDutyGet();
}

void flowControllerInit(void)
{
    int8_t lFlowStatus;

    lFlowStatus = pidInit(&gFlowPid,
                          FLOW_CONTROLLER_FLOW_KP,
                          FLOW_CONTROLLER_FLOW_KI,
                          FLOW_CONTROLLER_FLOW_KD,
                          FLOW_CONTROLLER_SAMPLE_PERIOD_S,
                          FLOW_CONTROLLER_EFFORT_MIN,
                          FLOW_CONTROLLER_EFFORT_MAX);
    gFlowControllerReady = (uint8_t)(lFlowStatus == PID_STATUS_OK);
    gFlowControllerState = FLOW_CONTROLLER_IDLE;
    flowControllerOutputClear();
}

void flowControllerProcess(void)
{
    eFlowControllerState lState;

    if (gFlowControllerReady == 0U) {
        flowControllerOutputClear();
        return;
    }

    lState = flowControllerStateResolve();
    flowControllerStateEnter(lState);

    switch (gFlowControllerState) {
        case FLOW_CONTROLLER_INSP_RISE:
        case FLOW_CONTROLLER_INSP_HOLD:
            flowControllerInspirationProcess();
            break;
        case FLOW_CONTROLLER_EXP_RELEASE:
        case FLOW_CONTROLLER_EXP_PEEP:
            flowControllerExpirationProcess();
            break;
        case FLOW_CONTROLLER_IDLE:
        default:
            flowControllerOutputClear();
            break;
    }
}

uint16_t flowControllerBlowerTargetGet(void)
{
    return gFlowBlowerTarget;
}

uint8_t flowControllerExpValveDutyGet(void)
{
    return gFlowExpValveDuty;
}

/**************************End of file********************************/
