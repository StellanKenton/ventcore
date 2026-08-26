/************************************************************************************
* @file     : apneaengine.h
* @brief    : Spontaneous ventilation apnea engine interface.
* @details  : Detects missing patient breaths and requests PSV-ST backup breaths.
***********************************************************************************/
#ifndef USER_APP_VENTLOGIC_APNEAENGINE_H
#define USER_APP_VENTLOGIC_APNEAENGINE_H

#include <stdint.h>

#include "phasecontroller.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APNEA_ENGINE_IDLE = 0,
    APNEA_ENGINE_MONITORING,
    APNEA_ENGINE_ALARM,
    APNEA_ENGINE_BACKUP,
} eApneaEngineState;

typedef struct stApneaEngine {
    eApneaEngineState state;
    eVentMode mode;
    ePhaseControllerState previousPhase;
    uint32_t referenceMs;
    uint8_t initialized;
} stApneaEngine;

/** Initialize apnea detection and backup scheduling. */
void apneaEngineInit(void);

/** Detect apnea and request a timed PSV-ST backup breath when configured. */
void apneaEngineProcess(uint32_t nowMs);

/** Return the current apnea monitoring state. */
eApneaEngineState apneaEngineStateGet(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTLOGIC_APNEAENGINE_H */
/**************************End of file********************************/
