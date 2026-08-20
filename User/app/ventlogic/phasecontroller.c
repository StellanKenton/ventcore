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

void phaseControllerProcess(uint8_t tickCount)
{
    switch (gPhaseController.runState)
    {
        case PHASE_IDLE:
            gPhaseController.expPeepTimeTicks = 0;
            gPhaseController.inspHoldTimeTicks = 0;
            gPhaseController.inspRiseTimeTicks = 0;
            break;
        case PHASE_INSP_RISE:
            gPhaseController.inspRiseTimeTicks += tickCount;
            if(gPhaseController.inspRiseTimeTicks >= (uint16_t)breathControlGet(BREATH_INSP_RISE_TIME)){
                gPhaseController.runState = PHASE_INSP_HOLD;
            }
            break;
        case PHASE_INSP_HOLD:
            gPhaseController.inspHoldTimeTicks += tickCount;
            if(gPhaseController.inspHoldTimeTicks >= (uint16_t)breathControlGet(BREATH_INSP_HOLD_TIME)){
                gPhaseController.runState = PHASE_EXP_PEEP;
            }
            break;
        case PHASE_EXP_PEEP:
            gPhaseController.expPeepTimeTicks += tickCount;
            if(gPhaseController.expPeepTimeTicks >= (uint16_t)breathControlGet(BREATH_INSP_PEEP_TIME)){
                gPhaseController.runState = PHASE_IDLE;
            }
            break;
        default:
            gPhaseController.runState = PHASE_IDLE;
            break;
    }

    if(breathControlGet(BREATH_RUN) == 0.0f){
        gPhaseController.runState = PHASE_IDLE;     
    }
    
}

/**************************End of file********************************/
