/************************************************************************************
* @file     : ventalgo.c
* @brief    : Ventilation algorithm manager.
* @details  : Initializes and periodically executes all ventilation controllers.
* @author   :
* @date     : 2026-08-24
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "ventalgo.h"

#include "fio2controller.h"
#include "flowcontroller.h"
#include "peepcontroller.h"
#include "pressurecontroller.h"

void ventAlgoInit(void)
{
    pressureControllerInit();
    flowControllerInit();
    peepControllerInit();
    fio2ControllerInit();
}

void ventAlgoProcess(void)
{
    pressureControllerProcess();
    flowControllerProcess();
    peepControllerProcess();
    fio2ControllerProcess();
}

/**************************End of file********************************/
