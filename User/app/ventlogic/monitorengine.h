/************************************************************************************
* @file     : monitorengine.h
* @brief    : Breath monitor engine interface.
* @details  : Declares monitor engine initialization and periodic processing.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTLOGIC_MONITORENGINE_H
#define USER_APP_VENTLOGIC_MONITORENGINE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MONITOR_ENGINE_SUCCESS              1
#define MONITOR_ENGINE_ERROR_PARAM         (-1)
#define MONITOR_FLOW_DEADBAND_LPM            0.5F
#define MONITOR_FLOW_SAMPLE_VOLUME_ML        0.1F

typedef enum {
    MONITOR_DATA_NONE = 0,
    MONITOR_TIDA_VOL,
    MONITOR_DATA_COUNT,
} eMonitorDataType;

typedef enum {
    MONITOR_STATE_IDLE = 0,
    MONITOR_STATE_INSP_RISE,
    MONITOR_STATE_INSP_HOLD,
    MONITOR_STATE_EXP_RELEASE,
    MONITOR_STATE_EXP_PEEP,
} eMonitorEngineState;

typedef struct stMonitorEngine {
    eMonitorEngineState runState;
} stMonitorEngine;

/** Initialize the monitor engine. */
void monitorEngineInit(void);

/** Get a monitor result selected by type. */
float monitorEngineGet(eMonitorDataType type);

/** Run one monitor engine processing cycle. */
void monitorEngineProcess(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTLOGIC_MONITORENGINE_H */
/**************************End of file********************************/
