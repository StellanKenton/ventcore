/************************************************************************************
* @file     : phasecontroller.h
* @brief    : Breath phase controller interface.
* @details  : Declares phase controller initialization and periodic processing.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTLOGIC_PHASECONTROLLER_H
#define USER_APP_VENTLOGIC_PHASECONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stPhaseController {
    ePhaseControllerState runState;
    float    runCMD;
    uint16_t inspRiseTimeTicks;
    uint16_t inspHoldTimeTicks;
    uint16_t expReleaseTimeTicks;
    uint16_t expPeepTimeTicks;
} stPhaseController;

typedef enum {
    PHASE_IDLE = 0,
    PHASE_INSP_RISE,
    PHASE_INSP_HOLD,
    PHASE_EXP_RELEASE,
    PHASE_EXP_PEEP,
} ePhaseControllerState;

/** Initialize the phase controller. */
void phaseControllerInit(void);

/** Run one phase controller processing cycle. */
void phaseControllerProcess(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTLOGIC_PHASECONTROLLER_H */
/**************************End of file********************************/
