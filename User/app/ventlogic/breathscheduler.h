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

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "settingdata.h"

#define BREATH_CONTROL_SUCCESS       1
#define BREATH_CONTROL_ERROR_PARAM  (-1)
#define BREATH_PEEP_LOCK_TIME_MS  192.0

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

    BREATH_COUNT,
} eBreathControlType;


typedef struct stBreathInfo {
    bool runState;
    bool runPrevious;
    eVentMode currentMode;
} stBreathInfo;


/* Configure the scheduler and leave it idle. */
int8_t breathSchedulerInit(void);

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
