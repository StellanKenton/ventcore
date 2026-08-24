/************************************************************************************
* @file     : actuatorcontroller.h
* @brief    : Breath actuator controller interface.
* @details  : Declares actuator controller initialization and periodic processing.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTLOGIC_ACTUATORCONTROLLER_H
#define USER_APP_VENTLOGIC_ACTUATORCONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

#define ACTUATOR_CONTROLLER_RELIEF_OPEN_DUTY     0U
#define ACTUATOR_CONTROLLER_RELIEF_CLOSED_DUTY   100U

/** Initialize the actuator controller. */
void actuatorControllerInit(void);

/** Run one actuator controller processing cycle. */
void actuatorControllerProcess(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTLOGIC_ACTUATORCONTROLLER_H */
/**************************End of file********************************/
