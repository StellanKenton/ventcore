/************************************************************************************
* @file     : expirationcontroller.h
* @brief    : Shared expiration controller interface.
* @details  : Controls release and PEEP for every normal breath type.
* @author   :
* @date     : 2026-08-26
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTALGO_EXPIRATIONCONTROLLER_H
#define USER_APP_VENTALGO_EXPIRATIONCONTROLLER_H

#include <stdint.h>

#include "actuatorrequest.h"
#include "breathscheduler.h"
#include "phasecontroller.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EXPIRATION_CONTROLLER_SAMPLE_PERIOD_S              0.006F
#define EXPIRATION_CONTROLLER_PEEP_KP                       0.25F
#define EXPIRATION_CONTROLLER_PEEP_KI                       0.05F
#define EXPIRATION_CONTROLLER_PEEP_KD                       0.0F
#define EXPIRATION_CONTROLLER_PEEP_EFFORT_MIN             (-1.0F)
#define EXPIRATION_CONTROLLER_PEEP_EFFORT_MAX               1.0F
#define EXPIRATION_CONTROLLER_BLOWER_CORRECTION_SCALE     800.0F
#define EXPIRATION_CONTROLLER_PEEP_RELIEF_DEADBAND          0.5F
#define EXPIRATION_CONTROLLER_PEEP_RELIEF_GAIN             10.0F
#define EXPIRATION_CONTROLLER_PEEP_RELIEF_MAX_OPENING      40.0F
#define EXPIRATION_CONTROLLER_EXP_VALVE_OPEN_DUTY           0U
#define EXPIRATION_CONTROLLER_EXP_VALVE_CLOSED_DUTY       100U
#define EXPIRATION_CONTROLLER_RELIEF_CLOSED_DUTY           100U
#define EXPIRATION_CONTROLLER_BLOWER_TARGET_MAX           8000U
#define EXPIRATION_CONTROLLER_RELEASE_SOFT_MARGIN_RATIO_MIN 0.50F
#define EXPIRATION_CONTROLLER_RELEASE_SOFT_MARGIN_RATIO_PEEP_SCALE 0.05F
#define EXPIRATION_CONTROLLER_RELEASE_PEEP_MARGIN_RATIO     0.50F
#define EXPIRATION_CONTROLLER_RELEASE_SOFT_MARGIN_MIN       2.0F
#define EXPIRATION_CONTROLLER_RELEASE_SOFT_MARGIN_MAX_MIN  10.0F
#define EXPIRATION_CONTROLLER_RELEASE_SOFT_MARGIN_MAX_PEEP_RATIO 2.0F
#define EXPIRATION_CONTROLLER_RELEASE_SOFT_MARGIN_MAX_LIMIT 35.0F
#define EXPIRATION_CONTROLLER_PEEP_ENTRY_MARGIN             2.0F
#define EXPIRATION_CONTROLLER_RELEASE_BLOWER_PROGRESS_GAIN  1.5F
#define EXPIRATION_CONTROLLER_RELEASE_BLOWER_MAX_STEP     400.0F
#define EXPIRATION_CONTROLLER_BLOWER_MAX_STEP             200.0F
#define EXPIRATION_CONTROLLER_EXP_VALVE_CAPTURE_STEP        4.0F
#define EXPIRATION_CONTROLLER_EXP_VALVE_CLOSE_MAX_STEP      2.0F
#define EXPIRATION_CONTROLLER_RELEASE_VALVE_DUTY_MAX      100.0F
#define EXPIRATION_CONTROLLER_PEEP_ENTRY_VALVE_BASE_DUTY   75.0F
#define EXPIRATION_CONTROLLER_PEEP_ENTRY_VALVE_DUTY_MIN    80.0F
#define EXPIRATION_CONTROLLER_PEEP_ENTRY_VALVE_DUTY_MAX    95.0F
#define EXPIRATION_CONTROLLER_PEEP_ENTRY_RAMP_TIME_MS    1200.0F

typedef enum {
    EXPIRATION_CONTROLLER_IDLE = 0,
    EXPIRATION_CONTROLLER_RELEASE,
    EXPIRATION_CONTROLLER_PEEP,
} eExpirationControllerState;

/** Initialize the shared release and PEEP controller. */
void expirationControllerInit(void);

/** Produce one expiration actuator request for the active breath plan. */
int8_t expirationControllerProcess(const stBreathPlan *plan,
                                   ePhaseControllerState phase,
                                   const stActuatorRequest *previousRequest,
                                   stActuatorRequest *request);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTALGO_EXPIRATIONCONTROLLER_H */
/**************************End of file********************************/
