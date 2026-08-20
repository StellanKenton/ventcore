/************************************************************************************
* @file     : pressurecontroller.h
* @brief    : Ventilation pressure controller interface.
* @details  : Declares pressure controller initialization and periodic processing.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTALGO_PRESSURECONTROLLER_H
#define USER_APP_VENTALGO_PRESSURECONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the pressure controller. */
void pressureControllerInit(void);

/** Run one pressure controller processing cycle. */
void pressureControllerProcess(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTALGO_PRESSURECONTROLLER_H */
/**************************End of file********************************/
