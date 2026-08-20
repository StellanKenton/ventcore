/************************************************************************************
* @file     : phasecontroller.c
* @brief    : Breath phase controller.
* @details  : Provides phase controller initialization and periodic processing.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "phasecontroller.h"
#include "breathscheduler.h"

stPhaseController gPhaseController;

void phaseControllerInit(void)
{
    gPhaseController.runState = PHASE_IDLE;
}

void phaseControllerProcess(void)
{
    switch (gPhaseController.runState)
    {
        case PHASE_IDLE:
            break;
        case PHASE_INSP_RISE:
            break;
        case PHASE_INSP_HOLD:
            break;
        case PHASE_EXP_RELEASE:
            break;
        case PHASE_EXP_PEEP:
            break;
        default:
            gPhaseController.runState = PHASE_IDLE;
            break;
    }
    if(breathControlGet(BREATH_RUN, &gPhaseController.runCMD)){
        
    }
    
}

/**************************End of file********************************/
