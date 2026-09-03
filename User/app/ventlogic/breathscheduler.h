/************************************************************************************
* @file     : breathscheduler.h
* @brief    : Breath scheduler interface.
* @details  : Declares breath plans selected from ventilation modes.
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
#define BREATH_PRESSURE_CONTROL_HEADROOM      9.0F
#define BREATH_PSV_MIN_INSPIRATORY_TIME_MS    200U

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

typedef enum {
    BREATH_CYCLE_TYPE_NONE = 0,
    BREATH_CYCLE_TYPE_TIME,
    BREATH_CYCLE_TYPE_FLOW,
    BREATH_CYCLE_TYPE_COUNT,
} eBreathCycleType;

typedef enum {
    BREATH_CYCLE_REASON_NONE = 0,
    BREATH_CYCLE_REASON_TIME,
    BREATH_CYCLE_REASON_FLOW,
    BREATH_CYCLE_REASON_MAX_INSPIRATORY_TIME,
    BREATH_CYCLE_REASON_PRESSURE_LIMIT,
    BREATH_CYCLE_REASON_ABORT,
    BREATH_CYCLE_REASON_COUNT,
} eBreathCycleReason;

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
    /** Shared live limits; changes take effect without replacing the active plan. */
    const stVentLimitSettings *limitSettings;
    float pressureTriggerCmh2o;
    float flowTriggerLpm;
    eBreathCycleType cycleType;
    float cycleOffPercent;
    uint32_t riseTimeMs;
    uint32_t holdTimeMs;
    uint32_t minimumInspiratoryTimeMs;
    uint32_t maximumInspiratoryTimeMs;
    uint32_t expiratoryTimeMs;
    uint32_t minimumExpiratoryTimeMs;
    uint32_t apneaTimeMs;
    uint32_t backupBreathIntervalMs;
    uint8_t timeTriggerEnabled;
} stBreathPlan;

/** Configure the scheduler and leave it idle. */
int8_t breathSchedulerInit(void);

/** Start ventilation with the selected mode settings. */
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

/** Return the sequence incremented on each stopped-to-running transition. */
uint32_t breathSchedulerRunSequenceGet(void);

/** Apply the latest settings for the selected ventilation mode. */
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
