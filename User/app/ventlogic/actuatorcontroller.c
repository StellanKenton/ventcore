/************************************************************************************
* @file     : actuatorcontroller.c
* @brief    : Breath actuator controller.
* @details  : Provides actuator controller initialization and periodic processing.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "actuatorcontroller.h"
#include "blower_vcm.h"
#include "breathscheduler.h"
#include "dvalve.h"
#include "fio2controller.h"
#include "flowcontroller.h"
#include "pressurecontroller.h"
#include "rtos.h"

static bool gActuatorControllerBlowerManualActive = false;
static eBlowerVcmControlMode gActuatorControllerBlowerManualMode = BLOWER_CTRL_INIT;
static uint16_t gActuatorControllerBlowerManualTarget = 0U;
static uint8_t gActuatorControllerBlowerManualSaturation = 0U;
static bool gActuatorControllerExpValveManualActive = false;
static uint8_t gActuatorControllerExpValveManualDuty = 0U;

void actuatorControllerInit(void)
{
    actuatorControllerBlowerManualClear();
    actuatorControllerExpValveManualClear();
    pressureControllerInit();
    flowControllerInit();
    fio2ControllerInit();
    (void)dvalveDutySet(DVALVE_IDX_RELIEF,ACTUATOR_CONTROLLER_RELIEF_CLOSED_DUTY);
}

void actuatorControllerProcess(void)
{
    bool lBlowerManualActive;
    eBlowerVcmControlMode lBlowerMode;
    uint16_t lBlowerTarget;
    uint8_t lBlowerSaturation;
    bool lExpValveManualActive;
    uint8_t lExpValveDuty;

    pressureControllerProcess();
    flowControllerProcess();
    fio2ControllerProcess();

    repRtosEnterCritical();
    lBlowerManualActive = gActuatorControllerBlowerManualActive;
    lBlowerMode = gActuatorControllerBlowerManualMode;
    lBlowerTarget = gActuatorControllerBlowerManualTarget;
    lBlowerSaturation = gActuatorControllerBlowerManualSaturation;
    lExpValveManualActive = gActuatorControllerExpValveManualActive;
    lExpValveDuty = gActuatorControllerExpValveManualDuty;
    repRtosExitCritical();

    if (!lBlowerManualActive) {
        lBlowerMode = BLOWER_CTRL_SPEED;
        lBlowerTarget = pressureControllerBlowerTargetGet();
        lBlowerSaturation = 0U;
    }
    if (!lExpValveManualActive) {
        lExpValveDuty = pressureControllerExpValveDutyGet();
    }
    (void)blowerVcmSendControl(lBlowerMode, lBlowerTarget, lBlowerSaturation);
    (void)dvalveDutySet(DVALVE_IDX_EXP, lExpValveDuty);

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
