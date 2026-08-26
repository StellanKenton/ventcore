/************************************************************************************
* @file     : breathscheduler.h
* @brief    : Breath scheduler interface.
* @details  : Declares validated breath plans selected from ventilation modes.
* @author   :
* @date     :
* @version  :
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTLOGIC_BREATHSCHEDULER_H
#define USER_APP_VENTLOGIC_BREATHSCHEDULER_H

#include <stdint.h>

#include "settingdata.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BREATH_CONTROL_SUCCESS              1
#define BREATH_CONTROL_ERROR_PARAM         (-1)
#define BREATH_CONTROL_ERROR_SETTINGS      (-2)
#define BREATH_CONTROL_ERROR_UNSUPPORTED   (-3)
#define BREATH_CONTROL_ERROR_STATE         (-4)
#define BREATH_PEEP_LOCK_TIME_MS           192U
#define BREATH_RISE_DELTA_REFERENCE         15.0F
#define BREATH_RISE_EXTRA_MS_PER_CMH2O      20.0F
#define BREATH_PRESSURE_CONTROL_HEADROOM      9.0F

typedef enum {
    BREATH_TYPE_NONE = 0,
    BREATH_TYPE_MANDATORY_PRESSURE,
    BREATH_TYPE_MANDATORY_VOLUME,
    BREATH_TYPE_SPONTANEOUS_PRESSURE_SUPPORT,
    BREATH_TYPE_COUNT,
} eBreathType;

typedef enum {
    BREATH_TRIGGER_REASON_NONE = 0,
    BREATH_TRIGGER_REASON_TIME,
    BREATH_TRIGGER_REASON_PRESSURE,
    BREATH_TRIGGER_REASON_FLOW,
    BREATH_TRIGGER_REASON_APNEA_BACKUP,
    BREATH_TRIGGER_REASON_COUNT,
} eBreathTriggerReason;

typedef struct stBreathPlan {
    uint32_t sequence;
    eVentMode mode;
    eBreathType breathType;
    eBreathTriggerReason triggerReason;
    eVentTriggerType allowedTriggerType;
    float peepCmh2o;
    float inspiratoryPressureCmh2o;
    float inspiratoryFlowLpm;
    float targetTidalVolumeMl;
    float fio2Percent;
    float pressureLimitCmh2o;
    float pressureTriggerCmh2o;
    float flowTriggerLpm;
    uint32_t riseTimeMs;
    uint32_t holdTimeMs;
    uint32_t expiratoryTimeMs;
    uint32_t minimumExpiratoryTimeMs;
} stBreathPlan;

/** Configure the scheduler and leave it idle. */
int8_t breathSchedulerInit(void);

/** Start ventilation after validating the selected mode settings. */
int8_t breathSchedulerStart(eVentMode mode);

/** Stop ventilation immediately. */
int8_t breathSchedulerStop(void);

/** Set a ventilation mode directly for the venttest console command. */
int8_t breathSchedulerTestModeSet(uint8_t mode);

/** Set the running state directly for the venttest console command. */
int8_t breathSchedulerTestRunSet(uint8_t run);

/** Get the currently configured ventilation mode. */
eVentMode breathSchedulerModeGet(void);

/** Return 1 while ventilation is running, otherwise 0. */
uint8_t breathSchedulerRunningGet(void);

/** Validate and apply the latest settings for the selected ventilation mode. */
int8_t breathSchedulerSettingsUpdate(eVentMode mode);

/** Select and copy the next breath plan for the supplied trigger reason. */
int8_t breathSchedulerNextPlanGet(eBreathTriggerReason triggerReason, stBreathPlan *plan);

/** Apply live setting changes to the next-breath template. */
void breathSchedulerProcess(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTLOGIC_BREATHSCHEDULER_H */
/**************************End of file********************************/
