/************************************************************************************
* @file     : breathscheduler.c
* @brief    : Breath scheduler.
* @details  : Builds mode settings into one immutable plan per breath.
* @author   :
* @date     :
* @version  :
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "breathscheduler.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "calibration.h"
#include "numfilter.h"
#include "rtos.h"

static volatile bool gBreathRunning = false;
static volatile eVentMode gBreathMode = VENT_MD_IDLE;
static bool gBreathSettingsApplied = false;
static uint32_t gBreathSequence = 0U;
static uint32_t gBreathRunSequence = 0U;
static stBreathPlan gBreathPlanTemplate;
static stBreathPlan gBreathBackupPlanTemplate;
static bool gBreathBackupPlanValid = false;
static stVentPatientSettings gBreathAppliedPatientSettings;
static stVentPacSettings gBreathAppliedPacSettings;
static stVentVacSettings gBreathAppliedVacSettings;
static stVentCpapPsvSettings gBreathAppliedCpapPsvSettings;
static stVentPsvStSettings gBreathAppliedPsvStSettings;

/** Return true when the PAC source settings differ from the applied snapshot. */
static bool breathSchedulerPacSettingsChanged(void)
{
    return !gBreathSettingsApplied ||
           (memcmp(&gBreathAppliedPatientSettings,
                   GetVentPatientSettings(),
                   sizeof(gBreathAppliedPatientSettings)) != 0) ||
           (memcmp(&gBreathAppliedPacSettings,
                   GetVentPacSettings(),
                   sizeof(gBreathAppliedPacSettings)) != 0);
}

/** Return true when the VAC source settings differ from the applied snapshot. */
static bool breathSchedulerVacSettingsChanged(void)
{
    return !gBreathSettingsApplied ||
           (memcmp(&gBreathAppliedPatientSettings,
                   GetVentPatientSettings(),
                   sizeof(gBreathAppliedPatientSettings)) != 0) ||
           (memcmp(&gBreathAppliedVacSettings,
                   GetVentVacSettings(),
                   sizeof(gBreathAppliedVacSettings)) != 0);
}

/** Return true when the CPAP/PSV source settings differ from the applied snapshot. */
static bool breathSchedulerCpapPsvSettingsChanged(void)
{
    return !gBreathSettingsApplied ||
           (memcmp(&gBreathAppliedPatientSettings,
                   GetVentPatientSettings(),
                   sizeof(gBreathAppliedPatientSettings)) != 0) ||
           (memcmp(&gBreathAppliedCpapPsvSettings,
                   GetVentCpapPsvSettings(),
                   sizeof(gBreathAppliedCpapPsvSettings)) != 0);
}

/** Return true when the PSV-ST source settings differ from the applied snapshot. */
static bool breathSchedulerPsvStSettingsChanged(void)
{
    return !gBreathSettingsApplied ||
           (memcmp(&gBreathAppliedPatientSettings,
                   GetVentPatientSettings(),
                   sizeof(gBreathAppliedPatientSettings)) != 0) ||
           (memcmp(&gBreathAppliedPsvStSettings,
                   GetVentPsvStSettings(),
                   sizeof(gBreathAppliedPsvStSettings)) != 0);
}

/** Build the next-breath template from PAC settings. */
static void breathSchedulerPacPlanApply(const stVentPacSettings *pacSettings,
                                        stBreathPlan *plan)
{
    float lBreathPeriodMs = 60000.0F / pacSettings->Rate;
    float lEffectiveRiseTime = (float)pacSettings->riseTimeMs;
    stBreathPlan lPlan = {0};

    lEffectiveRiseTime = NUMFILTER_MIN((float)pacSettings->inspiratoryTimeMs,
                                       lEffectiveRiseTime);

    lPlan.mode = VENT_MD_PAC;
    lPlan.breathType = BREATH_TYPE_MANDATORY_PRESSURE;
    lPlan.allowedTriggerType = pacSettings->triggerType;
    lPlan.peepCmh2o = pacSettings->peep;
    lPlan.inspiratoryPressureCmh2o = pacSettings->peep + pacSettings->DeltaPressure;
    lPlan.fio2Percent = pacSettings->oxygen;
    lPlan.pressureTriggerCmh2o = pacSettings->pressureTriggerCmh2o;
    lPlan.flowTriggerLpm = pacSettings->flowTriggerLpm;
    lPlan.cycleType = BREATH_CYCLE_TYPE_TIME;
    lPlan.riseTimeMs = (uint32_t)lEffectiveRiseTime;
    lPlan.holdTimeMs = pacSettings->inspiratoryTimeMs - lPlan.riseTimeMs;
    lPlan.minimumInspiratoryTimeMs = pacSettings->inspiratoryTimeMs;
    lPlan.maximumInspiratoryTimeMs = pacSettings->inspiratoryTimeMs;
    lPlan.expiratoryTimeMs = (uint32_t)NUMFILTER_MAX((float)BREATH_PEEP_LOCK_TIME_MS,
                                                     lBreathPeriodMs -
                                                     (float)pacSettings->inspiratoryTimeMs);
    lPlan.minimumExpiratoryTimeMs = BREATH_PEEP_LOCK_TIME_MS;
    lPlan.timeTriggerEnabled = 1U;
    *plan = lPlan;
}

/** Build the next-breath template from VAC settings. */
static void breathSchedulerVacPlanApply(const stVentVacSettings *vacSettings,
                                        stBreathPlan *plan)
{
    float lBreathPeriodMs = 60000.0F / vacSettings->freq;
    float lPauseTimeMs = (float)vacSettings->inspTimeMs * vacSettings->inspPausePct / 100.0F;
    float lFlowTimeMs = (float)vacSettings->inspTimeMs - lPauseTimeMs;
    stBreathPlan lPlan = {0};

    lPlan.mode = VENT_MD_VAC;
    lPlan.breathType = BREATH_TYPE_MANDATORY_VOLUME;
    lPlan.allowedTriggerType = vacSettings->triggerType;
    lPlan.peepCmh2o = vacSettings->peep;
    lPlan.inspiratoryFlowLpm = vacSettings->tidalVolume * 60.0F / lFlowTimeMs;
    lPlan.targetTidalVolumeMl = vacSettings->tidalVolume;
    lPlan.fio2Percent = vacSettings->oxygen;
    lPlan.pressureTriggerCmh2o = vacSettings->pressureTriggerCmh2o;
    lPlan.flowTriggerLpm = vacSettings->flowTriggerLpm;
    lPlan.cycleType = BREATH_CYCLE_TYPE_TIME;
    lPlan.riseTimeMs = (uint32_t)lFlowTimeMs;
    lPlan.holdTimeMs = (uint32_t)lPauseTimeMs;
    lPlan.minimumInspiratoryTimeMs = vacSettings->inspTimeMs;
    lPlan.maximumInspiratoryTimeMs = vacSettings->inspTimeMs;
    lPlan.expiratoryTimeMs = (uint32_t)NUMFILTER_MAX((float)BREATH_PEEP_LOCK_TIME_MS,
                                                     lBreathPeriodMs -
                                                     (float)vacSettings->inspTimeMs);
    lPlan.minimumExpiratoryTimeMs = BREATH_PEEP_LOCK_TIME_MS;
    lPlan.timeTriggerEnabled = 1U;
    *plan = lPlan;
}

/** Build one patient-triggered pressure-support breath template. */
static void breathSchedulerPsvPlanApply(eVentMode mode,
                                        float oxygenPercent,
                                        float peepCmh2o,
                                        float pressureLimitCmh2o,
                                        eVentTriggerType triggerType,
                                        float pressureTriggerCmh2o,
                                        float flowTriggerLpm,
                                        float pressureSupportCmh2o,
                                        uint32_t riseTimeMs,
                                        float cycleOffPercent,
                                        uint32_t maxInspiratoryTimeMs,
                                        uint32_t apneaTimeMs,
                                        uint32_t backupBreathIntervalMs,
                                        stBreathPlan *plan)
{
    stBreathPlan lPlan = {0};

    lPlan.mode = mode;
    lPlan.breathType = BREATH_TYPE_SPONTANEOUS_PRESSURE_SUPPORT;
    lPlan.allowedTriggerType = triggerType;
    lPlan.peepCmh2o = peepCmh2o;
    lPlan.inspiratoryPressureCmh2o = peepCmh2o + pressureSupportCmh2o;
    lPlan.fio2Percent = oxygenPercent;
    lPlan.pressureLimitCmh2o = pressureLimitCmh2o;
    lPlan.pressureTriggerCmh2o = pressureTriggerCmh2o;
    lPlan.flowTriggerLpm = flowTriggerLpm;
    lPlan.cycleType = BREATH_CYCLE_TYPE_FLOW;
    lPlan.cycleOffPercent = cycleOffPercent;
    lPlan.riseTimeMs = riseTimeMs;
    lPlan.minimumInspiratoryTimeMs = BREATH_PSV_MIN_INSPIRATORY_TIME_MS;
    lPlan.maximumInspiratoryTimeMs = maxInspiratoryTimeMs;
    lPlan.minimumExpiratoryTimeMs = BREATH_PEEP_LOCK_TIME_MS;
    lPlan.apneaTimeMs = apneaTimeMs;
    lPlan.backupBreathIntervalMs = backupBreathIntervalMs;
    lPlan.timeTriggerEnabled = 0U;
    *plan = lPlan;
}

/** Build the timed pressure-control breath used during PSV-ST backup. */
static void breathSchedulerPsvBackupPlanApply(const stVentPsvStSettings *settings,
                                              stBreathPlan *plan)
{
    stBreathPlan lPlan = {0};

    lPlan.mode = VENT_MD_PSV_ST;
    lPlan.breathType = BREATH_TYPE_MANDATORY_PRESSURE;
    lPlan.allowedTriggerType = settings->triggerType;
    lPlan.peepCmh2o = settings->peepCmh2o;
    lPlan.inspiratoryPressureCmh2o = settings->backupInspiratoryPressureCmh2o;
    lPlan.fio2Percent = settings->oxygenPercent;
    lPlan.pressureLimitCmh2o = settings->pressureLimitCmh2o;
    lPlan.pressureTriggerCmh2o = settings->pressureTriggerCmh2o;
    lPlan.flowTriggerLpm = settings->flowTriggerLpm;
    lPlan.cycleType = BREATH_CYCLE_TYPE_TIME;
    lPlan.riseTimeMs = settings->backupRiseTimeMs;
    lPlan.holdTimeMs = settings->backupInspiratoryTimeMs - settings->backupRiseTimeMs;
    lPlan.minimumInspiratoryTimeMs = settings->backupInspiratoryTimeMs;
    lPlan.maximumInspiratoryTimeMs = settings->backupInspiratoryTimeMs;
    lPlan.minimumExpiratoryTimeMs = BREATH_PEEP_LOCK_TIME_MS;
    lPlan.apneaTimeMs = settings->apneaTimeMs;
    lPlan.backupBreathIntervalMs =
        (uint32_t)(60000.0F / settings->backupRespiratoryRateBpm);
    lPlan.timeTriggerEnabled = 0U;
    *plan = lPlan;
}

int8_t breathSchedulerInit(void)
{
    gBreathRunning = false;
    gBreathMode = VENT_MD_IDLE;
    gBreathSettingsApplied = false;
    gBreathSequence = 0U;
    gBreathRunSequence = 0U;
    (void)memset(&gBreathPlanTemplate, 0, sizeof(gBreathPlanTemplate));
    (void)memset(&gBreathBackupPlanTemplate, 0,
                 sizeof(gBreathBackupPlanTemplate));
    gBreathBackupPlanValid = false;
    return BREATH_CONTROL_SUCCESS;
}

int8_t breathSchedulerStart(eVentMode mode)
{
    int8_t lStatus = breathSchedulerSettingsUpdate(mode);

    if (lStatus != BREATH_CONTROL_SUCCESS) {
        return lStatus;
    }
    repRtosEnterCritical();
    if (!gBreathRunning) {
        gBreathRunSequence++;
    }
    gBreathRunning = true;
    repRtosExitCritical();
    return BREATH_CONTROL_SUCCESS;
}

int8_t breathSchedulerStop(void)
{
    repRtosEnterCritical();
    gBreathRunning = false;
    repRtosExitCritical();
    return BREATH_CONTROL_SUCCESS;
}

int8_t breathSchedulerTestModeSet(uint8_t mode)
{
    if ((mode <= (uint8_t)VENT_MD_IDLE) || (mode >= (uint8_t)VENT_MD_COUNT)) {
        return BREATH_CONTROL_ERROR_PARAM;
    }
    return breathSchedulerSettingsUpdate((eVentMode)mode);
}

int8_t breathSchedulerTestRunSet(uint8_t run)
{
    if (run > 1U) {
        return BREATH_CONTROL_ERROR_PARAM;
    }
    if (run == 0U) {
        return breathSchedulerStop();
    }
    return breathSchedulerStart(breathSchedulerModeGet());
}

eVentMode breathSchedulerModeGet(void)
{
    eVentMode lMode;

    repRtosEnterCritical();
    lMode = gBreathMode;
    repRtosExitCritical();
    return lMode;
}

uint8_t breathSchedulerRunningGet(void)
{
    uint8_t lRunning;

    repRtosEnterCritical();
    lRunning = gBreathRunning ? 1U : 0U;
    repRtosExitCritical();
    return lRunning;
}

uint32_t breathSchedulerRunSequenceGet(void)
{
    uint32_t lSequence;

    repRtosEnterCritical();
    lSequence = gBreathRunSequence;
    repRtosExitCritical();
    return lSequence;
}

int8_t breathSchedulerSettingsUpdate(eVentMode mode)
{
    stVentPatientSettings lPatientSettings;
    const stVentLimitSettings *lLimitSettings;
    stVentPacSettings lPacSettings;
    stVentVacSettings lVacSettings;
    stVentCpapPsvSettings lCpapPsvSettings;
    stVentPsvStSettings lPsvStSettings;
    stBreathPlan lPlan;
    stBreathPlan lBackupPlan = {0};
    bool lBackupPlanValid = false;

    if ((mode <= VENT_MD_IDLE) || (mode >= VENT_MD_COUNT)) {
        return BREATH_CONTROL_ERROR_PARAM;
    }
    if ((mode != VENT_MD_PAC) &&
        (mode != VENT_MD_VAC) &&
        (mode != VENT_MD_CPAP_PSV) &&
        (mode != VENT_MD_PSV_ST)) {
        return BREATH_CONTROL_ERROR_UNSUPPORTED;
    }
    if ((calibrationIsValid(CALIBRATION_TYPE_ZERO) == 0U) ||
        (calibrationIsValid(CALIBRATION_TYPE_PRESSURE) == 0U)) {
        return BREATH_CONTROL_ERROR_SETTINGS;
    }
    if (((mode == VENT_MD_CPAP_PSV) || (mode == VENT_MD_PSV_ST)) &&
        (calibrationIsValid(CALIBRATION_TYPE_PROX_FLOW) == 0U)) {
        return BREATH_CONTROL_ERROR_SETTINGS;
    }

    lPatientSettings = *GetVentPatientSettings();
    lLimitSettings = GetVentLimitSettings();
    if (mode == VENT_MD_PAC) {
        lPacSettings = *GetVentPacSettings();
        breathSchedulerPacPlanApply(&lPacSettings, &lPlan);
    } else if (mode == VENT_MD_VAC) {
        lVacSettings = *GetVentVacSettings();
        breathSchedulerVacPlanApply(&lVacSettings, &lPlan);
    } else if (mode == VENT_MD_CPAP_PSV) {
        lCpapPsvSettings = *GetVentCpapPsvSettings();
        breathSchedulerPsvPlanApply(mode,
                                    lCpapPsvSettings.oxygenPercent,
                                    lCpapPsvSettings.peepCmh2o,
                                    lCpapPsvSettings.pressureLimitCmh2o,
                                    lCpapPsvSettings.triggerType,
                                    lCpapPsvSettings.pressureTriggerCmh2o,
                                    lCpapPsvSettings.flowTriggerLpm,
                                    lCpapPsvSettings.pressureSupportCmh2o,
                                    lCpapPsvSettings.riseTimeMs,
                                    lCpapPsvSettings.cycleOffPercent,
                                    lCpapPsvSettings.maxInspiratoryTimeMs,
                                    lCpapPsvSettings.apneaAlarmTimeMs,
                                    0U,
                                    &lPlan);
    } else {
        lPsvStSettings = *GetVentPsvStSettings();
        breathSchedulerPsvPlanApply(mode,
                                    lPsvStSettings.oxygenPercent,
                                    lPsvStSettings.peepCmh2o,
                                    lPsvStSettings.pressureLimitCmh2o,
                                    lPsvStSettings.triggerType,
                                    lPsvStSettings.pressureTriggerCmh2o,
                                    lPsvStSettings.flowTriggerLpm,
                                    lPsvStSettings.pressureSupportCmh2o,
                                    lPsvStSettings.riseTimeMs,
                                    lPsvStSettings.cycleOffPercent,
                                    lPsvStSettings.maxInspiratoryTimeMs,
                                    lPsvStSettings.apneaTimeMs,
                                    (uint32_t)(60000.0F /
                                               lPsvStSettings.backupRespiratoryRateBpm),
                                    &lPlan);
        breathSchedulerPsvBackupPlanApply(&lPsvStSettings, &lBackupPlan);
        lBackupPlanValid = true;
    }
    lPlan.limitSettings = lLimitSettings;
    if (lBackupPlanValid) {
        lBackupPlan.limitSettings = lLimitSettings;
    }
    repRtosEnterCritical();
    gBreathPlanTemplate = lPlan;
    gBreathBackupPlanTemplate = lBackupPlan;
    gBreathBackupPlanValid = lBackupPlanValid;
    if (mode == VENT_MD_PAC) {
        gBreathAppliedPacSettings = lPacSettings;
    } else if (mode == VENT_MD_VAC) {
        gBreathAppliedVacSettings = lVacSettings;
    } else if (mode == VENT_MD_CPAP_PSV) {
        gBreathAppliedCpapPsvSettings = lCpapPsvSettings;
    } else {
        gBreathAppliedPsvStSettings = lPsvStSettings;
    }
    gBreathAppliedPatientSettings = lPatientSettings;
    gBreathSettingsApplied = true;
    gBreathMode = mode;
    repRtosExitCritical();
    return BREATH_CONTROL_SUCCESS;
}

int8_t breathSchedulerNextPlanGet(eBreathTriggerReason triggerReason, stBreathPlan *plan)
{
    if ((plan == NULL) ||
        (triggerReason <= BREATH_TRIGGER_REASON_NONE) ||
        (triggerReason >= BREATH_TRIGGER_REASON_COUNT)) {
        return BREATH_CONTROL_ERROR_PARAM;
    }
    repRtosEnterCritical();
    if (!gBreathRunning || !gBreathSettingsApplied ||
        (gBreathPlanTemplate.breathType == BREATH_TYPE_NONE)) {
        repRtosExitCritical();
        return BREATH_CONTROL_ERROR_STATE;
    }

    if (triggerReason == BREATH_TRIGGER_REASON_APNEA_BACKUP) {
        if (!gBreathBackupPlanValid) {
            repRtosExitCritical();
            return BREATH_CONTROL_ERROR_STATE;
        }
        *plan = gBreathBackupPlanTemplate;
    } else {
        *plan = gBreathPlanTemplate;
    }
    gBreathSequence++;
    plan->sequence = gBreathSequence;
    plan->triggerReason = triggerReason;
    repRtosExitCritical();
    return BREATH_CONTROL_SUCCESS;
}

void breathSchedulerProcess(void)
{
    eVentMode lMode = breathSchedulerModeGet();
    bool lModeRunning = (breathSchedulerRunningGet() != 0U) &&
                        (lMode > VENT_MD_IDLE) &&
                        (lMode < VENT_MD_COUNT);
    bool lSettingsChanged = false;

    if (!lModeRunning) {
        return;
    }
    switch (lMode) {
        case VENT_MD_PAC:
            lSettingsChanged = breathSchedulerPacSettingsChanged();
            break;
        case VENT_MD_VAC:
            lSettingsChanged = breathSchedulerVacSettingsChanged();
            break;
        case VENT_MD_CPAP_PSV:
            lSettingsChanged = breathSchedulerCpapPsvSettingsChanged();
            break;
        case VENT_MD_PSV_ST:
            lSettingsChanged = breathSchedulerPsvStSettingsChanged();
            break;
        default:
            break;
    }

    if (lSettingsChanged) {
        /* Live edits take effect in the next-breath template. */
        (void)breathSchedulerSettingsUpdate(lMode);
    }
}
/**************************End of file********************************/
