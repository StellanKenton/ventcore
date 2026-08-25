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

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PHASE_CONTROL_SUCCESS       1
#define PHASE_CONTROL_ERROR_PARAM  (-1)
#define PHASE_EXP_PEEP_ENTRY_MARGIN        2.0F
#define PHASE_EXP_RELEASE_MAX_TIME_MS 1200U

typedef enum {
    PHASE_NONE = 0,
    PHASE_REF_PRESSURE,
    PHASE_REF_FAST_PRESSURE,
    PHASE_COUNT,
} ePhaseControlType;

typedef enum {
    PHASE_IDLE = 0,
    PHASE_INSP_RISE,
    PHASE_INSP_HOLD,
    PHASE_EXP_RELEASE,
    PHASE_EXP_PEEP,
} ePhaseControllerState;

typedef struct stPhaseController {
    ePhaseControllerState runState;
    uint16_t inspRiseTimeTicks;
    uint16_t inspHoldTimeTicks;
    uint16_t expPeepTimeTicks;
    float inspRiseStartPressure;
} stPhaseController;

/** Initialize the phase controller. */
void phaseControllerInit(void);

/** Set a phase control value selected by type. */
int8_t phaseControlSet(ePhaseControlType type, float value);

/** Get a phase control value selected by type. */
float phaseControlGet(ePhaseControlType type);

/** Get the current breath phase. */
ePhaseControllerState phaseControllerStateGet(void);

/** Run one phase controller processing cycle. */
void phaseControllerProcess(uint8_t tickCount);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTLOGIC_PHASECONTROLLER_H */
/**************************End of file********************************/
