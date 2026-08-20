/************************************************************************************
* @file     : breathscheduler.h
* @brief    : Breath phase scheduler interface.
* @details  : Declares the lightweight inspiration and expiration state machine.
* @author   :
* @date     :
* @version  :
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTLOGIC_BREATHSCHEDULER_H
#define USER_APP_VENTLOGIC_BREATHSCHEDULER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif






/* Configure the scheduler and leave it idle. */
int8_t breathSchedulerInit(void);

/* Advance the state machine using the current monotonic tick. */
void breathSchedulerProcess(void);


#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTLOGIC_BREATHSCHEDULER_H */
/**************************End of file********************************/
