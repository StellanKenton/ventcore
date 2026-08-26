/************************************************************************************
* @file     : cycleengine.h
* @brief    : Breath cycle engine interface.
* @details  : Declares cycle engine initialization and periodic processing.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTLOGIC_CYCLEENGINE_H
#define USER_APP_VENTLOGIC_CYCLEENGINE_H

#include <stdint.h>

#include "phasecontroller.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CYCLE_ENGINE_CONFIRM_SAMPLES             3U
#define CYCLE_ENGINE_MINIMUM_PEAK_FLOW_LPM       1.0F

typedef enum {
    CYCLE_ENGINE_IDLE = 0,
    CYCLE_ENGINE_TRACKING,
} eCycleEngineState;

typedef struct stCycleEngine {
    eCycleEngineState state;
    ePhaseControllerState previousPhase;
    uint32_t planSequence;
    uint32_t inspirationStartedMs;
    float peakInspiratoryFlowLpm;
    uint8_t confirmSamples;
} stCycleEngine;

/** Initialize the cycle engine. */
void cycleEngineInit(void);

/** Run one cycle engine processing cycle. */
void cycleEngineProcess(uint32_t nowMs);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTLOGIC_CYCLEENGINE_H */
/**************************End of file********************************/
