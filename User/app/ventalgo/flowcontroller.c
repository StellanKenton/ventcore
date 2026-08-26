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
#include "monitorengine.h"
#include "phasecontroller.h"
#include "pid.h"
#include "pressurecontroller.h"

static stPid gFlowVolumePid;
static stPid gFlowInnerPid;
static uint16_t gFlowBlowerTarget;
static uint8_t gFlowExpValveDuty;
static uint8_t gFlowControllerReady;
static eFlowControllerState gFlowControllerState;
static float gFlowTargetVti;

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

/** Reset the inspiratory cascade and its target-volume trajectory. */
static void flowControllerInspirationReset(void)
{
    (void)pidReset(&gFlowVolumePid);
    (void)pidReset(&gFlowInnerPid);
    gFlowTargetVti = 0.0F;
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

/** Run the VTi outer loop and flow inner loop for VAC inspiration. */
static void flowControllerInspirationProcess(void)
{
    float lBlowerFeedforward;
    float lEffort;
    float lFeedforwardPressure;
    float lFlowCorrection;
    float lFlowReference;
    float lFlowTarget;
    float lMeasuredFlow;
    float lTidalVolumeTarget;

    lFlowReference = flowControllerClamp(phaseControlGet(PHASE_REF_FLOW),
                                         FLOW_CONTROLLER_FLOW_TARGET_MIN,
                                         FLOW_CONTROLLER_FLOW_TARGET_MAX);
    lTidalVolumeTarget = breathControlGet(BREATH_TIDAL_VOLUME);
    gFlowTargetVti += breathControlGet(BREATH_FLOW) * FLOW_CONTROLLER_SAMPLE_VOLUME_ML;
    gFlowTargetVti = flowControllerClamp(gFlowTargetVti, 0.0F, lTidalVolumeTarget);

    if (pidUpdate(&gFlowVolumePid,
                  gFlowTargetVti,
                  monitorEngineGet(MONITOR_TIDA_VOL_INSP),
                  &lFlowCorrection) != PID_STATUS_OK) {
        flowControllerOutputClear();
        return;
    }
    lFlowTarget = flowControllerClamp(lFlowReference + lFlowCorrection,
                                      FLOW_CONTROLLER_FLOW_TARGET_MIN,
                                      FLOW_CONTROLLER_FLOW_TARGET_MAX);
    lMeasuredFlow = controlDataGet(INSP_FLOW_FILTERED) * FLOW_CONTROLLER_FLOW_INPUT_SCALE;
    if (pidUpdate(&gFlowInnerPid, lFlowTarget, lMeasuredFlow, &lEffort) != PID_STATUS_OK) {
        flowControllerOutputClear();
        return;
    }

    lFeedforwardPressure = breathControlGet(BREATH_PEEP_PRESSURE) +
                           (FLOW_CONTROLLER_FLOW_FF_LINEAR * lFlowTarget) +
                           (FLOW_CONTROLLER_FLOW_FF_QUADRATIC * lFlowTarget * lFlowTarget);
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
    int8_t lVolumeStatus;

    lVolumeStatus = pidInit(&gFlowVolumePid,
                            FLOW_CONTROLLER_VOLUME_KP,
                            FLOW_CONTROLLER_VOLUME_KI,
                            FLOW_CONTROLLER_VOLUME_KD,
                            FLOW_CONTROLLER_SAMPLE_PERIOD_S,
                            FLOW_CONTROLLER_VOLUME_CORRECTION_MIN,
                            FLOW_CONTROLLER_VOLUME_CORRECTION_MAX);
    lFlowStatus = pidInit(&gFlowInnerPid,
                          FLOW_CONTROLLER_FLOW_KP,
                          FLOW_CONTROLLER_FLOW_KI,
                          FLOW_CONTROLLER_FLOW_KD,
                          FLOW_CONTROLLER_SAMPLE_PERIOD_S,
                          FLOW_CONTROLLER_EFFORT_MIN,
                          FLOW_CONTROLLER_EFFORT_MAX);
    gFlowControllerReady = (uint8_t)((lVolumeStatus == PID_STATUS_OK) &&
                                     (lFlowStatus == PID_STATUS_OK));
    gFlowControllerState = FLOW_CONTROLLER_IDLE;
    gFlowTargetVti = 0.0F;
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
