/************************************************************************************
* @file     : breathscheduler.h
* @brief    : Breath phase scheduler interface.
* @details  : Declares the lightweight inspiration and expiration state machine.
* @author   :
* @date     :
* @version  :
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTLOGIC_BREATHSCHEDULER_H
#define USER_APP_VENTLOGIC_BREATHSCHEDULER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "settingdata.h"

#define BREATH_CONTROL_SUCCESS              1
#define BREATH_CONTROL_ERROR_PARAM         (-1)
#define BREATH_CONTROL_ERROR_SETTINGS      (-2)
#define BREATH_CONTROL_ERROR_UNSUPPORTED   (-3)
#define BREATH_PEEP_LOCK_TIME_MS           192.0F
#define BREATH_RISE_DELTA_REFERENCE         15.0F
#define BREATH_RISE_EXTRA_MS_PER_CMH2O      20.0F
#define BREATH_PRESSURE_CONTROL_HEADROOM      9.0F

typedef enum {
    PRESSURE_CONTROL = 0,
    VOLUME_CONTROL,
} eCONTROL_TYPE;

/* Select the closed-loop control strategy used by the phase controller. */
typedef enum {
    BREATH_NONE = 0,
    BREATH_RUN,
    BREATH_PATIENT_TYPE,
    BREATH_IDEAL_BODY_WEIGHT,
    BREATH_IDEAL_BODY_HEIGHT,
    BREATH_FLOW,
    BREATH_INSP_PRESSURE,
    BREATH_PEEP_PRESSURE,
    BREATH_VOLUME,
    BREATH_CONTROL_TYPE,
    BREATH_INSP_RISE_TIME,
    BREATH_INSP_HOLD_TIME,
    BREATH_INSP_PEEP_TIME,
    BREATH_FIO2,
    BREATH_TIDAL_VOLUME,
    BREATH_INSP_PAUSE_PERCENT,
    BREATH_TRIGGER_STATE,
    BREATH_TRIGGER_FLOW,
    BREATH_TRIGGER_PRESSURE,
    BREATH_PRESSURE_LIMIT,
    BREATH_COUNT,
} eBreathControlType;


/* Configure the scheduler and leave it idle. */
int8_t breathSchedulerInit(void);

/** Start ventilation after validating and applying the selected mode settings. */
int8_t breathSchedulerStart(eVentMode mode);

/** Stop ventilation immediately. */
int8_t breathSchedulerStop(void);

/** Set the PAC mode directly for the venttest console command. */
int8_t breathSchedulerTestModeSet(uint8_t mode);

/** Set the running state directly for the venttest console command. */
int8_t breathSchedulerTestRunSet(uint8_t run);

/** Get the currently configured ventilation mode. */
eVentMode breathSchedulerModeGet(void);

/** Return 1 while ventilation is running, otherwise 0. */
uint8_t breathSchedulerRunningGet(void);

/** Validate and apply the latest settings for the selected ventilation mode. */
int8_t breathSchedulerSettingsUpdate(eVentMode mode);

/* Set a breath control value selected by type. */
int8_t breathControlSet(eBreathControlType type, float value);

/* Get a breath control value selected by type. */
float breathControlGet(eBreathControlType type);

/* Advance the state machine using the current monotonic tick. */
void breathSchedulerProcess(void);


#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTLOGIC_BREATHSCHEDULER_H */
/**************************End of file********************************/
