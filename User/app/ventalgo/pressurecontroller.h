/************************************************************************************
* @file     : pressurecontroller.h
* @brief    : Inspiratory pressure controller interface.
* @details  : Produces a unified actuator request for pressure inspirations.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTALGO_PRESSURECONTROLLER_H
#define USER_APP_VENTALGO_PRESSURECONTROLLER_H

#include <stdint.h>

#include "actuatorrequest.h"
#include "breathscheduler.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRESSURE_CONTROLLER_SAMPLE_PERIOD_S          0.006F
#define PRESSURE_CONTROLLER_SAMPLE_PERIOD_MS          6U
#define PRESSURE_CONTROLLER_OUTER_KP                 0.5F
#define PRESSURE_CONTROLLER_OUTER_HOLD_KP            0.4F
#define PRESSURE_CONTROLLER_OUTER_HOLD_KI            0.4F
#define PRESSURE_CONTROLLER_OUTER_KI                 0.0F
#define PRESSURE_CONTROLLER_OUTER_KD                 0.0F
#define PRESSURE_CONTROLLER_OUTER_CORRECTION_MIN    (-4.0F)
#define PRESSURE_CONTROLLER_OUTER_CORRECTION_MAX    10.0F
#define PRESSURE_CONTROLLER_INNER_KP                 0.015F
#define PRESSURE_CONTROLLER_INNER_KI                 0.0F
#define PRESSURE_CONTROLLER_INNER_KD                 0.0F
#define PRESSURE_CONTROLLER_EFFORT_MIN              (-1.0F)
#define PRESSURE_CONTROLLER_EFFORT_MAX               1.0F
#define PRESSURE_CONTROLLER_FLOW_INPUT_SCALE         0.5F
#define PRESSURE_CONTROLLER_FLOW_INPUT_MAX          35.0F
#define PRESSURE_CONTROLLER_FLOW_FF_LINEAR           0.1572F
#define PRESSURE_CONTROLLER_FLOW_FF_QUADRATIC        0.004013F
#define PRESSURE_CONTROLLER_FLOW_FF_MAX               9.0F
#define PRESSURE_CONTROLLER_FLOW_FF_DELTA_RATIO       0.3F
#define PRESSURE_CONTROLLER_FLOW_FF_RISE_FILTER_GAIN  0.15F
#define PRESSURE_CONTROLLER_FLOW_FF_HOLD_FILTER_GAIN  0.35F
#define PRESSURE_CONTROLLER_RISE_LEAD_DELTA_DEADBAND  10.0F
#define PRESSURE_CONTROLLER_RISE_LEAD_RATIO            0.2F
#define PRESSURE_CONTROLLER_REFERENCE_SLEW_MAX        100.0F
#define PRESSURE_CONTROLLER_INSP_TARGET_MIN           0.0F
#define PRESSURE_CONTROLLER_INSP_TARGET_MAX         100.0F
#define PRESSURE_CONTROLLER_EXP_VALVE_CLOSED_DUTY   100U
#define PRESSURE_CONTROLLER_HOLD_RELIEF_DEADBAND      0.3F
#define PRESSURE_CONTROLLER_HOLD_RELIEF_GAIN          4.0F
#define PRESSURE_CONTROLLER_HOLD_RELIEF_MAX_OPENING  10.0F
#define PRESSURE_CONTROLLER_BLOWER_SPEED_SCALE       800U
#define PRESSURE_CONTROLLER_RELIEF_CLOSED_DUTY       100U

typedef enum {
    PRESSURE_CONTROLLER_IDLE = 0,
    PRESSURE_CONTROLLER_INSP_RISE,
    PRESSURE_CONTROLLER_INSP_HOLD,
} ePressureControllerState;

typedef struct stPressureControllerDiagnostic {
    float inspTarget;
    float flowCompensation;
    float patientCorrection;
    float innerEffort;
    float blowerFeedforward;
} stPressureControllerDiagnostic;

/** Initialize the inspiratory pressure controller. */
void pressureControllerInit(void);

/** Produce one pressure-control actuator request for the active breath plan. */
int8_t pressureControllerProcess(const stBreathPlan *plan, stActuatorRequest *request);

/** Copy the latest inspiratory-control diagnostic values. */
void pressureControllerDiagnosticGet(stPressureControllerDiagnostic *diagnostic);

/** Return the current pressure-controller state. */
ePressureControllerState pressureControllerStateGet(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTALGO_PRESSURECONTROLLER_H */
/**************************End of file********************************/
