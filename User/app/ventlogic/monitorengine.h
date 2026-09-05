/************************************************************************************
* @file     : monitorengine.h
* @brief    : Breath monitor and completed-result interface.
* @details  : Integrates breath signals and publishes one immutable result per cycle.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTLOGIC_MONITORENGINE_H
#define USER_APP_VENTLOGIC_MONITORENGINE_H

#include <stdint.h>

#include "breathscheduler.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MONITOR_ENGINE_SUCCESS                  1
#define MONITOR_ENGINE_ERROR_PARAM             (-1)
#define MONITOR_ENGINE_ERROR_STATE             (-2)
#define MONITOR_FLOW_DEADBAND_LPM                0.5F
#define MONITOR_FLOW_SAMPLE_VOLUME_ML            0.1F
#define MONITOR_PLATEAU_END_WINDOW_MS             100U
#define MONITOR_LEAK_COEFFICIENT_MIN              (-2.0F)
#define MONITOR_LEAK_COEFFICIENT_MAX               50.0F
#define MONITOR_LEAK_PRESSURE_SUM_MIN               0.001F
#define BREATH_RESULT_VALID_COMPLETE             (1UL << 0)
#define BREATH_RESULT_VALID_VTI                  (1UL << 1)
#define BREATH_RESULT_VALID_VTE                  (1UL << 2)
#define BREATH_RESULT_VALID_PPEAK                (1UL << 3)
#define BREATH_RESULT_VALID_PEEP                 (1UL << 4)
#define BREATH_RESULT_VALID_CYCLE_TIME           (1UL << 5)
#define BREATH_RESULT_VALID_INSPIRATORY_TIME     (1UL << 6)
#define BREATH_RESULT_VALID_PEAK_INSP_FLOW       (1UL << 7)
#define BREATH_RESULT_VALID_PLATEAU_PRESSURE     (1UL << 8)

typedef enum {
    MONITOR_DATA_NONE = 0,
    MONITOR_TIDA_VOL,
    MONITOR_TIDA_VOL_INSP,
    MONITOR_TIDA_VOL_EXP,
    MONITOR_PLATEAU_PRS,
    MONITOR_LEAK_COEFFICIENT,
    MONITOR_LEAK_FLOW,
    MONITOR_DATA_COUNT,
} eMonitorDataType;

typedef enum {
    MONITOR_STATE_IDLE = 0,
    MONITOR_STATE_INSP,
    MONITOR_STATE_EXP,
} eMonitorEngineState;

typedef struct stBreathResult {
    uint32_t sequence;
    eVentMode mode;
    eBreathType breathType;
    eBreathTriggerReason triggerReason;
    float vtiMl;
    float vteMl;
    float ppeakCmh2o;
    float plateauPressureCmh2o;
    float peepCmh2o;
    float peakInspiratoryFlowLpm;
    eBreathCycleReason cycleReason;
    uint32_t inspiratoryTimeMs;
    uint32_t cycleTimeMs;
    uint32_t validMask;
} stBreathResult;

typedef struct stMonitorEngine {
    eMonitorEngineState runState;
    uint32_t breathStartedMs;
    uint32_t inspiratoryTimeMs;
    float peakPressureCmh2o;
    float peakInspiratoryFlowLpm;
    float plateauPressureSumCmh2o;
    uint32_t plateauPressureSampleCount;
    float leakFlowSumLpm;
    float leakPressureRootSum;
    eBreathCycleReason cycleReason;
    uint8_t breathActive;
    stBreathPlan breathPlan;
} stMonitorEngine;

/** Initialize waveform monitoring and completed-breath publication. */
void monitorEngineInit(void);

/** Get a continuously updated monitor value selected by type. */
float monitorEngineGet(eMonitorDataType type);

/** Copy the latest completed breath result. */
int8_t monitorEngineBreathResultGet(stBreathResult *result);

/** Update monitoring and publish a result when the next inspiration begins. */
void monitorEngineProcess(uint32_t nowMs);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTLOGIC_MONITORENGINE_H */
/**************************End of file********************************/
