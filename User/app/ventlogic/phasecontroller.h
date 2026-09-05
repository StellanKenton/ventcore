/************************************************************************************
* @file     : phasecontroller.h
* @brief    : Breath plan phase executor interface.
* @details  : Executes one scheduler-selected plan without inspecting vent modes.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTLOGIC_PHASECONTROLLER_H
#define USER_APP_VENTLOGIC_PHASECONTROLLER_H

#include <stdint.h>

#include "breathscheduler.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PHASE_CONTROL_SUCCESS             1
#define PHASE_CONTROL_ERROR_PARAM         (-1)
#define PHASE_CONTROL_ERROR_STATE         (-2)
#define PHASE_PRESSURE_FALL_TIME_MS        168U
#define PHASE_COMPENSATION_TIME_MS          60U
#define PHASE_COMPENSATION_INSP_FLOW_MAX     0.1F
#define PHASE_FLOW_RISE_HALF_TIME_MS         26U
#define PHASE_FLOW_RISE_NINETY_TIME_MS       85U
#define PHASE_FLOW_RISE_FULL_TIME_MS        170U
#define PHASE_FLOW_RISE_HALF_RATIO            0.5F
#define PHASE_FLOW_RISE_NINETY_RATIO          0.9F

typedef enum {
    PHASE_NONE = 0,
    PHASE_REF_PRESSURE,
    PHASE_REF_FLOW,
    PHASE_COUNT,
} ePhaseControlType;

typedef enum {
    PHASE_IDLE = 0,
    PHASE_COMPEN,
    PHASE_INSP,
    PHASE_EXP,
} ePhaseControllerState;

typedef struct stPhaseController {
    ePhaseControllerState runState;
    uint32_t inspirationStartedMs;
    uint32_t expirationStartedMs;
    uint32_t runSequence;
    uint32_t compensationStartedMs;
    float compensationFlowSum;
    uint16_t compensationSampleCount;
    eBreathCycleReason cycleReason;
    uint8_t planValid;
    uint8_t breathStarted;
    uint8_t expirationCaptureComplete;
    uint8_t volumePauseActive;
    stBreathPlan activePlan;
} stPhaseController;

/** Initialize the phase executor. */
void phaseControllerInit(void);

/** Set a phase reference value selected by type. */
int8_t phaseControlSet(ePhaseControlType type, float value);

/** Get a phase reference value selected by type. */
float phaseControlGet(ePhaseControlType type);

/** Get the current breath phase. */
ePhaseControllerState phaseControllerStateGet(void);

/** Return the volume-pause state published by the current VentTask phase update. */
uint8_t phaseControllerVolumePauseActiveGet(void);

/** Copy the immutable plan currently executed by the phase controller. */
int8_t phaseControllerActivePlanGet(stBreathPlan *plan);

/** Start a patient-triggered breath after the plan's expiratory lock time. */
int8_t phaseControllerTrigger(eBreathTriggerReason triggerReason, uint32_t nowMs);

/** End a flow-cycled spontaneous inspiration and begin expiration. */
int8_t phaseControllerCycle(eBreathCycleReason cycleReason, uint32_t nowMs);

/** Notify the phase executor that expiration capture is stable. */
int8_t phaseControllerExpirationCaptureNotify(void);

/** Return whether expiration has reached stable PEEP control. */
uint8_t phaseControllerExpirationReadyGet(void);

/** Return the reason recorded when the active inspiration ended. */
eBreathCycleReason phaseControllerCycleReasonGet(void);

/** Execute the active breath plan using a monotonic millisecond tick. */
void phaseControllerProcess(uint32_t nowMs);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTLOGIC_PHASECONTROLLER_H */
/**************************End of file********************************/
