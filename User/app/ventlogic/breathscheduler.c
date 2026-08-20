/************************************************************************************
* @file     : breathscheduler.c
* @brief    : Breath phase scheduler.
* @details  : Implements a tick-driven inspiration and expiration state machine.
* @author   :
* @date     :
* @version  :
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "breathscheduler.h"

#include <stdbool.h>

stBreathInfo gBreathInfo;

int8_t breathSchedulerInit(void)
{
    gBreathInfo.runState = false;
    gBreathInfo.currentMode = VENT_MD_IDLE;
    return 0;
}

void breathSchedulerProcess(void)
{
    
}
/**************************End of file********************************/
