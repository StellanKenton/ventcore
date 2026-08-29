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
#define PHASE_EXP_PEEP_ENTRY_MARGIN       2.0F
#define PHASE_EXP_RELEASE_MAX_TIME_MS     1200U

typedef enum {
    PHASE_NONE = 0,
    PHASE_REF_PRESSURE,
    PHASE_REF_FAST_PRESSURE,
    PHASE_REF_FLOW,
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
    uint32_t stateStartedMs;
    uint32_t inspirationStartedMs;
    uint32_t expirationStartedMs;
    float inspRiseStartPressure;
    eBreathCycleReason cycleReason;
    uint8_t planValid;
    uint8_t breathStarted;
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

/** Copy the immutable plan currently executed by the phase controller. */
int8_t phaseControllerActivePlanGet(stBreathPlan *plan);

/** Start a patient-triggered breath after the plan's expiratory lock time. */
int8_t phaseControllerTrigger(eBreathTriggerReason triggerReason, uint32_t nowMs);

/** End a flow-cycled spontaneous inspiration and begin expiration. */
int8_t phaseControllerCycle(eBreathCycleReason cycleReason, uint32_t nowMs);

/** Return the reason recorded when the active inspiration ended. */
eBreathCycleReason phaseControllerCycleReasonGet(void);

/** Execute the active breath plan using a monotonic millisecond tick. */
void phaseControllerProcess(uint32_t nowMs);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTLOGIC_PHASECONTROLLER_H */
/**************************End of file********************************/
