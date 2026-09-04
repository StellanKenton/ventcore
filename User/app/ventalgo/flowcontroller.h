/************************************************************************************
* @file     : flowcontroller.h
* @brief    : Inspiratory flow controller interface.
* @details  : Produces a unified actuator request for volume inspirations.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTALGO_FLOWCONTROLLER_H
#define USER_APP_VENTALGO_FLOWCONTROLLER_H

#include <stdint.h>

#include "actuatorrequest.h"
#include "breathscheduler.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLOW_CONTROLLER_SAMPLE_PERIOD_S          0.006F
#define FLOW_CONTROLLER_FLOW_KP                   0.02F
#define FLOW_CONTROLLER_FLOW_KI                   0.005F
#define FLOW_CONTROLLER_FLOW_KD                   0.00F
#define FLOW_CONTROLLER_HOLD_KP                   0.015F
#define FLOW_CONTROLLER_HOLD_KI                   0.02F
#define FLOW_CONTROLLER_EFFORT_MIN              (-1.0F)
#define FLOW_CONTROLLER_EFFORT_MAX                1.0F
#define FLOW_CONTROLLER_FLOW_INPUT_SCALE          1.0F
#define FLOW_CONTROLLER_FLOW_TARGET_MIN           0.0F
#define FLOW_CONTROLLER_FLOW_TARGET_MAX         120.0F
#define FLOW_CONTROLLER_FLOW_FF_LINEAR            0.1572F
#define FLOW_CONTROLLER_FLOW_FF_QUADRATIC         0.004013F
#define FLOW_CONTROLLER_FEEDFORWARD_PRESSURE_ALPHA 0.20F
#define FLOW_CONTROLLER_HOLD_EFFORT_ALPHA          0.20F
#define FLOW_CONTROLLER_BLOWER_SPEED_SCALE       800U
#define FLOW_CONTROLLER_EXP_VALVE_CLOSED_DUTY    100U
#define FLOW_CONTROLLER_RELIEF_CLOSED_DUTY       100U

typedef enum {
    FLOW_CONTROLLER_IDLE = 0,
    FLOW_CONTROLLER_INSP_RISE,
    FLOW_CONTROLLER_INSP_HOLD,
    FLOW_CONTROLLER_INSP_PAUSE,
} eFlowControllerState;

/** Initialize the inspiratory flow controller. */
void flowControllerInit(void);

/** Produce one flow-control actuator request for the active breath plan. */
int8_t flowControllerProcess(const stBreathPlan *plan, stActuatorRequest *request);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTALGO_FLOWCONTROLLER_H */
/**************************End of file********************************/
