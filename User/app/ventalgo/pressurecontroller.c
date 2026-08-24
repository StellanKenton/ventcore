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

#include "breathscheduler.h"
#include "controldata.h"
#include "phasecontroller.h"
#include "pid.h"

static stPid gPressureOuterPid;
static stPid gPressureInnerPid;
static stPid gPressurePeepPid;
static uint16_t gPressureBlowerTarget;
static uint8_t gPressureExpValveDuty;
static uint8_t gPressureControllerReady;
static uint8_t gPressureRiseBoostActive;
static ePressureControllerState gPressureControllerState;

static void pressureControllerOutputClear(void)
{
    gPressureBlowerTarget = 0U;
    gPressureExpValveDuty = 0U;
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
        gPressureBlowerTarget = (uint16_t)(effort *
                                           (float)PRESSURE_CONTROLLER_BLOWER_PWM_SCALE);
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
                          PRESSURE_CONTROLLER_EFFORT_MIN,
                          PRESSURE_CONTROLLER_EFFORT_MAX);
    gPressureControllerReady = (uint8_t)((lOuterStatus == PID_STATUS_OK) &&
                                         (lInnerStatus == PID_STATUS_OK) &&
                                         (lPeepStatus == PID_STATUS_OK));
    gPressureControllerState = PRESSURE_CONTROLLER_IDLE;
    pressureControllerOutputClear();
}

static void pressureControllerInspirationProcess(float referencePressure, float patientPressure)
{
    float lInspPressure;
    float lInspCorrection;
    float lInspTarget;
    float lEffort;

    lInspPressure = controlDataGet(INSP_REAL_PRS);

    /* The patient-pressure loop trims the target of the faster inspiratory-pressure loop. */
    if (pidUpdate(&gPressureOuterPid,
                  referencePressure,
                  patientPressure,
                  &lInspCorrection) != PID_STATUS_OK) {
        pressureControllerOutputClear();
        return;
    }

    lInspTarget = referencePressure + lInspCorrection;
    if (pidUpdate(&gPressureInnerPid,
                  lInspTarget,
                  lInspPressure,
                  &lEffort) != PID_STATUS_OK) {
        pressureControllerOutputClear();
        return;
    }

    lEffort += PRESSURE_CONTROLLER_INSP_FEEDFORWARD;
    if (lEffort > PRESSURE_CONTROLLER_EFFORT_MAX) {
        lEffort = PRESSURE_CONTROLLER_EFFORT_MAX;
    }

    /* Inspiration never vents pressure through EXP; negative effort only stops the blower. */
    gPressureBlowerTarget = (lEffort > 0.0F) ?
                             (uint16_t)(lEffort * (float)PRESSURE_CONTROLLER_BLOWER_PWM_SCALE) : 0U;
    gPressureExpValveDuty = PRESSURE_CONTROLLER_EXP_VALVE_CLOSED_DUTY;
}

static void pressureControllerInspirationRiseProcess(float referencePressure, float patientPressure)
{
    if ((gPressureRiseBoostActive != 0U) &&
        (patientPressure < (referencePressure - PRESSURE_CONTROLLER_RISE_BOOST_EXIT_MARGIN))) {
        gPressureBlowerTarget = PRESSURE_CONTROLLER_RISE_BOOST_BLOWER_TARGET;
        gPressureExpValveDuty = PRESSURE_CONTROLLER_EXP_VALVE_CLOSED_DUTY;
        return;
    }

    if (gPressureRiseBoostActive != 0U) {
        gPressureRiseBoostActive = 0U;
        (void)pidReset(&gPressureOuterPid);
        (void)pidReset(&gPressureInnerPid);
    }
    pressureControllerInspirationProcess(referencePressure, patientPressure);
}

static void pressureControllerPeepProcess(float referencePressure, float patientPressure)
{
    float lEffort;
    float lValveDuty;

    if (pidUpdate(&gPressurePeepPid,
                  referencePressure,
                  patientPressure,
                  &lEffort) != PID_STATUS_OK) {
        pressureControllerOutputClear();
        return;
    }

    if (lEffort >= 0.0F) {
        lEffort += PRESSURE_CONTROLLER_PEEP_FEEDFORWARD;
        if (lEffort > PRESSURE_CONTROLLER_EFFORT_MAX) {
            lEffort = PRESSURE_CONTROLLER_EFFORT_MAX;
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
            gPressureRiseBoostActive = 1U;
            break;
        case PRESSURE_CONTROLLER_EXP_RELEASE:
            pressureControllerPidsReset();
            gPressureRiseBoostActive = 0U;
            break;
        case PRESSURE_CONTROLLER_EXP_PEEP:
            (void)pidReset(&gPressurePeepPid);
            break;
        case PRESSURE_CONTROLLER_IDLE:
            pressureControllerPidsReset();
            gPressureRiseBoostActive = 0U;
            break;
        case PRESSURE_CONTROLLER_INSP_HOLD:
        default:
            break;
    }
}

void pressureControllerProcess(void)
{
    ePressureControllerState lState;
    float lReferencePressure;
    float lPatientPressure;

    if (gPressureControllerReady == 0U) {
        pressureControllerOutputClear();
        return;
    }

    lState = pressureControllerStateResolve();
    if (lState != gPressureControllerState) {
        pressureControllerStateEnter(lState);
    }

    lReferencePressure = phaseControlGet(PHASE_REF_PRESSURE);
    lPatientPressure = controlDataGet(PAT_REAL_PRS);

    switch (gPressureControllerState) {
        case PRESSURE_CONTROLLER_INSP_RISE:
            pressureControllerInspirationRiseProcess(breathControlGet(BREATH_INSP_PRESSURE),
                                                      lPatientPressure);
            break;
        case PRESSURE_CONTROLLER_INSP_HOLD:
            if (gPressureRiseBoostActive != 0U) {
                pressureControllerInspirationRiseProcess(lReferencePressure, lPatientPressure);
            } else {
                pressureControllerInspirationProcess(lReferencePressure, lPatientPressure);
            }
            break;
        case PRESSURE_CONTROLLER_EXP_RELEASE:
            gPressureBlowerTarget = 0U;
            gPressureExpValveDuty = PRESSURE_CONTROLLER_EXP_RELEASE_DUTY;
            break;
        case PRESSURE_CONTROLLER_EXP_PEEP:
            pressureControllerPeepProcess(lReferencePressure, lPatientPressure);
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

/**************************End of file********************************/
