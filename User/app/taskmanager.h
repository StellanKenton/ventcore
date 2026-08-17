/************************************************************************************
* @file     : taskmanager.h
* @brief    : Project task manager.
* @details  : Declares the entry used to create the project task manager.
* @author   :
* @date     :
* @version  :
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_TASK_MANAGER_H
#define USER_APP_TASK_MANAGER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_TASK_STACK_SIZE          256U
#define DEFAULT_TASK_PRIORITY            1U
#define DEFAULT_TASK_INTERVAL_MS         10U

#define VENT_TASK_STACK_SIZE             256U
#define VENT_TASK_PRIORITY               3U
#define VENT_TASK_INTERVAL_MS            10U

#define SENSOR_TASK_STACK_SIZE           256U
#define SENSOR_TASK_PRIORITY             2U
#define SENSOR_TASK_INTERVAL_MS          20U

#define HMI_TASK_STACK_SIZE              256U
#define HMI_TASK_PRIORITY                2U
#define HMI_TASK_INTERVAL_MS             20U

#define ALARM_TASK_STACK_SIZE            256U
#define ALARM_TASK_PRIORITY              3U
#define ALARM_TASK_INTERVAL_MS           10U

bool WorkerTasksRegister(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_TASK_MANAGER_H */
/**************************End of file********************************/
