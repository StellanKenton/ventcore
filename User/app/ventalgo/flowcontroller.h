/************************************************************************************
* @file     : flowcontroller.h
* @brief    : Ventilation flow controller interface.
* @details  : Declares flow controller initialization and periodic processing.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTALGO_FLOWCONTROLLER_H
#define USER_APP_VENTALGO_FLOWCONTROLLER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FLOW_CONTROLLER_SAMPLE_PERIOD_S          0.006F

#define FLOW_CONTROLLER_FLOW_KP                   0.02F
#define FLOW_CONTROLLER_FLOW_KI                   0.005F
#define FLOW_CONTROLLER_FLOW_KD                   0.00F
#define FLOW_CONTROLLER_EFFORT_MIN              (-1.0F)
#define FLOW_CONTROLLER_EFFORT_MAX                1.0F
#define FLOW_CONTROLLER_FLOW_INPUT_SCALE          0.5F
#define FLOW_CONTROLLER_FLOW_TARGET_MIN           0.0F
#define FLOW_CONTROLLER_FLOW_TARGET_MAX         120.0F

#define FLOW_CONTROLLER_FLOW_FF_LINEAR            0.1572F
#define FLOW_CONTROLLER_FLOW_FF_QUADRATIC         0.004013F
#define FLOW_CONTROLLER_BLOWER_SPEED_SCALE      8000U
#define FLOW_CONTROLLER_EXP_VALVE_CLOSED_DUTY    100U

typedef enum {
    FLOW_CONTROLLER_IDLE = 0,
    FLOW_CONTROLLER_INSP_RISE,
    FLOW_CONTROLLER_INSP_HOLD,
    FLOW_CONTROLLER_EXP_RELEASE,
    FLOW_CONTROLLER_EXP_PEEP,
} eFlowControllerState;

/** Initialize the flow controller. */
void flowControllerInit(void);

/** Run one flow controller processing cycle. */
void flowControllerProcess(void);

/** Get the requested blower PWM in VCM protocol scale. */
uint16_t flowControllerBlowerTargetGet(void);

/** Get the requested expiratory valve duty in percent. */
uint8_t flowControllerExpValveDutyGet(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTALGO_FLOWCONTROLLER_H */
/**************************End of file********************************/
