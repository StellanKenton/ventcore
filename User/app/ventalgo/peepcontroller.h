/************************************************************************************
* @file     : peepcontroller.h
* @brief    : Ventilation PEEP controller interface.
* @details  : Declares PEEP controller initialization and periodic processing.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTALGO_PEEPCONTROLLER_H
#define USER_APP_VENTALGO_PEEPCONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the PEEP controller. */
void peepControllerInit(void);

/** Run one PEEP controller processing cycle. */
void peepControllerProcess(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTALGO_PEEPCONTROLLER_H */
/**************************End of file********************************/
