/************************************************************************************
* @file     : main.c
* @brief    : Application entry point.
* @details  : Starts the task manager and the RTOS scheduler.
* @author   :
* @date     :
* @version  :
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "rtos.h"
#include "taskmanager.h"

int main(void)
{
    if (!taskManagerStart()) {
        for (;;) {
        }
    }

    (void)repRtosSchedulerStart();

    for (;;) {
    }
}

/**************************End of file********************************/
