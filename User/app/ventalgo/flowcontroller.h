/************************************************************************************
* @file     : flowcontroller.h
* @brief    : Ventilation flow controller interface.
* @details  : Declares flow controller initialization and periodic processing.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTALGO_FLOWCONTROLLER_H
#define USER_APP_VENTALGO_FLOWCONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the flow controller. */
void flowControllerInit(void);

/** Run one flow controller processing cycle. */
void flowControllerProcess(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTALGO_FLOWCONTROLLER_H */
/**************************End of file********************************/
