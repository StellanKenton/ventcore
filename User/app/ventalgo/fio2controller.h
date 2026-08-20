/************************************************************************************
* @file     : fio2controller.h
* @brief    : Ventilation FiO2 controller interface.
* @details  : Declares FiO2 controller initialization and periodic processing.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTALGO_FIO2CONTROLLER_H
#define USER_APP_VENTALGO_FIO2CONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the FiO2 controller. */
void fio2ControllerInit(void);

/** Run one FiO2 controller processing cycle. */
void fio2ControllerProcess(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTALGO_FIO2CONTROLLER_H */
/**************************End of file********************************/
