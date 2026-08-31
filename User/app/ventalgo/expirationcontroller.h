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

/*
 * Fast PEEP feedback is P-only. The slow steady-state component is learned into
 * adaptive feedforward, avoiding two competing integrators.
 */
#define EXPIRATION_CONTROLLER_PEEP_KP                                  0.20F
#define EXPIRATION_CONTROLLER_PEEP_KI                                  0.0F
#define EXPIRATION_CONTROLLER_PEEP_KD                                  0.0F
#define EXPIRATION_CONTROLLER_PEEP_EFFORT_MIN                        (-1.0F)
#define EXPIRATION_CONTROLLER_PEEP_EFFORT_MAX                          1.0F
#define EXPIRATION_CONTROLLER_BLOWER_CORRECTION_SCALE                800.0F

/* Static pressure relief plus positive dP/dt damping during PEEP recovery. */
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
 * Online PEEP FF adaptation. calibtransPrsSpeed() remains the baseline; the
 * learned FF is retained across breaths for the same PEEP and reset when the
 * configured PEEP changes materially.
 */
#define EXPIRATION_CONTROLLER_PEEP_ADAPT_TARGET_CHANGE_RESET           0.5F
#define EXPIRATION_CONTROLLER_PEEP_ADAPT_SETTLE_TIME_MS              400U
#define EXPIRATION_CONTROLLER_PEEP_ADAPT_PRESSURE_WINDOW               5.0F
#define EXPIRATION_CONTROLLER_PEEP_ADAPT_PRESSURE_DEADBAND             0.15F
#define EXPIRATION_CONTROLLER_PEEP_ADAPT_SLOPE_MAX                     4.0F
#define EXPIRATION_CONTROLLER_PEEP_ADAPT_GAIN_PER_CYCLE                0.60F
#define EXPIRATION_CONTROLLER_PEEP_ADAPT_MAX_STEP_PER_CYCLE            2.0F
#define EXPIRATION_CONTROLLER_PEEP_ADAPT_MIN_RATIO                     0.65F
#define EXPIRATION_CONTROLLER_PEEP_ADAPT_MAX_RATIO                     1.35F

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

/* CAPTURE trajectory: fast far from PEEP, gentler in the last few cmH2O. */
#define EXPIRATION_CONTROLLER_CAPTURE_SLOPE_K_FAR                       8.0F
#define EXPIRATION_CONTROLLER_CAPTURE_SLOPE_K_NEAR                      4.0F
#define EXPIRATION_CONTROLLER_CAPTURE_SLOPE_K_BLEND_ERROR               6.0F
#define EXPIRATION_CONTROLLER_CAPTURE_MAX_FALL_RATE                    80.0F
#define EXPIRATION_CONTROLLER_CAPTURE_PID_KP                            0.020F
#define EXPIRATION_CONTROLLER_CAPTURE_PID_KI                            0.0F
#define EXPIRATION_CONTROLLER_CAPTURE_PID_KD                            0.0F
#define EXPIRATION_CONTROLLER_CAPTURE_PID_EFFORT_MIN                  (-1.0F)
#define EXPIRATION_CONTROLLER_CAPTURE_PID_EFFORT_MAX                    1.0F

/* Strong braking, gentle reopening to suppress CAPTURE ringing. */
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
#define EXPIRATION_CONTROLLER_CAPTURE_VALVE_OPEN_STEP_MAX              10.0F
#define EXPIRATION_CONTROLLER_CAPTURE_SLOPE_UNDERSPEED_MAX            100.0F
#define EXPIRATION_CONTROLLER_CAPTURE_VALVE_OPEN_STEP_GAIN              0.08F
#define EXPIRATION_CONTROLLER_CAPTURE_BLOWER_MAX_STEP                 800.0F

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

typedef struct {
    float peepBaseFeedforwardTarget;
    float peepAdaptiveBiasTarget;
    float peepAdaptiveFeedforwardTarget;
    float peepFeedbackEffort;
    float pressureSlopeCmh2oPerS;
} stExpirationControllerDiagnostic;

void expirationControllerInit(void);

int8_t expirationControllerProcess(const stBreathPlan *plan,
                                   ePhaseControllerState phase,
                                   const stActuatorRequest *previousRequest,
                                   stActuatorRequest *request);

void expirationControllerDiagnosticGet(stExpirationControllerDiagnostic *diagnostic);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTALGO_EXPIRATIONCONTROLLER_H */
/**************************End of file********************************/
