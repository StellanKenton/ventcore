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

#define EXPIRATION_CONTROLLER_SAMPLE_PERIOD_S                         0.006F

/* PEEP pressure PI. Slow feedforward adaptation below removes steady PI bias. */
#define EXPIRATION_CONTROLLER_PEEP_KP                                  0.25F
#define EXPIRATION_CONTROLLER_PEEP_KI                                  0.05F
#define EXPIRATION_CONTROLLER_PEEP_KD                                  0.0F
#define EXPIRATION_CONTROLLER_PEEP_EFFORT_MIN                        (-1.0F)
#define EXPIRATION_CONTROLLER_PEEP_EFFORT_MAX                          1.0F
#define EXPIRATION_CONTROLLER_BLOWER_CORRECTION_SCALE                800.0F

/* Pressure and positive-slope relief provide damping during PEEP recovery. */
#define EXPIRATION_CONTROLLER_PEEP_RELIEF_DEADBAND                     0.5F
#define EXPIRATION_CONTROLLER_PEEP_RELIEF_GAIN                        10.0F
#define EXPIRATION_CONTROLLER_PEEP_RELIEF_MAX_OPENING                 40.0F
#define EXPIRATION_CONTROLLER_PEEP_RELIEF_SLOPE_ENABLE_MARGIN          1.0F
#define EXPIRATION_CONTROLLER_PEEP_RELIEF_SLOPE_DEADBAND               5.0F
#define EXPIRATION_CONTROLLER_PEEP_RELIEF_SLOPE_GAIN                   0.20F
#define EXPIRATION_CONTROLLER_PEEP_RELIEF_SLOPE_MAX_OPENING           20.0F
#define EXPIRATION_CONTROLLER_PEEP_VALVE_OPEN_STEP_BASE                8.0F
#define EXPIRATION_CONTROLLER_PEEP_VALVE_OPEN_STEP_MAX                20.0F
#define EXPIRATION_CONTROLLER_PEEP_VALVE_OPEN_SLOPE_GAIN               0.15F

/*
 * Online feedforward trim. The calibration table remains the base feedforward;
 * a slow learned trim absorbs the sustained PI correction at one PEEP setting.
 * The trim is reset when the configured PEEP changes materially.
 */
#define EXPIRATION_CONTROLLER_PEEP_ADAPT_TARGET_CHANGE_RESET           0.5F
#define EXPIRATION_CONTROLLER_PEEP_ADAPT_SETTLE_TIME_MS              500U
#define EXPIRATION_CONTROLLER_PEEP_ADAPT_PRESSURE_WINDOW               3.0F
#define EXPIRATION_CONTROLLER_PEEP_ADAPT_SLOPE_MAX                     4.0F
#define EXPIRATION_CONTROLLER_PEEP_ADAPT_EFFORT_DEADBAND               0.05F
#define EXPIRATION_CONTROLLER_PEEP_ADAPT_TRANSFER_PER_CYCLE            1.5F
#define EXPIRATION_CONTROLLER_PEEP_ADAPT_TRIM_MIN                   (-1500.0F)
#define EXPIRATION_CONTROLLER_PEEP_ADAPT_TRIM_MAX                    1500.0F

#define EXPIRATION_CONTROLLER_EXP_VALVE_OPEN_DUTY                      0U
#define EXPIRATION_CONTROLLER_EXP_VALVE_CLOSED_DUTY                  100U
#define EXPIRATION_CONTROLLER_RELIEF_CLOSED_DUTY                      100U
#define EXPIRATION_CONTROLLER_BLOWER_TARGET_MAX                      8000U

/* RELEASE -> CAPTURE transition: brake earlier when pressure is falling faster. */
#define EXPIRATION_CONTROLLER_PEEP_BASE_MARGIN                         2.0F
#define EXPIRATION_CONTROLLER_CAPTURE_MARGIN_MIN                       2.0F
#define EXPIRATION_CONTROLLER_CAPTURE_MARGIN_MAX                      15.0F
#define EXPIRATION_CONTROLLER_BRAKE_TIME_S                              0.080F
#define EXPIRATION_CONTROLLER_PRESSURE_SLOPE_INTERVAL_S                 0.024F
#define EXPIRATION_CONTROLLER_PRESSURE_HISTORY_COUNT                    5U

/*
 * CAPTURE trajectory. Far from PEEP a steeper slope is allowed; near PEEP the
 * desired slope is reduced automatically to improve damping before handoff.
 */
#define EXPIRATION_CONTROLLER_CAPTURE_SLOPE_K_FAR                       8.0F
#define EXPIRATION_CONTROLLER_CAPTURE_SLOPE_K_NEAR                      4.0F
#define EXPIRATION_CONTROLLER_CAPTURE_SLOPE_K_BLEND_ERROR               6.0F
#define EXPIRATION_CONTROLLER_CAPTURE_MAX_FALL_RATE                    80.0F
#define EXPIRATION_CONTROLLER_CAPTURE_PID_KP                            0.020F
#define EXPIRATION_CONTROLLER_CAPTURE_PID_KI                            0.0F
#define EXPIRATION_CONTROLLER_CAPTURE_PID_KD                            0.0F
#define EXPIRATION_CONTROLLER_CAPTURE_PID_EFFORT_MIN                  (-1.0F)
#define EXPIRATION_CONTROLLER_CAPTURE_PID_EFFORT_MAX                    1.0F

/* Strong braking, gentler release: actuator asymmetry adds damping. */
#define EXPIRATION_CONTROLLER_CAPTURE_VALVE_CORRECTION_CLOSE_SCALE     60.0F
#define EXPIRATION_CONTROLLER_CAPTURE_VALVE_CORRECTION_OPEN_SCALE      25.0F
#define EXPIRATION_CONTROLLER_CAPTURE_VALVE_BASE_DUTY                  75.0F
#define EXPIRATION_CONTROLLER_CAPTURE_VALVE_DUTY_MIN                   80.0F
#define EXPIRATION_CONTROLLER_CAPTURE_VALVE_DUTY_MAX                  100.0F
#define EXPIRATION_CONTROLLER_CAPTURE_VALVE_CLOSE_STEP_BASE            15.0F
#define EXPIRATION_CONTROLLER_CAPTURE_VALVE_CLOSE_STEP_MAX             50.0F
#define EXPIRATION_CONTROLLER_CAPTURE_SLOPE_OVERSPEED_MAX             120.0F
#define EXPIRATION_CONTROLLER_CAPTURE_VALVE_CLOSE_STEP_GAIN             0.30F
#define EXPIRATION_CONTROLLER_CAPTURE_VALVE_OPEN_STEP_BASE              4.0F
#define EXPIRATION_CONTROLLER_CAPTURE_VALVE_OPEN_STEP_MAX              12.0F
#define EXPIRATION_CONTROLLER_CAPTURE_SLOPE_UNDERSPEED_MAX            100.0F
#define EXPIRATION_CONTROLLER_CAPTURE_VALVE_OPEN_STEP_GAIN              0.08F
#define EXPIRATION_CONTROLLER_CAPTURE_BLOWER_MAX_STEP                 800.0F

/* CAPTURE -> PEEP handoff. Crossing PEEP is handled immediately in the source. */
#define EXPIRATION_CONTROLLER_CAPTURE_PRESSURE_TOLERANCE                0.8F
#define EXPIRATION_CONTROLLER_CAPTURE_STABLE_SLOPE_MAX                  8.0F
#define EXPIRATION_CONTROLLER_CAPTURE_STABLE_SAMPLE_COUNT               3U
#define EXPIRATION_CONTROLLER_CAPTURE_TIMEOUT_MS                      180U

#define EXPIRATION_CONTROLLER_BLOWER_MAX_STEP                         800.0F
#define EXPIRATION_CONTROLLER_EXP_VALVE_CLOSE_MAX_STEP                  8.0F
#define EXPIRATION_CONTROLLER_PEEP_ENTRY_VALVE_BASE_DUTY               75.0F
#define EXPIRATION_CONTROLLER_PEEP_ENTRY_VALVE_DUTY_MIN                80.0F
#define EXPIRATION_CONTROLLER_PEEP_ENTRY_VALVE_DUTY_MAX                95.0F
#define EXPIRATION_CONTROLLER_PEEP_ENTRY_RAMP_TIME_MS                1200.0F

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
                                   ePhaseControllerState phase,
                                   const stActuatorRequest *previousRequest,
                                   stActuatorRequest *request);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTALGO_EXPIRATIONCONTROLLER_H */
/**************************End of file********************************/
