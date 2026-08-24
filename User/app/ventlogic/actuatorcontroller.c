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
#include "fio2controller.h"
#include "flowcontroller.h"
#include "peepcontroller.h"
#include "pressurecontroller.h"

void actuatorControllerInit(void)
{
    pressureControllerInit();
    flowControllerInit();
    fio2ControllerInit();
}

void actuatorControllerProcess(void)
{
    pressureControllerProcess();
    flowControllerProcess();
    fio2ControllerProcess();
}

/**************************End of file********************************/
