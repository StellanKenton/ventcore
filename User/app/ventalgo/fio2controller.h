/************************************************************************************
* @file     : fio2controller.h
* @brief    : Ventilation FiO2 controller interface.
* @details  : Adds an oxygen-valve proposal to a unified actuator request.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTALGO_FIO2CONTROLLER_H
#define USER_APP_VENTALGO_FIO2CONTROLLER_H

#include <stdint.h>

#include "actuatorrequest.h"
#include "breathscheduler.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the FiO2 controller. */
void fio2ControllerInit(void);

/** Add the current FiO2 output to an otherwise valid actuator request. */
int8_t fio2ControllerProcess(const stBreathPlan *plan, stActuatorRequest *request);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTALGO_FIO2CONTROLLER_H */
/**************************End of file********************************/
