/************************************************************************************
* @file     : triggerengine.h
* @brief    : Breath trigger engine interface.
* @details  : Declares trigger engine initialization and periodic processing.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTLOGIC_TRIGGERENGINE_H
#define USER_APP_VENTLOGIC_TRIGGERENGINE_H

#include <stdint.h>

#include "phasecontroller.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TRIGGER_ENGINE_BASELINE_GAIN       0.10F
#define TRIGGER_ENGINE_SETTLE_SAMPLES         8U
#define TRIGGER_ENGINE_CONFIRM_SAMPLES        3U

typedef enum {
    TRIGGER_ENGINE_IDLE = 0,
    TRIGGER_ENGINE_SETTLING,
    TRIGGER_ENGINE_ARMED,
} eTriggerEngineState;

typedef struct stTriggerEngine {
    eTriggerEngineState state;
    ePhaseControllerState previousPhase;
    uint32_t planSequence;
    float pressureBaselineCmh2o;
    float flowBaselineLpm;
    uint8_t settleSamples;
    uint8_t confirmSamples;
} stTriggerEngine;

/** Initialize the trigger engine. */
void triggerEngineInit(void);

/** Run one trigger engine processing cycle. */
void triggerEngineProcess(uint32_t nowMs);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTLOGIC_TRIGGERENGINE_H */
/**************************End of file********************************/
