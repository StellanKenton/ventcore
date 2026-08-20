/************************************************************************************
* @file     : settingdata.h
* @brief    : Ventilator setting data declarations.
* @details  : Defines the selectable ventilation modes.
***********************************************************************************/
#ifndef USER_APP_DATABUS_SETTINGDATA_H
#define USER_APP_DATABUS_SETTINGDATA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    VENT_MD_IDLE = 0,
    VENT_MD_PAC,
    VENT_MD_VAC,
    VENT_MD_CPAP_PSV,
    VENT_MD_PSV_ST,
    VENT_MD_P_SIMV,
    VENT_MD_V_SIMV,
    VENT_MD_PRVC,
    VENT_MD_PRVC_SIMV,
    VENT_MD_VS,
    VENT_MD_BAPAP,
    VENT_MD_APRV,
    VENT_MD_NCPAP,
    VENT_MD_NCPAP_PC,
    VENT_MD_NIPPV,
    VENT_MD_SNIPPV,
    VENT_MD_AMV,
    VENT_MD_IAMV,
    VENT_MD_PPS,
    VENT_MD_CPRV,
    VENT_MD_HFO,
    VENT_MD_COUNT
} eVentMode;

typedef enum {
    VENT_TRIGGER_OFF = 0,
    VENT_TRIGGER_PRESSURE,
    VENT_TRIGGER_FLOW,
    VENT_TRIGGER_COUNT
} eVentTriggerType;

typedef enum {
    VENT_FLOW_SQUARE = 0,
    VENT_FLOW_DECELERATING,
    VENT_FLOW_PATTERN_COUNT
} eVentFlowPattern;

typedef enum {
    VENT_PATIENT_ADULT = 0,
    VENT_PATIENT_PEDIATRIC,
    VENT_PATIENT_NEONATAL,
    VENT_PATIENT_TYPE_COUNT
} eVentPatientType;

typedef struct stVentLimitSettings {
    float pressureLow;          // min 1.0
    float pressureHigh;         // max 100.0
    float minuteVolumeLow;      // min 0.1
    float minuteVolumeHigh;     // max 100.0
    uint16_t tidalVolumeLow;    // min 1
    uint16_t tidalVolumeHigh;   // max 6000
    uint16_t o2PercentLow;      // min 18
    uint16_t o2PercentHigh;     // max 100
    uint16_t frequencyLow;      // min 1
    uint16_t frequencyHigh;     // max 160
    uint16_t apneaTimeHigh;     // max 60
}stVentLimitSettings;


typedef struct stVentPatientSettings {
    eVentPatientType Type;
    uint16_t IdealBodyWeightKg;  // min 1, max 300
    uint16_t IdealBodyHeightCm;  // min 2, max 440
}stVentPatientSettings;

/* Pressure assist/control ventilation. */
typedef struct stVentPacSettings {
    float oxygen;
    float peep;
    float Rate;
    uint32_t inspiratoryTimeMs;
    float inspPressure;
    uint32_t riseTimeMs;
    eVentTriggerType triggerType;
    float pressureTriggerCmh2o;
    float flowTriggerLpm;
} stVentPacSettings;

/* Volume assist/control ventilation. */
typedef struct stVentVacSettings {
    float oxygenPercent;
    float peepCmh2o;
    float pressureLimitCmh2o;
    float respiratoryRateBpm;
    uint32_t inspiratoryTimeMs;
    float tidalVolumeMl;
    float inspiratoryFlowLpm;
    eVentFlowPattern flowPattern;
    eVentTriggerType triggerType;
    float pressureTriggerCmh2o;
    float flowTriggerLpm;
} stVentVacSettings;

/* Continuous positive airway pressure with pressure support. */
typedef struct stVentCpapPsvSettings {
    float oxygenPercent;
    float peepCmh2o;
    float pressureLimitCmh2o;
    eVentTriggerType triggerType;
    float pressureTriggerCmh2o;
    float flowTriggerLpm;
    float pressureSupportCmh2o;
    uint32_t riseTimeMs;
    float cycleOffPercent;
    uint32_t maxInspiratoryTimeMs;
    uint32_t apneaAlarmTimeMs;
} stVentCpapPsvSettings;

/* Pressure support with timed pressure-control backup. */
typedef struct stVentPsvStSettings {
    float oxygenPercent;
    float peepCmh2o;
    float pressureLimitCmh2o;
    eVentTriggerType triggerType;
    float pressureTriggerCmh2o;
    float flowTriggerLpm;
    float pressureSupportCmh2o;
    uint32_t riseTimeMs;
    float cycleOffPercent;
    uint32_t maxInspiratoryTimeMs;
    uint32_t apneaTimeMs;
    float backupRespiratoryRateBpm;
    uint32_t backupInspiratoryTimeMs;
    float backupInspiratoryPressureCmh2o;
    uint32_t backupRiseTimeMs;
} stVentPsvStSettings;

/* Pressure SIMV with spontaneous pressure support. */
typedef struct stVentPSimvSettings {
    float oxygenPercent;
    float peepCmh2o;
    float pressureLimitCmh2o;
    float respiratoryRateBpm;
    uint32_t inspiratoryTimeMs;
    float inspiratoryPressureCmh2o;
    uint32_t pressureRiseTimeMs;
    eVentTriggerType triggerType;
    float pressureTriggerCmh2o;
    float flowTriggerLpm;
    uint32_t syncWindowMs;
    float pressureSupportCmh2o;
    uint32_t supportRiseTimeMs;
    float cycleOffPercent;
    uint32_t maxSpontaneousInspiratoryTimeMs;
} stVentPSimvSettings;

/* Volume SIMV with spontaneous pressure support. */
typedef struct stVentVSimvSettings {
    float oxygenPercent;
    float peepCmh2o;
    float pressureLimitCmh2o;
    float respiratoryRateBpm;
    uint32_t inspiratoryTimeMs;
    float tidalVolumeMl;
    float inspiratoryFlowLpm;
    eVentFlowPattern flowPattern;
    eVentTriggerType triggerType;
    float pressureTriggerCmh2o;
    float flowTriggerLpm;
    uint32_t syncWindowMs;
    float pressureSupportCmh2o;
    uint32_t supportRiseTimeMs;
    float cycleOffPercent;
    uint32_t maxSpontaneousInspiratoryTimeMs;
} stVentVSimvSettings;

/* Pressure-regulated volume control. */
typedef struct stVentPrvcSettings {
    float oxygenPercent;
    float peepCmh2o;
    float pressureLimitCmh2o;
    float respiratoryRateBpm;
    uint32_t inspiratoryTimeMs;
    eVentTriggerType triggerType;
    float pressureTriggerCmh2o;
    float flowTriggerLpm;
    float targetTidalVolumeMl;
    float minimumInspiratoryPressureCmh2o;
    float maximumInspiratoryPressureCmh2o;
    float maximumPressureStepCmh2o;
} stVentPrvcSettings;

/* PRVC SIMV with spontaneous pressure support. */
typedef struct stVentPrvcSimvSettings {
    float oxygenPercent;
    float peepCmh2o;
    float pressureLimitCmh2o;
    float respiratoryRateBpm;
    uint32_t inspiratoryTimeMs;
    eVentTriggerType triggerType;
    float pressureTriggerCmh2o;
    float flowTriggerLpm;
    uint32_t syncWindowMs;
    float targetTidalVolumeMl;
    float minimumInspiratoryPressureCmh2o;
    float maximumInspiratoryPressureCmh2o;
    float maximumPressureStepCmh2o;
    float pressureSupportCmh2o;
    uint32_t supportRiseTimeMs;
    float cycleOffPercent;
    uint32_t maxSpontaneousInspiratoryTimeMs;
} stVentPrvcSimvSettings;

/* Spontaneous volume support. */
typedef struct stVentVsSettings {
    float oxygenPercent;
    float peepCmh2o;
    float pressureLimitCmh2o;
    eVentTriggerType triggerType;
    float pressureTriggerCmh2o;
    float flowTriggerLpm;
    float initialPressureSupportCmh2o;
    uint32_t riseTimeMs;
    float cycleOffPercent;
    uint32_t maxInspiratoryTimeMs;
    float targetTidalVolumeMl;
    float minimumSupportPressureCmh2o;
    float maximumSupportPressureCmh2o;
    float maximumPressureStepCmh2o;
    uint32_t apneaAlarmTimeMs;
} stVentVsSettings;

/* Biphasic airway pressure with spontaneous breathing. */
typedef struct stVentBapapSettings {
    float oxygenPercent;
    float pressureLimitCmh2o;
    float pressureHighCmh2o;
    float pressureLowCmh2o;
    uint32_t timeHighMs;
    uint32_t timeLowMs;
    eVentTriggerType triggerType;
    float pressureTriggerCmh2o;
    float flowTriggerLpm;
    float pressureSupportCmh2o;
    uint32_t riseTimeMs;
    float cycleOffPercent;
    uint32_t maxInspiratoryTimeMs;
} stVentBapapSettings;

/* Airway pressure release ventilation. */
typedef struct stVentAprvSettings {
    float oxygenPercent;
    float pressureLimitCmh2o;
    float pressureHighCmh2o;
    float pressureLowCmh2o;
    uint32_t timeHighMs;
    uint32_t timeLowMs;
    float releaseCycleOffPercent;
} stVentAprvSettings;

/* Neonatal nasal CPAP. */
typedef struct stVentNcpapSettings {
    float oxygenPercent;
    float cpapPressureCmh2o;
    float biasFlowLpm;
    float pressureLimitCmh2o;
    uint32_t apneaAlarmTimeMs;
} stVentNcpapSettings;

/* Neonatal nasal CPAP with pressure-control breaths. */
typedef struct stVentNcpapPcSettings {
    float oxygenPercent;
    float cpapPressureCmh2o;
    float biasFlowLpm;
    float pressureLimitCmh2o;
    uint32_t apneaAlarmTimeMs;
    float respiratoryRateBpm;
    uint32_t inspiratoryTimeMs;
    float inspiratoryPressureCmh2o;
    uint32_t riseTimeMs;
} stVentNcpapPcSettings;

/* Non-invasive positive-pressure ventilation. */
typedef struct stVentNippvSettings {
    float oxygenPercent;
    float peepCmh2o;
    float pressureLimitCmh2o;
    float respiratoryRateBpm;
    uint32_t inspiratoryTimeMs;
    float inspiratoryPressureCmh2o;
    uint32_t riseTimeMs;
    float leakCompensationLimitLpm;
} stVentNippvSettings;

/* Synchronized neonatal NIPPV. */
typedef struct stVentSnippvSettings {
    float oxygenPercent;
    float peepCmh2o;
    float pressureLimitCmh2o;
    float respiratoryRateBpm;
    uint32_t inspiratoryTimeMs;
    float inspiratoryPressureCmh2o;
    uint32_t riseTimeMs;
    eVentTriggerType triggerType;
    float pressureTriggerCmh2o;
    float flowTriggerLpm;
    uint32_t syncWindowMs;
    float leakCompensationLimitLpm;
} stVentSnippvSettings;

/* Adaptive minute ventilation. */
typedef struct stVentAmvSettings {
    float oxygenPercent;
    float peepCmh2o;
    float pressureLimitCmh2o;
    float idealBodyWeightKg;
    float minuteVolumePercent;
    float minimumRespiratoryRateBpm;
    float maximumRespiratoryRateBpm;
    float maximumTidalVolumeMl;
    eVentTriggerType triggerType;
    float pressureTriggerCmh2o;
    float flowTriggerLpm;
    float pressureSupportCmh2o;
    uint32_t riseTimeMs;
    float cycleOffPercent;
    uint32_t maxInspiratoryTimeMs;
} stVentAmvSettings;

/* Intelligent adaptive minute ventilation supervisory limits. */
typedef struct stVentIamvSettings {
    float oxygenPercent;
    float peepCmh2o;
    float pressureLimitCmh2o;
    float idealBodyWeightKg;
    float targetMinuteVentilationLpm;
    float minimumTidalVolumeMl;
    float maximumTidalVolumeMl;
    float minimumRespiratoryRateBpm;
    float maximumRespiratoryRateBpm;
    float minimumSupportPressureCmh2o;
    float maximumSupportPressureCmh2o;
    eVentTriggerType triggerType;
    float pressureTriggerCmh2o;
    float flowTriggerLpm;
} stVentIamvSettings;

/* Proportional pressure support. */
typedef struct stVentPpsSettings {
    float oxygenPercent;
    float peepCmh2o;
    float pressureLimitCmh2o;
    eVentTriggerType triggerType;
    float pressureTriggerCmh2o;
    float flowTriggerLpm;
    float flowAssistCmh2oPerLps;
    float volumeAssistCmh2oPerL;
    float cycleOffPercent;
    uint32_t maxInspiratoryTimeMs;
    uint32_t apneaAlarmTimeMs;
} stVentPpsSettings;

/* Cardiopulmonary resuscitation ventilation. */
typedef struct stVentCprvSettings {
    eVentPatientType patientType;
    float idealBodyWeightKg;
    float oxygenPercent;
    float peepCmh2o;
    float pressureLimitCmh2o;
    float tidalVolumeMl;
    float respiratoryRateBpm;
    float inspiratoryExpiratoryRatio;
    uint8_t electronicItdEnabled;
} stVentCprvSettings;

/* High-frequency oscillatory ventilation. */
typedef struct stVentHfoSettings {
    float oxygenPercent;
    float meanAirwayPressureCmh2o;
    float pressureAmplitudeCmh2o;
    float frequencyHz;
    float inspiratoryRatioPercent;
    float biasFlowLpm;
    uint8_t sighEnabled;
    float sighPressureCmh2o;
    uint32_t sighIntervalMs;
} stVentHfoSettings;

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_DATABUS_SETTINGDATA_H */

/*************************************** End of file ********************************/
