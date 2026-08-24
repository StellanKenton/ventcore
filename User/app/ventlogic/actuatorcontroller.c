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

void actuatorControllerInit(void)
{
    pressureControllerInit();
    flowControllerInit();
    fio2ControllerInit();
    (void)dvalveDutySet(DVALVE_IDX_RELIEF,ACTUATOR_CONTROLLER_RELIEF_CLOSED_DUTY);
}

void actuatorControllerProcess(void)
{
    pressureControllerProcess();
    flowControllerProcess();
    fio2ControllerProcess();

    (void)blowerVcmSendControl(BLOWER_CTRL_SPEED,pressureControllerBlowerTargetGet(),0U);
    (void)dvalveDutySet(DVALVE_IDX_EXP,pressureControllerExpValveDutyGet());

}

/**************************End of file********************************/
