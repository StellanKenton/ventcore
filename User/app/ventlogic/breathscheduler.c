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
#include <float.h>
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
static stBreathVolumeFeedback gBreathVolumeFeedback;

/** Bound a signed volume correction symmetrically. */
static float breathSchedulerVolumeClamp(float value, float limit) {
    return NUMFILTER_MAX(-limit, NUMFILTER_MIN(value, limit));
}

void breathSchedulerVolumeReset(void) {
    repRtosEnterCritical();
    (void)memset(&gBreathVolumeFeedback, 0, sizeof(gBreathVolumeFeedback));
    repRtosExitCritical();
}

void breathSchedulerVolumeFeedback(const stBreathPlan *plan, float vtiMl, uint8_t valid) {
    float lError;
    float lTarget;
    float lStep;
    float lGain = BREATH_VOLUME_CORRECTION_GAIN;
    float lStepLimit;
    float lCorrectionLimit;

    if (plan == NULL) {
        return;
    }
    repRtosEnterCritical();
    if (!gBreathRunning || (gBreathMode != VENT_MD_VAC) ||
        (plan->mode != VENT_MD_VAC) ||
        (plan->sequence != gBreathSequence) ||
        (plan->configurationSequence != gBreathPlanTemplate.configurationSequence) ||
        (gBreathVolumeFeedback.consumed &&
         (plan->sequence == gBreathVolumeFeedback.lastSequence))) {
        repRtosExitCritical();
        return;
    }
    gBreathVolumeFeedback.lastSequence = plan->sequence;
    gBreathVolumeFeedback.consumed = 1U;
    lTarget = gBreathPlanTemplate.targetTidalVolumeMl;
    if ((valid == 0U) || !(vtiMl > 0.0F && vtiMl <= FLT_MAX) ||
        !(lTarget > 0.0F && lTarget <= FLT_MAX) ||
        (plan->limitSettings == NULL) ||
        (plan->pressureLimitCmh2o != plan->limitSettings->pressureHigh)) {
        repRtosExitCritical();
        return;
    }
    if (gBreathVolumeFeedback.initialized == 0U) {
        lGain = BREATH_VOLUME_STARTUP_CORRECTION_GAIN;
        gBreathVolumeFeedback.filteredVtiMl = vtiMl;
        gBreathVolumeFeedback.filteredAppliedCorrectionMl = plan->volumeCorrectionMl;
        /* Re-zeroing may reset learning after the initial plan was already loaded. */
        gBreathVolumeFeedback.pressureLimitCmh2o = plan->pressureLimitCmh2o;
        gBreathVolumeFeedback.initialized = 1U;
    } else {
        gBreathVolumeFeedback.filteredVtiMl += BREATH_VOLUME_FILTER_ALPHA *
            (vtiMl - gBreathVolumeFeedback.filteredVtiMl);
        gBreathVolumeFeedback.filteredAppliedCorrectionMl += BREATH_VOLUME_FILTER_ALPHA *
            (plan->volumeCorrectionMl - gBreathVolumeFeedback.filteredAppliedCorrectionMl);
    }
    /* Remove the EMA lag of corrections already applied to the measured breaths. */
    lError = lTarget - gBreathVolumeFeedback.filteredVtiMl -
        (gBreathVolumeFeedback.correctionMl - gBreathVolumeFeedback.filteredAppliedCorrectionMl);
    if ((lError > lTarget * BREATH_VOLUME_ERROR_DEADBAND_RATIO) ||
        (lError < -lTarget * BREATH_VOLUME_ERROR_DEADBAND_RATIO)) {
        lStepLimit = lTarget * BREATH_VOLUME_CORRECTION_STEP_RATIO;
        lCorrectionLimit = lTarget * BREATH_VOLUME_CORRECTION_LIMIT_RATIO;
#if !BREATH_VOLUME_FLOW_COMPENSATION_ENABLE
        /* Store time adaptation as equivalent mL for the existing EMA feedback. */
        lStepLimit = plan->inspiratoryFlowLpm * BREATH_VOLUME_TIME_STEP_MS / 60.0F;
        lCorrectionLimit = plan->inspiratoryFlowLpm *
            (float)gBreathPlanTemplate.riseTimeMs * BREATH_VOLUME_TIME_LIMIT_RATIO / 60.0F;
#endif
        lStep = breathSchedulerVolumeClamp(lGain * lError, lStepLimit);
        gBreathVolumeFeedback.correctionMl = breathSchedulerVolumeClamp(
            gBreathVolumeFeedback.correctionMl + lStep, lCorrectionLimit);
    }
    repRtosExitCritical();
}

/** Convert an internal delivery volume using the existing rise/startup model. */
static float breathSchedulerVacFlowCalculate(float deliveryTargetMl, float flowTimeMs) {
    float lSafeTimeMs = NUMFILTER_MAX(BREATH_VOLUME_MIN_EFFECTIVE_FLOW_TIME_MS, flowTimeMs);
#if BREATH_VOLUME_FLOW_COMPENSATION_ENABLE
    float lEffectiveTimeMs = NUMFILTER_MAX(BREATH_VOLUME_MIN_EFFECTIVE_FLOW_TIME_MS,
                                          lSafeTimeMs - BREATH_VOLUME_FLOW_RISE_AREA_LOSS_MS);
    float lStartupLossMl = NUMFILTER_MIN(BREATH_VOLUME_STARTUP_VOLUME_LOSS_MAX_ML,
                                        BREATH_VOLUME_STARTUP_LOSS_TIME_ML_MS / lSafeTimeMs);
    return (deliveryTargetMl + lStartupLossMl) * 60.0F / lEffectiveTimeMs;
#else
    return deliveryTargetMl * 60.0F / lSafeTimeMs;
#endif
}

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
    /* Select nominal flow or the compile-time legacy startup compensation. */
    lPlan.inspiratoryFlowLpm = breathSchedulerVacFlowCalculate(vacSettings->tidalVolume,
                                                              lFlowTimeMs);
    lPlan.targetTidalVolumeMl = vacSettings->tidalVolume;
    lPlan.deliveryTargetMl = vacSettings->tidalVolume;
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
    lPlan.inspiratoryPressureCmh2o = settings->peepCmh2o + settings->pressureSupportCmh2o;
    lPlan.fio2Percent = settings->oxygenPercent;
    lPlan.pressureLimitCmh2o = GetVentLimitSettings()->pressureHigh;
    lPlan.pressureTriggerCmh2o = settings->pressureTriggerCmh2o;
    lPlan.flowTriggerLpm = settings->flowTriggerLpm;
    lPlan.cycleType = BREATH_CYCLE_TYPE_TIME;
    lPlan.riseTimeMs = NUMFILTER_MIN(settings->riseTimeMs, settings->apneaInspTimeMs);
    lPlan.holdTimeMs = settings->apneaInspTimeMs - NUMFILTER_MIN(settings->riseTimeMs, settings->apneaInspTimeMs);
    lPlan.minimumInspiratoryTimeMs = settings->apneaInspTimeMs;
    lPlan.maximumInspiratoryTimeMs = settings->apneaInspTimeMs;
    lPlan.minimumExpiratoryTimeMs = BREATH_PEEP_LOCK_TIME_MS;
    lPlan.apneaTimeMs = (uint32_t)GetVentLimitSettings()->apneaTimeHigh * 1000U;
    lPlan.backupBreathIntervalMs =
        (uint32_t)(60000.0F / settings->apneaRateBpm);
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
    breathSchedulerVolumeReset();
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
        gBreathPlanTemplate.configurationSequence++;
        breathSchedulerVolumeReset();
    }
    gBreathRunning = true;
    repRtosExitCritical();
    return BREATH_CONTROL_SUCCESS;
}

int8_t breathSchedulerStop(void)
{
    repRtosEnterCritical();
    gBreathRunning = false;
    breathSchedulerVolumeReset();
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
        /* CPAP permits zero support; PSV requires bounded flow-cycle timing. */
        if (!(lCpapPsvSettings.oxygenPercent >= 21.0F && lCpapPsvSettings.oxygenPercent <= 100.0F) ||
            !(lCpapPsvSettings.peepCmh2o >= 0.0F && lCpapPsvSettings.peepCmh2o <= 100.0F) ||
            !(lCpapPsvSettings.pressureSupportCmh2o >= 0.0F &&
              lCpapPsvSettings.peepCmh2o + lCpapPsvSettings.pressureSupportCmh2o < lCpapPsvSettings.pressureLimitCmh2o) ||
            !(lCpapPsvSettings.pressureLimitCmh2o <= 100.0F) ||
            (lCpapPsvSettings.maxInspiratoryTimeMs < BREATH_PSV_MIN_INSPIRATORY_TIME_MS) ||
            (lCpapPsvSettings.maxInspiratoryTimeMs > 10000U) ||
            (lCpapPsvSettings.riseTimeMs > lCpapPsvSettings.maxInspiratoryTimeMs) ||
            !(lCpapPsvSettings.cycleOffPercent > 0.0F && lCpapPsvSettings.cycleOffPercent < 100.0F) ||
            ((unsigned int)lCpapPsvSettings.triggerType >= VENT_TRIGGER_COUNT) ||
            ((lCpapPsvSettings.triggerType == VENT_TRIGGER_PRESSURE) &&
             !(lCpapPsvSettings.pressureTriggerCmh2o < 0.0F && lCpapPsvSettings.pressureTriggerCmh2o >= -100.0F)) ||
            ((lCpapPsvSettings.triggerType == VENT_TRIGGER_FLOW) &&
             !(lCpapPsvSettings.flowTriggerLpm > 0.0F && lCpapPsvSettings.flowTriggerLpm <= 200.0F))) {
            return BREATH_CONTROL_ERROR_SETTINGS;
        }
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
        /* Reject invalid values before division, conversion or plan publication. */
        if (!(lPsvStSettings.apneaRateBpm >= 1.0F && lPsvStSettings.apneaRateBpm <= 160.0F) ||
            (lPsvStSettings.apneaInspTimeMs < BREATH_PSV_MIN_INSPIRATORY_TIME_MS) ||
            ((float)lPsvStSettings.apneaInspTimeMs + BREATH_PEEP_LOCK_TIME_MS >
             60000.0F / lPsvStSettings.apneaRateBpm) ||
            !(lPsvStSettings.oxygenPercent >= 21.0F && lPsvStSettings.oxygenPercent <= 100.0F) ||
            !(lPsvStSettings.peepCmh2o >= 0.0F && lPsvStSettings.peepCmh2o <= 100.0F) ||
            !(lPsvStSettings.pressureSupportCmh2o > 0.0F &&
              lPsvStSettings.peepCmh2o + lPsvStSettings.pressureSupportCmh2o < lLimitSettings->pressureHigh) ||
            !(lLimitSettings->pressureHigh <= 100.0F) ||
            (lLimitSettings->apneaTimeHigh == 0U) || (lLimitSettings->apneaTimeHigh > 60U) ||
            (lPsvStSettings.riseTimeMs > BREATH_PSV_MAX_INSPIRATORY_TIME_MS) ||
            !(lPsvStSettings.cycleOffPercent > 0.0F && lPsvStSettings.cycleOffPercent < 100.0F) ||
            ((unsigned int)lPsvStSettings.triggerType >= VENT_TRIGGER_COUNT) ||
            ((lPsvStSettings.triggerType == VENT_TRIGGER_PRESSURE) &&
             !(lPsvStSettings.pressureTriggerCmh2o < 0.0F && lPsvStSettings.pressureTriggerCmh2o >= -100.0F)) ||
            ((lPsvStSettings.triggerType == VENT_TRIGGER_FLOW) &&
             !(lPsvStSettings.flowTriggerLpm > 0.0F && lPsvStSettings.flowTriggerLpm <= 200.0F))) {
            return BREATH_CONTROL_ERROR_SETTINGS;
        }
        breathSchedulerPsvPlanApply(mode,
                                    lPsvStSettings.oxygenPercent,
                                    lPsvStSettings.peepCmh2o,
                                    lLimitSettings->pressureHigh,
                                    lPsvStSettings.triggerType,
                                    lPsvStSettings.pressureTriggerCmh2o,
                                    lPsvStSettings.flowTriggerLpm,
                                    lPsvStSettings.pressureSupportCmh2o,
                                    lPsvStSettings.riseTimeMs,
                                    lPsvStSettings.cycleOffPercent,
                                    BREATH_PSV_MAX_INSPIRATORY_TIME_MS,
                                    (uint32_t)lLimitSettings->apneaTimeHigh * 1000U,
                                    (uint32_t)(60000.0F /
                                               lPsvStSettings.apneaRateBpm),
                                    &lPlan);
        breathSchedulerPsvBackupPlanApply(&lPsvStSettings, &lBackupPlan);
        lBackupPlanValid = true;
    }
    lPlan.limitSettings = lLimitSettings;
    if (lBackupPlanValid) {
        lBackupPlan.limitSettings = lLimitSettings;
    }
    repRtosEnterCritical();
    lPlan.configurationSequence = gBreathPlanTemplate.configurationSequence + 1U;
    breathSchedulerVolumeReset();
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
    if (plan->mode == VENT_MD_VAC) {
        if (gBreathVolumeFeedback.pressureLimitCmh2o != plan->limitSettings->pressureHigh) {
            breathSchedulerVolumeReset();
            gBreathVolumeFeedback.pressureLimitCmh2o = plan->limitSettings->pressureHigh;
        }
        plan->pressureLimitCmh2o = plan->limitSettings->pressureHigh;
        plan->filteredVtiMl = gBreathVolumeFeedback.filteredVtiMl;
        plan->volumeCorrectionMl = gBreathVolumeFeedback.correctionMl;
        plan->deliveryTargetMl = plan->targetTidalVolumeMl + plan->volumeCorrectionMl;
#if BREATH_VOLUME_FLOW_COMPENSATION_ENABLE
        /* Recompute from the user target; never compound the previous flow request. */
        plan->inspiratoryFlowLpm = breathSchedulerVacFlowCalculate(
            plan->deliveryTargetMl,
            (float)gBreathAppliedVacSettings.inspTimeMs -
            ((float)gBreathAppliedVacSettings.inspTimeMs *
             gBreathAppliedVacSettings.inspPausePct / 100.0F));
#else
        /* Extend only delivery, preserve the pause and total mandatory period. */
        float lTimeMs = plan->volumeCorrectionMl * 60.0F / plan->inspiratoryFlowLpm;
        int32_t lDeltaMs;
        lTimeMs = NUMFILTER_MIN(lTimeMs,
            (float)(plan->expiratoryTimeMs - plan->minimumExpiratoryTimeMs));
        lTimeMs = NUMFILTER_MAX(lTimeMs, 1.0F - (float)plan->riseTimeMs);
        lDeltaMs = (int32_t)(lTimeMs + ((lTimeMs >= 0.0F) ? 0.5F : -0.5F));
        plan->riseTimeMs = (uint32_t)((int32_t)plan->riseTimeMs + lDeltaMs);
        plan->minimumInspiratoryTimeMs = (uint32_t)((int32_t)plan->minimumInspiratoryTimeMs + lDeltaMs);
        plan->maximumInspiratoryTimeMs = plan->minimumInspiratoryTimeMs;
        plan->expiratoryTimeMs = (uint32_t)((int32_t)plan->expiratoryTimeMs - lDeltaMs);
        plan->volumeCorrectionMl = (float)lDeltaMs * plan->inspiratoryFlowLpm / 60.0F;
        plan->deliveryTargetMl = plan->targetTidalVolumeMl + plan->volumeCorrectionMl;
        /* Track the applied bound to prevent windup when expiration has no room. */
        gBreathVolumeFeedback.correctionMl = plan->volumeCorrectionMl;
#endif
    }
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
