/************************************************************************************
* @file     : pressurecontroller.h
* @brief    : Ventilation pressure controller interface.
* @details  : Declares pressure controller initialization and periodic processing.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTALGO_PRESSURECONTROLLER_H
#define USER_APP_VENTALGO_PRESSURECONTROLLER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PRESSURE_CONTROLLER_SAMPLE_PERIOD_S          0.006F

#define PRESSURE_CONTROLLER_OUTER_KP                 0.2F
#define PRESSURE_CONTROLLER_OUTER_KI                 0.0F
#define PRESSURE_CONTROLLER_OUTER_KD                 0.0F
#define PRESSURE_CONTROLLER_OUTER_CORRECTION_MIN    (-1.0F)
#define PRESSURE_CONTROLLER_OUTER_CORRECTION_MAX     1.0F

#define PRESSURE_CONTROLLER_INNER_KP                 0.015F
#define PRESSURE_CONTROLLER_INNER_KI                 0.0F
#define PRESSURE_CONTROLLER_INNER_KD                 0.0F
#define PRESSURE_CONTROLLER_EFFORT_MIN              (-1.0F)
#define PRESSURE_CONTROLLER_EFFORT_MAX               1.0F

#define PRESSURE_CONTROLLER_FLOW_INPUT_SCALE         0.5F
#define PRESSURE_CONTROLLER_FLOW_INPUT_MAX           35.0F
#define PRESSURE_CONTROLLER_FLOW_FF_LINEAR           0.1572F
#define PRESSURE_CONTROLLER_FLOW_FF_QUADRATIC        0.004013F
#define PRESSURE_CONTROLLER_FLOW_FF_MAX               9.0F
#define PRESSURE_CONTROLLER_FLOW_FF_DELTA_RATIO       0.3F
#define PRESSURE_CONTROLLER_FLOW_FF_RISE_FILTER_GAIN  0.15F
#define PRESSURE_CONTROLLER_FLOW_FF_HOLD_FILTER_GAIN  0.15F
#define PRESSURE_CONTROLLER_INSP_TARGET_MIN           0.0F
#define PRESSURE_CONTROLLER_INSP_TARGET_MAX           100.0F

#define PRESSURE_CONTROLLER_PEEP_KP                  0.25F
#define PRESSURE_CONTROLLER_PEEP_KI                  0.05F
#define PRESSURE_CONTROLLER_PEEP_KD                  0.0F
#define PRESSURE_CONTROLLER_PEEP_EFFORT_MIN         (-1.0F)
#define PRESSURE_CONTROLLER_PEEP_EFFORT_MAX           1.0F

#define PRESSURE_CONTROLLER_PEEP_BLOWER_CORRECTION     800.0F
#define PRESSURE_CONTROLLER_PEEP_RELIEF_DEADBAND       0.5F
#define PRESSURE_CONTROLLER_PEEP_RELIEF_GAIN           10.0F
#define PRESSURE_CONTROLLER_PEEP_RELIEF_MAX_OPENING    40.0F
#define PRESSURE_CONTROLLER_EXP_VALVE_OPEN_DUTY       0U
#define PRESSURE_CONTROLLER_EXP_VALVE_CLOSED_DUTY     100U
#define PRESSURE_CONTROLLER_INSP_EXP_DUTY             PRESSURE_CONTROLLER_EXP_VALVE_CLOSED_DUTY
#define PRESSURE_CONTROLLER_EXP_RELEASE_DUTY          PRESSURE_CONTROLLER_EXP_VALVE_OPEN_DUTY
#define PRESSURE_CONTROLLER_HOLD_RELIEF_DEADBAND       2.0F
#define PRESSURE_CONTROLLER_HOLD_RELIEF_GAIN           2.0F
#define PRESSURE_CONTROLLER_HOLD_RELIEF_MAX_OPENING    10.0F
#define PRESSURE_CONTROLLER_BLOWER_SPEED_SCALE        8000U

typedef enum {
    PRESSURE_CONTROLLER_IDLE = 0,
    PRESSURE_CONTROLLER_INSP_RISE,
    PRESSURE_CONTROLLER_INSP_HOLD,
    PRESSURE_CONTROLLER_EXP_RELEASE,
    PRESSURE_CONTROLLER_EXP_PEEP,
} ePressureControllerState;

typedef struct stPressureControllerDiagnostic {
    float inspTarget;
    float flowCompensation;
    float patientCorrection;
    float innerEffort;
    float blowerFeedforward;
} stPressureControllerDiagnostic;

/** Initialize the pressure controller. */
void pressureControllerInit(void);

/** Run one pressure controller processing cycle. */
void pressureControllerProcess(void);

/** Get the requested blower PWM in VCM protocol scale. */
uint16_t pressureControllerBlowerTargetGet(void);

/** Get the requested expiratory valve duty in percent. */
uint8_t pressureControllerExpValveDutyGet(void);

/** Copy the latest inspiratory-control diagnostic values. */
void pressureControllerDiagnosticGet(stPressureControllerDiagnostic *diagnostic);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTALGO_PRESSURECONTROLLER_H */
/**************************End of file********************************/
