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
#define EXPIRATION_CONTROLLER_PEEP_BASE_MARGIN              2.0F
#define EXPIRATION_CONTROLLER_CAPTURE_MARGIN_MIN            2.0F
#define EXPIRATION_CONTROLLER_CAPTURE_MARGIN_MAX           10.0F
#define EXPIRATION_CONTROLLER_BRAKE_TIME_S                   0.060F
#define EXPIRATION_CONTROLLER_PRESSURE_SLOPE_INTERVAL_S      0.024F
#define EXPIRATION_CONTROLLER_PRESSURE_HISTORY_COUNT         5U
#define EXPIRATION_CONTROLLER_CAPTURE_RAMP_TIME_MS           60U
#define EXPIRATION_CONTROLLER_CAPTURE_BLOWER_START_DUTY      60.0F
#define EXPIRATION_CONTROLLER_CAPTURE_VALVE_BASE_DUTY        75.0F
#define EXPIRATION_CONTROLLER_CAPTURE_VALVE_DUTY_MIN         80.0F
#define EXPIRATION_CONTROLLER_CAPTURE_VALVE_DUTY_MAX         90.0F
#define EXPIRATION_CONTROLLER_CAPTURE_PRESSURE_TOLERANCE      1.0F
#define EXPIRATION_CONTROLLER_CAPTURE_STABLE_SLOPE_MAX        5.0F
#define EXPIRATION_CONTROLLER_CAPTURE_STABLE_SAMPLE_COUNT     3U
#define EXPIRATION_CONTROLLER_RELEASE_BLOWER_PROGRESS_GAIN  1.5F
#define EXPIRATION_CONTROLLER_RELEASE_BLOWER_MAX_STEP     400.0F
#define EXPIRATION_CONTROLLER_BLOWER_MAX_STEP             800.0F
#define EXPIRATION_CONTROLLER_EXP_VALVE_CLOSE_MAX_STEP      8.0F
#define EXPIRATION_CONTROLLER_RELEASE_VALVE_DUTY_MAX      100.0F
#define EXPIRATION_CONTROLLER_PEEP_ENTRY_VALVE_BASE_DUTY   75.0F
#define EXPIRATION_CONTROLLER_PEEP_ENTRY_VALVE_DUTY_MIN    80.0F
#define EXPIRATION_CONTROLLER_PEEP_ENTRY_VALVE_DUTY_MAX    95.0F
#define EXPIRATION_CONTROLLER_PEEP_ENTRY_RAMP_TIME_MS    1200.0F

typedef enum {
    EXPIRATION_CONTROLLER_IDLE = 0,
    EXPIRATION_CONTROLLER_RELEASE,
    EXPIRATION_CONTROLLER_CAPTURE,
    EXPIRATION_CONTROLLER_PEEP,
} eExpirationControllerState;

/** Initialize the shared release and PEEP controller. */
void expirationControllerInit(void);

/** Produce one expiration actuator request for the active breath plan. */
int8_t expirationControllerProcess(const stBreathPlan *plan,
                                   const stActuatorRequest *previousRequest,
                                   stActuatorRequest *request);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTALGO_EXPIRATIONCONTROLLER_H */
/**************************End of file********************************/
