/************************************************************************************
* @file     : breathscheduler.c
* @brief    : Breath scheduler.
* @details  : Validates mode settings and selects one immutable plan per breath.
* @author   :
* @date     :
* @version  :
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "breathscheduler.h"

#include <float.h>
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
static stBreathPlan gBreathPlanTemplate;
static stVentPatientSettings gBreathAppliedPatientSettings;
static stVentLimitSettings gBreathAppliedLimitSettings;
static stVentPacSettings gBreathAppliedPacSettings;
static stVentVacSettings gBreathAppliedVacSettings;

/** Return true for a finite single-precision value. */
static bool breathSchedulerFinite(float value)
{
    return (value >= -FLT_MAX) && (value <= FLT_MAX);
}

/** Return true when patient and common limit settings are valid. */
static bool breathSchedulerCommonSettingsValid(const stVentPatientSettings *patientSettings,
                                               const stVentLimitSettings *limitSettings)
{
    if ((patientSettings == NULL) || (limitSettings == NULL)) {
        return false;
    }
    return ((uint32_t)patientSettings->Type < (uint32_t)VENT_PATIENT_TYPE_COUNT) &&
           ((uint32_t)patientSettings->Gas < (uint32_t)VENT_GAS_COUNT) &&
           breathSchedulerFinite(limitSettings->pressureLow) &&
           breathSchedulerFinite(limitSettings->pressureHigh) &&
           breathSchedulerFinite(limitSettings->minuteVolumeLow) &&
           breathSchedulerFinite(limitSettings->minuteVolumeHigh) &&
           (limitSettings->pressureLow < limitSettings->pressureHigh) &&
           (limitSettings->minuteVolumeLow < limitSettings->minuteVolumeHigh);
}

/** Return true when a configured patient-trigger selection is valid. */
static bool breathSchedulerTriggerValid(eVentTriggerType triggerType,
                                        float pressureTriggerCmh2o,
                                        float flowTriggerLpm)
{
    if ((uint32_t)triggerType >= (uint32_t)VENT_TRIGGER_COUNT) {
        return false;
    }
    if ((triggerType == VENT_TRIGGER_PRESSURE) &&
        (!breathSchedulerFinite(pressureTriggerCmh2o) ||
         (pressureTriggerCmh2o == 0.0F))) {
        return false;
    }
    if ((triggerType == VENT_TRIGGER_FLOW) &&
        (!breathSchedulerFinite(flowTriggerLpm) || (flowTriggerLpm <= 0.0F))) {
        return false;
    }
    return true;
}

/** Return true when the PAC source settings differ from the applied snapshot. */
static bool breathSchedulerPacSettingsChanged(void)
{
    return !gBreathSettingsApplied ||
           (memcmp(&gBreathAppliedPatientSettings,
                   GetVentPatientSettings(),
                   sizeof(gBreathAppliedPatientSettings)) != 0) ||
           (memcmp(&gBreathAppliedLimitSettings,
                   GetVentLimitSettings(),
                   sizeof(gBreathAppliedLimitSettings)) != 0) ||
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
           (memcmp(&gBreathAppliedLimitSettings,
                   GetVentLimitSettings(),
                   sizeof(gBreathAppliedLimitSettings)) != 0) ||
           (memcmp(&gBreathAppliedVacSettings,
                   GetVentVacSettings(),
                   sizeof(gBreathAppliedVacSettings)) != 0);
}

/** Validate the PAC settings used to build a pressure breath plan. */
static bool breathSchedulerPacSettingsValid(const stVentPatientSettings *patientSettings,
                                            const stVentLimitSettings *limitSettings,
                                            const stVentPacSettings *pacSettings)
{
    const stCalibrationPressure *lPressureCalibration;
    float lBreathPeriodMs;
    float lCalibratedPressureMaximum;
    float lInspPressure;
    uint8_t lIndex;

    if (!breathSchedulerCommonSettingsValid(patientSettings, limitSettings) ||
        (pacSettings == NULL) ||
        !breathSchedulerFinite(pacSettings->oxygen) ||
        !breathSchedulerFinite(pacSettings->peep) ||
        !breathSchedulerFinite(pacSettings->Rate) ||
        !breathSchedulerFinite(pacSettings->DeltaPressure) ||
        (pacSettings->Rate <= 0.0F) ||
        (pacSettings->inspiratoryTimeMs == 0U) ||
        (pacSettings->riseTimeMs > pacSettings->inspiratoryTimeMs) ||
        (pacSettings->oxygen < (float)limitSettings->o2PercentLow) ||
        (pacSettings->oxygen > (float)limitSettings->o2PercentHigh) ||
        (pacSettings->Rate < (float)limitSettings->frequencyLow) ||
        (pacSettings->Rate > (float)limitSettings->frequencyHigh) ||
        (pacSettings->peep < limitSettings->pressureLow) ||
        (pacSettings->DeltaPressure < 0.0F) ||
        !breathSchedulerTriggerValid(pacSettings->triggerType,
                                     pacSettings->pressureTriggerCmh2o,
                                     pacSettings->flowTriggerLpm)) {
        return false;
    }

    lInspPressure = pacSettings->peep + pacSettings->DeltaPressure;
    lBreathPeriodMs = 60000.0F / pacSettings->Rate;
    lPressureCalibration = calibrationGetPressure();
    if (lPressureCalibration == NULL) {
        return false;
    }
    lCalibratedPressureMaximum = lPressureCalibration->pressureValues[0U];
    for (lIndex = 1U; lIndex < CALIBRATION_PRESSURE_POINT_COUNT; lIndex++) {
        if (lPressureCalibration->pressureValues[lIndex] > lCalibratedPressureMaximum) {
            lCalibratedPressureMaximum = lPressureCalibration->pressureValues[lIndex];
        }
    }
    return (lInspPressure <= limitSettings->pressureHigh) &&
           (lInspPressure <= (lCalibratedPressureMaximum - BREATH_PRESSURE_CONTROL_HEADROOM)) &&
           (lBreathPeriodMs >= ((float)pacSettings->inspiratoryTimeMs +
                                (float)BREATH_PEEP_LOCK_TIME_MS));
}

/** Build the next-breath template from validated PAC settings. */
static void breathSchedulerPacPlanApply(const stVentLimitSettings *limitSettings,
                                        const stVentPacSettings *pacSettings,
                                        stBreathPlan *plan)
{
    float lBreathPeriodMs = 60000.0F / pacSettings->Rate;
    float lEffectiveRiseTime = (float)pacSettings->riseTimeMs;
    stBreathPlan lPlan = {0};

    if (pacSettings->DeltaPressure > BREATH_RISE_DELTA_REFERENCE) {
        lEffectiveRiseTime += (pacSettings->DeltaPressure - BREATH_RISE_DELTA_REFERENCE) *
                              BREATH_RISE_EXTRA_MS_PER_CMH2O;
    }
    lEffectiveRiseTime = NUMFILTER_MIN((float)pacSettings->inspiratoryTimeMs,
                                       lEffectiveRiseTime);

    lPlan.mode = VENT_MD_PAC;
    lPlan.breathType = BREATH_TYPE_MANDATORY_PRESSURE;
    lPlan.allowedTriggerType = pacSettings->triggerType;
    lPlan.peepCmh2o = pacSettings->peep;
    lPlan.inspiratoryPressureCmh2o = pacSettings->peep + pacSettings->DeltaPressure;
    lPlan.fio2Percent = pacSettings->oxygen;
    lPlan.pressureLimitCmh2o = limitSettings->pressureHigh;
    lPlan.pressureTriggerCmh2o = pacSettings->pressureTriggerCmh2o;
    lPlan.flowTriggerLpm = pacSettings->flowTriggerLpm;
    lPlan.riseTimeMs = (uint32_t)lEffectiveRiseTime;
    lPlan.holdTimeMs = pacSettings->inspiratoryTimeMs - lPlan.riseTimeMs;
    lPlan.expiratoryTimeMs = (uint32_t)NUMFILTER_MAX((float)BREATH_PEEP_LOCK_TIME_MS,
                                                     lBreathPeriodMs -
                                                     (float)pacSettings->inspiratoryTimeMs);
    lPlan.minimumExpiratoryTimeMs = BREATH_PEEP_LOCK_TIME_MS;
    *plan = lPlan;
}

/** Validate the VAC settings used to build a volume breath plan. */
static bool breathSchedulerVacSettingsValid(const stVentPatientSettings *patientSettings,
                                            const stVentLimitSettings *limitSettings,
                                            const stVentVacSettings *vacSettings)
{
    float lBreathPeriodMs;
    float lMinuteVolumeLpm;

    if (!breathSchedulerCommonSettingsValid(patientSettings, limitSettings) ||
        (vacSettings == NULL) ||
        !breathSchedulerFinite(vacSettings->oxygen) ||
        !breathSchedulerFinite(vacSettings->peep) ||
        !breathSchedulerFinite(vacSettings->freq) ||
        !breathSchedulerFinite(vacSettings->tidalVolume) ||
        !breathSchedulerFinite(vacSettings->inspPausePct) ||
        (vacSettings->freq <= 0.0F) ||
        (vacSettings->inspTimeMs == 0U) ||
        (vacSettings->inspPausePct < 0.0F) ||
        (vacSettings->inspPausePct >= 100.0F) ||
        (vacSettings->oxygen < (float)limitSettings->o2PercentLow) ||
        (vacSettings->oxygen > (float)limitSettings->o2PercentHigh) ||
        (vacSettings->freq < (float)limitSettings->frequencyLow) ||
        (vacSettings->freq > (float)limitSettings->frequencyHigh) ||
        (vacSettings->peep < limitSettings->pressureLow) ||
        (vacSettings->peep > limitSettings->pressureHigh) ||
        (vacSettings->tidalVolume < (float)limitSettings->tidalVolumeLow) ||
        (vacSettings->tidalVolume > (float)limitSettings->tidalVolumeHigh) ||
        !breathSchedulerTriggerValid(vacSettings->triggerType,
                                     vacSettings->pressureTriggerCmh2o,
                                     vacSettings->flowTriggerLpm)) {
        return false;
    }

    lMinuteVolumeLpm = vacSettings->tidalVolume * vacSettings->freq / 1000.0F;
    lBreathPeriodMs = 60000.0F / vacSettings->freq;
    return (lMinuteVolumeLpm >= limitSettings->minuteVolumeLow) &&
           (lMinuteVolumeLpm <= limitSettings->minuteVolumeHigh) &&
           (lBreathPeriodMs >= ((float)vacSettings->inspTimeMs +
                                (float)BREATH_PEEP_LOCK_TIME_MS));
}

/** Build the next-breath template from validated VAC settings. */
static void breathSchedulerVacPlanApply(const stVentLimitSettings *limitSettings,
                                        const stVentVacSettings *vacSettings,
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
    lPlan.pressureLimitCmh2o = limitSettings->pressureHigh;
    lPlan.pressureTriggerCmh2o = vacSettings->pressureTriggerCmh2o;
    lPlan.flowTriggerLpm = vacSettings->flowTriggerLpm;
    lPlan.riseTimeMs = (uint32_t)lFlowTimeMs;
    lPlan.holdTimeMs = (uint32_t)lPauseTimeMs;
    lPlan.expiratoryTimeMs = (uint32_t)NUMFILTER_MAX((float)BREATH_PEEP_LOCK_TIME_MS,
                                                     lBreathPeriodMs -
                                                     (float)vacSettings->inspTimeMs);
    lPlan.minimumExpiratoryTimeMs = BREATH_PEEP_LOCK_TIME_MS;
    *plan = lPlan;
}

int8_t breathSchedulerInit(void)
{
    gBreathRunning = false;
    gBreathMode = VENT_MD_IDLE;
    gBreathSettingsApplied = false;
    gBreathSequence = 0U;
    (void)memset(&gBreathPlanTemplate, 0, sizeof(gBreathPlanTemplate));
    return BREATH_CONTROL_SUCCESS;
}

int8_t breathSchedulerStart(eVentMode mode)
{
    int8_t lStatus = breathSchedulerSettingsUpdate(mode);

    if (lStatus != BREATH_CONTROL_SUCCESS) {
        return lStatus;
    }
    repRtosEnterCritical();
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

int8_t breathSchedulerSettingsUpdate(eVentMode mode)
{
    stVentPatientSettings lPatientSettings;
    stVentLimitSettings lLimitSettings;
    stVentPacSettings lPacSettings;
    stVentVacSettings lVacSettings;
    stBreathPlan lPlan;

    if ((mode <= VENT_MD_IDLE) || (mode >= VENT_MD_COUNT)) {
        return BREATH_CONTROL_ERROR_PARAM;
    }
    if ((mode != VENT_MD_PAC) && (mode != VENT_MD_VAC)) {
        return BREATH_CONTROL_ERROR_UNSUPPORTED;
    }
    if ((calibrationIsValid(CALIBRATION_TYPE_ZERO) == 0U) ||
        (calibrationIsValid(CALIBRATION_TYPE_PRESSURE) == 0U)) {
        return BREATH_CONTROL_ERROR_SETTINGS;
    }

    lPatientSettings = *GetVentPatientSettings();
    lLimitSettings = *GetVentLimitSettings();
    if (mode == VENT_MD_PAC) {
        lPacSettings = *GetVentPacSettings();
        if (!breathSchedulerPacSettingsValid(&lPatientSettings, &lLimitSettings, &lPacSettings)) {
            return BREATH_CONTROL_ERROR_SETTINGS;
        }
        breathSchedulerPacPlanApply(&lLimitSettings, &lPacSettings, &lPlan);
    } else {
        lVacSettings = *GetVentVacSettings();
        if (!breathSchedulerVacSettingsValid(&lPatientSettings, &lLimitSettings, &lVacSettings)) {
            return BREATH_CONTROL_ERROR_SETTINGS;
        }
        breathSchedulerVacPlanApply(&lLimitSettings, &lVacSettings, &lPlan);
    }
    repRtosEnterCritical();
    gBreathPlanTemplate = lPlan;
    if (mode == VENT_MD_PAC) {
        gBreathAppliedPacSettings = lPacSettings;
    } else {
        gBreathAppliedVacSettings = lVacSettings;
    }
    gBreathAppliedPatientSettings = lPatientSettings;
    gBreathAppliedLimitSettings = lLimitSettings;
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

    *plan = gBreathPlanTemplate;
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
                        ((lMode == VENT_MD_PAC) || (lMode == VENT_MD_VAC));

    if (!lModeRunning) {
        return;
    }
    if (((lMode == VENT_MD_PAC) && breathSchedulerPacSettingsChanged()) ||
        ((lMode == VENT_MD_VAC) && breathSchedulerVacSettingsChanged())) {
        /* Invalid live edits leave the last validated next-breath template active. */
        (void)breathSchedulerSettingsUpdate(lMode);
    }
}
/**************************End of file********************************/
