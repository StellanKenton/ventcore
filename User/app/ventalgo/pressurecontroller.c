/************************************************************************************
* @file     : pressurecontroller.c
* @brief    : Ventilation pressure controller.
* @details  : Provides pressure controller initialization and periodic processing.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "pressurecontroller.h"

#include <stddef.h>

#include "breathscheduler.h"
#include "controldata.h"
#include "phasecontroller.h"
#include "pid.h"
#include "calibtrans.h"

static stPid gPressureOuterPid;
static stPid gPressureInnerPid;
static stPid gPressurePeepPid;
static uint16_t gPressureBlowerTarget;
static uint8_t gPressureExpValveDuty;
static uint8_t gPressureControllerReady;
static ePressureControllerState gPressureControllerState;
static stPressureControllerDiagnostic gPressureDiagnostic;

/** Clamp a pressure-controller value to a configured range. */
static float pressureControllerClamp(float value, float minimum, float maximum) {
    if (value > maximum) {
        return maximum;
    }
    if (value < minimum) {
        return minimum;
    }
    return value;
}

/** Clear the latest inspiratory-control diagnostic values. */
static void pressureControllerDiagnosticClear(void) {
    gPressureDiagnostic.inspTarget = 0.0F;
    gPressureDiagnostic.flowCompensation = 0.0F;
    gPressureDiagnostic.patientCorrection = 0.0F;
    gPressureDiagnostic.innerEffort = 0.0F;
    gPressureDiagnostic.blowerFeedforward = 0.0F;
}

static void pressureControllerOutputClear(void)
{
    gPressureBlowerTarget = 0U;
    gPressureExpValveDuty = 0U;
    pressureControllerDiagnosticClear();
}

static void pressureControllerPidsReset(void)
{
    (void)pidReset(&gPressureOuterPid);
    (void)pidReset(&gPressureInnerPid);
    (void)pidReset(&gPressurePeepPid);
}

static void pressureControllerEffortApply(float effort)
{
    if (effort >= 0.0F) {
        gPressureBlowerTarget = 0;//(uint16_t)(effort *(float)PRESSURE_CONTROLLER_BLOWER_SPEED_SCALE);
        gPressureExpValveDuty = PRESSURE_CONTROLLER_INSP_EXP_DUTY;
    } else {
        gPressureBlowerTarget = 0U;
        gPressureExpValveDuty = (uint8_t)((float)PRESSURE_CONTROLLER_EXP_VALVE_CLOSED_DUTY + effort);
    }
}

void pressureControllerInit(void)
{
    int8_t lOuterStatus;
    int8_t lInnerStatus;
    int8_t lPeepStatus;

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
    lPeepStatus = pidInit(&gPressurePeepPid,
                          PRESSURE_CONTROLLER_PEEP_KP,
                          PRESSURE_CONTROLLER_PEEP_KI,
                          PRESSURE_CONTROLLER_PEEP_KD,
                          PRESSURE_CONTROLLER_SAMPLE_PERIOD_S,
                          PRESSURE_CONTROLLER_PEEP_EFFORT_MIN,
                          PRESSURE_CONTROLLER_PEEP_EFFORT_MAX);
    gPressureControllerReady = (uint8_t)((lOuterStatus == PID_STATUS_OK) &&
                                         (lInnerStatus == PID_STATUS_OK) &&
                                         (lPeepStatus == PID_STATUS_OK));
    gPressureControllerState = PRESSURE_CONTROLLER_IDLE;
    pressureControllerOutputClear();
}

/** Calculate the inspiratory-pressure target through the patient-pressure outer loop. */
static int8_t pressureControllerInspirationOuterLoopProcess(float *inspTarget)
{
    float lFlow;
    float lFlowCompensation;
    float lInspCorrection;
    float lPatientReference;
    int8_t lStatus;

    if (inspTarget == NULL) {
        return PID_ERROR_PARAM;
    }

    lPatientReference = phaseControlGet(PHASE_REF_PRESSURE);
    lFlow = controlDataGet(INSP_FLOW_FILTERED) * PRESSURE_CONTROLLER_FLOW_INPUT_SCALE;
    lFlow = pressureControllerClamp(lFlow, 0.0F, PRESSURE_CONTROLLER_FLOW_INPUT_MAX);
    lFlowCompensation = (PRESSURE_CONTROLLER_FLOW_FF_LINEAR * lFlow) +
                        (PRESSURE_CONTROLLER_FLOW_FF_QUADRATIC * lFlow * lFlow);
    lFlowCompensation = pressureControllerClamp(lFlowCompensation,
                                                0.0F,
                                                PRESSURE_CONTROLLER_FLOW_FF_MAX);

    lStatus = pidUpdate(&gPressureOuterPid,
                        lPatientReference,
                        controlDataGet(PAT_REAL_PRS),
                        &lInspCorrection);
    if (lStatus != PID_STATUS_OK) {
        return lStatus;
    }

    *inspTarget = pressureControllerClamp(lPatientReference + lFlowCompensation + lInspCorrection,
                                          PRESSURE_CONTROLLER_INSP_TARGET_MIN,
                                          PRESSURE_CONTROLLER_INSP_TARGET_MAX);
    gPressureDiagnostic.inspTarget = *inspTarget;
    gPressureDiagnostic.flowCompensation = lFlowCompensation;
    gPressureDiagnostic.patientCorrection = lInspCorrection;
    return PID_STATUS_OK;
}

/** Calculate the actuator effort through the inspiratory-pressure inner loop. */
static int8_t pressureControllerInspirationInnerLoopProcess(float inspTarget, float *effort)
{
    return pidUpdate(&gPressureInnerPid,
                     inspTarget,
                     controlDataGet(INSP_REAL_PRS),
                     effort);
}

static void pressureControllerInspirationProcess(void)
{
    float lInspTarget = 0.0F;
    float lEffort;
    float lBlowerFeedforward;

    if (pressureControllerInspirationOuterLoopProcess(&lInspTarget) != PID_STATUS_OK) {
        pressureControllerOutputClear();
        return;
    }

    if (pressureControllerInspirationInnerLoopProcess(lInspTarget, &lEffort) != PID_STATUS_OK) {
        pressureControllerOutputClear();
        return;
    }
    if (calibtransPrsSpeed(lInspTarget, &lBlowerFeedforward) != CALIBTRANS_STATUS_OK) {
        pressureControllerOutputClear();
        return;
    }
    gPressureDiagnostic.innerEffort = lEffort;
    gPressureDiagnostic.blowerFeedforward = lBlowerFeedforward;
    lEffort = (lEffort * PRESSURE_CONTROLLER_BLOWER_SPEED_SCALE) +
              (lBlowerFeedforward * 10.0F);

    if (lEffort > PRESSURE_CONTROLLER_BLOWER_SPEED_SCALE) {
        lEffort = PRESSURE_CONTROLLER_BLOWER_SPEED_SCALE;
    }

    /* Inspiration never vents pressure through EXP; negative effort only stops the blower. */
    gPressureBlowerTarget = (lEffort > 0.0F) ? (uint16_t)lEffort: 0U;
    gPressureExpValveDuty = PRESSURE_CONTROLLER_EXP_VALVE_CLOSED_DUTY;
}

/** Proportionally vent excess patient pressure during the inspiratory hold only. */
static void pressureControllerHoldReliefProcess(void)
{
    float lExcessPressure;
    float lValveOpening;

    lExcessPressure = controlDataGet(PAT_REAL_PRS) - phaseControlGet(PHASE_REF_PRESSURE);
    if (lExcessPressure <= PRESSURE_CONTROLLER_HOLD_RELIEF_DEADBAND) {
        return;
    }

    lValveOpening = (lExcessPressure - PRESSURE_CONTROLLER_HOLD_RELIEF_DEADBAND) *
                    PRESSURE_CONTROLLER_HOLD_RELIEF_GAIN;
    lValveOpening = pressureControllerClamp(lValveOpening,
                                             0.0F,
                                             PRESSURE_CONTROLLER_HOLD_RELIEF_MAX_OPENING);
    gPressureExpValveDuty = (uint8_t)((float)PRESSURE_CONTROLLER_EXP_VALVE_CLOSED_DUTY -
                                      lValveOpening);
}

static void pressureControllerPeepProcess(void)
{
    float lEffort;
    float lValveDuty;

    if (pidUpdate(&gPressurePeepPid,
                  phaseControlGet(PHASE_REF_PRESSURE),
                  controlDataGet(PAT_REAL_PRS),
                  &lEffort) != PID_STATUS_OK) {
        pressureControllerOutputClear();
        return;
    }

    if (lEffort >= 0.0F) {
        lEffort += 25.0F; /* Add a feedforward term to keep the blower spooled during PEEP. */
        if (lEffort > PRESSURE_CONTROLLER_PEEP_EFFORT_MAX) {
            lEffort = PRESSURE_CONTROLLER_PEEP_EFFORT_MAX;
        }
        pressureControllerEffortApply(lEffort);
        if (gPressureBlowerTarget < PRESSURE_CONTROLLER_PEEP_SPIN_BLOWER_TARGET) {
            gPressureBlowerTarget = PRESSURE_CONTROLLER_PEEP_SPIN_BLOWER_TARGET;
        }
        return;
    }

    lValveDuty = (float)PRESSURE_CONTROLLER_EXP_VALVE_CLOSED_DUTY +
                 (lEffort * PRESSURE_CONTROLLER_PEEP_EXP_GAIN);
    if (lValveDuty < (float)PRESSURE_CONTROLLER_EXP_VALVE_OPEN_DUTY) {
        lValveDuty = (float)PRESSURE_CONTROLLER_EXP_VALVE_OPEN_DUTY;
    }
    /* Keep the blower spooled during PEEP and balance its flow with EXP. */
    gPressureBlowerTarget = PRESSURE_CONTROLLER_PEEP_SPIN_BLOWER_TARGET;
    gPressureExpValveDuty = (uint8_t)lValveDuty;
}

static ePressureControllerState pressureControllerStateResolve(void)
{
    if (breathControlGet(BREATH_RUN) != 1.0F) {
        return PRESSURE_CONTROLLER_IDLE;
    }

    switch (phaseControllerStateGet()) {
        case PHASE_INSP_RISE:
            return PRESSURE_CONTROLLER_INSP_RISE;
        case PHASE_INSP_HOLD:
            return PRESSURE_CONTROLLER_INSP_HOLD;
        case PHASE_EXP_RELEASE:
            return PRESSURE_CONTROLLER_EXP_RELEASE;
        case PHASE_EXP_PEEP:
            return PRESSURE_CONTROLLER_EXP_PEEP;
        case PHASE_IDLE:
        default:
            return PRESSURE_CONTROLLER_IDLE;
    }
}

static void pressureControllerStateEnter(ePressureControllerState state)
{
    if(gPressureControllerState == state) {
        return;
    }
    gPressureControllerState = state;
    switch (state) {
        case PRESSURE_CONTROLLER_INSP_RISE:
            (void)pidReset(&gPressureOuterPid);
            (void)pidReset(&gPressureInnerPid);
            break;
        case PRESSURE_CONTROLLER_EXP_RELEASE:
            pressureControllerPidsReset();
            pressureControllerDiagnosticClear();
            break;
        case PRESSURE_CONTROLLER_EXP_PEEP:
            (void)pidReset(&gPressurePeepPid);
            break;
        case PRESSURE_CONTROLLER_IDLE:
            pressureControllerPidsReset();
            break;
        case PRESSURE_CONTROLLER_INSP_HOLD:
        default:
            break;
    }
}

void pressureControllerProcess(void)
{
    ePressureControllerState lState;

    if (gPressureControllerReady == 0U) {
        pressureControllerOutputClear();
        return;
    }

    lState = pressureControllerStateResolve();
    if (lState != gPressureControllerState) {
        pressureControllerStateEnter(lState);
    }

    switch (gPressureControllerState) {
        case PRESSURE_CONTROLLER_INSP_RISE:
            pressureControllerInspirationProcess();
            break;
        case PRESSURE_CONTROLLER_INSP_HOLD:
            pressureControllerInspirationProcess();
            pressureControllerHoldReliefProcess();
            break;
        case PRESSURE_CONTROLLER_EXP_RELEASE:
            gPressureBlowerTarget = 0U;
            gPressureExpValveDuty = PRESSURE_CONTROLLER_EXP_RELEASE_DUTY;
            break;
        case PRESSURE_CONTROLLER_EXP_PEEP:
            pressureControllerPeepProcess();
            break;
        case PRESSURE_CONTROLLER_IDLE:
        default:
            pressureControllerOutputClear();
            break;
    }
}

uint16_t pressureControllerBlowerTargetGet(void)
{
    return gPressureBlowerTarget;
}

uint8_t pressureControllerExpValveDutyGet(void)
{
    return gPressureExpValveDuty;
}

void pressureControllerDiagnosticGet(stPressureControllerDiagnostic *diagnostic) {
    if (diagnostic == NULL) {
        return;
    }
    *diagnostic = gPressureDiagnostic;
}

/**************************End of file********************************/
