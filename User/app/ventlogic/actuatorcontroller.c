/************************************************************************************
* @file     : actuatorcontroller.c
* @brief    : Breath actuator arbitration.
* @details  : Selects one controller request and remains the only BSP writer.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "actuatorcontroller.h"

#include <stddef.h>

#include "breathscheduler.h"
#include "dvalve.h"
#include "expirationcontroller.h"
#include "fio2controller.h"
#include "flowcontroller.h"
#include "phasecontroller.h"
#include "pressurecontroller.h"
#include "rtos.h"

static bool gActuatorControllerBlowerManualActive = false;
static eBlowerVcmControlMode gActuatorControllerBlowerManualMode = BLOWER_CTRL_INIT;
static uint16_t gActuatorControllerBlowerManualTarget = 0U;
static uint8_t gActuatorControllerBlowerManualSaturation = 0U;
static bool gActuatorControllerExpValveManualActive = false;
static uint8_t gActuatorControllerExpValveManualDuty = 0U;
static stActuatorRequest gActuatorControllerLastRequest;

/** Fill the confirmed idle output used when no controller request is valid. */
static void actuatorControllerSafeRequestSet(stActuatorRequest *request)
{
    request->blowerTarget = 0U;
    request->expiratoryValveDuty = ACTUATOR_CONTROLLER_EXP_SAFE_DUTY;
    request->oxygenValveDuty = ACTUATOR_CONTROLLER_OXYGEN_SAFE_DUTY;
    request->reliefValveDuty = ACTUATOR_CONTROLLER_RELIEF_CLOSED_DUTY;
    request->validMask = ACTUATOR_REQUEST_VALID_BREATH_OUTPUTS;
}

void actuatorControllerInit(void)
{
    actuatorControllerBlowerManualClear();
    actuatorControllerExpValveManualClear();
    actuatorControllerSafeRequestSet(&gActuatorControllerLastRequest);
    pressureControllerInit();
    flowControllerInit();
    expirationControllerInit();
    fio2ControllerInit();
    (void)dvalveDutySet(DVALVE_IDX_RELIEF,
                        ACTUATOR_CONTROLLER_RELIEF_CLOSED_DUTY);
}

/** Select the automatic request for the current phase and active plan. */
static int8_t actuatorControllerAutomaticRequestGet(stActuatorRequest *request)
{
    ePhaseControllerState lPhase;
    stBreathPlan lPlan;
    int8_t lStatus;

    if (request == NULL) {
        return ACTUATOR_REQUEST_ERROR_PARAM;
    }
    if ((breathSchedulerRunningGet() == 0U) ||
        (phaseControllerActivePlanGet(&lPlan) != PHASE_CONTROL_SUCCESS)) {
        actuatorControllerSafeRequestSet(request);
        return ACTUATOR_REQUEST_SUCCESS;
    }

    lPhase = phaseControllerStateGet();
    switch (lPhase) {
        case PHASE_INSP:
            switch (lPlan.breathType) {
                case BREATH_TYPE_MANDATORY_VOLUME:
                    lStatus = flowControllerProcess(&lPlan, request);
                    break;
                case BREATH_TYPE_MANDATORY_PRESSURE:
                case BREATH_TYPE_SPONTANEOUS_PRESSURE_SUPPORT:
                    lStatus = pressureControllerProcess(&lPlan, request);
                    break;
                default:
                    actuatorControllerSafeRequestSet(request);
                    return ACTUATOR_REQUEST_ERROR_STATE;
            }
            break;

        case PHASE_EXP:
            lStatus = expirationControllerProcess(&lPlan,
                                                  &gActuatorControllerLastRequest,
                                                  request);
            break;

        case PHASE_IDLE:
        default:
            actuatorControllerSafeRequestSet(request);
            return ACTUATOR_REQUEST_SUCCESS;
    }

    if ((lStatus != ACTUATOR_REQUEST_SUCCESS) ||
        ((request->validMask & ACTUATOR_REQUEST_VALID_BREATH_OUTPUTS) !=
         ACTUATOR_REQUEST_VALID_BREATH_OUTPUTS)) {
        actuatorControllerSafeRequestSet(request);
        return ACTUATOR_REQUEST_ERROR_STATE;
    }
    if (fio2ControllerProcess(&lPlan, request) != ACTUATOR_REQUEST_SUCCESS) {
        actuatorControllerSafeRequestSet(request);
        return ACTUATOR_REQUEST_ERROR_STATE;
    }
    return ACTUATOR_REQUEST_SUCCESS;
}

void actuatorControllerProcess(void)
{
    bool lBlowerManualActive;
    bool lExpValveManualActive;
    eBlowerVcmControlMode lBlowerMode = BLOWER_CTRL_SPEED;
    stActuatorRequest lRequest;
    uint16_t lBlowerTarget;
    uint8_t lBlowerSaturation = 0U;
    uint8_t lExpValveDuty;

    (void)actuatorControllerAutomaticRequestGet(&lRequest);

    repRtosEnterCritical();
    lBlowerManualActive = gActuatorControllerBlowerManualActive;
    lExpValveManualActive = gActuatorControllerExpValveManualActive;
    lBlowerTarget = lBlowerManualActive ?
                     gActuatorControllerBlowerManualTarget :
                     lRequest.blowerTarget;
    lExpValveDuty = lExpValveManualActive ?
                    gActuatorControllerExpValveManualDuty :
                    lRequest.expiratoryValveDuty;
    if (lBlowerManualActive) {
        lBlowerMode = gActuatorControllerBlowerManualMode;
        lBlowerSaturation = gActuatorControllerBlowerManualSaturation;
    }
    lRequest.blowerTarget = lBlowerTarget;
    lRequest.expiratoryValveDuty = lExpValveDuty;
    lRequest.validMask |= ACTUATOR_REQUEST_VALID_BREATH_OUTPUTS;
    gActuatorControllerLastRequest = lRequest;
    repRtosExitCritical();

    (void)blowerVcmSendControl(lBlowerMode, lBlowerTarget * 10U, lBlowerSaturation);
    (void)dvalveDutySet(DVALVE_IDX_EXP, lRequest.expiratoryValveDuty);
    if ((lRequest.validMask & ACTUATOR_REQUEST_VALID_OXYGEN_VALVE) != 0U) {
        (void)dvalveDutySet(DVALVE_IDX_O2, lRequest.oxygenValveDuty);
    }
    if ((lRequest.validMask & ACTUATOR_REQUEST_VALID_RELIEF_VALVE) != 0U) {
        (void)dvalveDutySet(DVALVE_IDX_RELIEF, lRequest.reliefValveDuty);
    }
}

int8_t actuatorControllerLastRequestGet(stActuatorRequest *request)
{
    if (request == NULL) {
        return ACTUATOR_REQUEST_ERROR_PARAM;
    }
    repRtosEnterCritical();
    *request = gActuatorControllerLastRequest;
    repRtosExitCritical();
    return ACTUATOR_REQUEST_SUCCESS;
}

void actuatorControllerBlowerManualSet(eBlowerVcmControlMode mode,
                                       uint16_t targetValue,
                                       uint8_t vcmSaturation)
{
    repRtosEnterCritical();
    gActuatorControllerBlowerManualMode = mode;
    gActuatorControllerBlowerManualTarget = targetValue;
    gActuatorControllerBlowerManualSaturation = vcmSaturation;
    gActuatorControllerBlowerManualActive = true;
    repRtosExitCritical();
}

void actuatorControllerBlowerManualClear(void)
{
    repRtosEnterCritical();
    gActuatorControllerBlowerManualActive = false;
    repRtosExitCritical();
}

bool actuatorControllerBlowerManualIsActive(void)
{
    bool lActive;

    repRtosEnterCritical();
    lActive = gActuatorControllerBlowerManualActive;
    repRtosExitCritical();
    return lActive;
}

void actuatorControllerExpValveManualSet(uint8_t dutyPercent)
{
    repRtosEnterCritical();
    gActuatorControllerExpValveManualDuty = dutyPercent;
    gActuatorControllerExpValveManualActive = true;
    repRtosExitCritical();
}

void actuatorControllerExpValveManualClear(void)
{
    repRtosEnterCritical();
    gActuatorControllerExpValveManualActive = false;
    repRtosExitCritical();
}

/**************************End of file********************************/
