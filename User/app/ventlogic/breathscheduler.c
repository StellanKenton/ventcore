/************************************************************************************
* @file     : breathscheduler.c
* @brief    : Breath phase scheduler.
* @details  : Implements a tick-driven inspiration and expiration state machine.
* @author   :
* @date     :
* @version  :
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "breathscheduler.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "numfilter.h"
#include "calibration.h"
#include "settingdata.h"

static volatile bool gBreathRunning = false;
static volatile eVentMode gBreathMode = VENT_MD_IDLE;
static float gBreathData[BREATH_COUNT];
static bool gBreathSettingsApplied = false;
static stVentPatientSettings gBreathAppliedPatientSettings;
static stVentLimitSettings gBreathAppliedLimitSettings;
static stVentPacSettings gBreathAppliedPacSettings;
static stVentVacSettings gBreathAppliedVacSettings;

/** Return true when the PAC source settings differ from the last applied snapshot. */
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

/** Return true when the VAC source settings differ from the last applied snapshot. */
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

/** Validate the PAC settings used to derive phase controller parameters. */
static bool breathSchedulerPacSettingsValid(const stVentPatientSettings *patientSettings,
                                            const stVentLimitSettings *limitSettings,
                                            const stVentPacSettings *pacSettings)
{
    const stCalibrationPressure *lPressureCalibration;
    float lBreathPeriodMs;
    float lCalibratedPressureMaximum;
    float lInspPressure;
    uint8_t lIndex;

    if ((patientSettings == NULL) || (limitSettings == NULL) || (pacSettings == NULL)) {
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
           (lBreathPeriodMs >= ((float)pacSettings->inspiratoryTimeMs + BREATH_PEEP_LOCK_TIME_MS));
}

/** Apply validated PAC settings to the phase controller data exchange. */
static void breathSchedulerPacSettingsApply(const stVentPatientSettings *patientSettings,
                                             const stVentPacSettings *pacSettings)
{
    float lBreathPeriodMs = 60000.0F / pacSettings->Rate;
    float lEffectiveRiseTime = (float)pacSettings->riseTimeMs;

    if (pacSettings->DeltaPressure > BREATH_RISE_DELTA_REFERENCE) {
        lEffectiveRiseTime += (pacSettings->DeltaPressure - BREATH_RISE_DELTA_REFERENCE) *
                              BREATH_RISE_EXTRA_MS_PER_CMH2O;
    }
    lEffectiveRiseTime = NUMFILTER_MIN((float)pacSettings->inspiratoryTimeMs,
                                       lEffectiveRiseTime);

    (void)breathControlSet(BREATH_PATIENT_TYPE, (float)patientSettings->Type);
    (void)breathControlSet(BREATH_IDEAL_BODY_WEIGHT, (float)patientSettings->IdealBodyWeightKg);
    (void)breathControlSet(BREATH_IDEAL_BODY_HEIGHT, (float)patientSettings->IdealBodyHeightCm);
    (void)breathControlSet(BREATH_PEEP_PRESSURE, pacSettings->peep);
    (void)breathControlSet(BREATH_INSP_PRESSURE, pacSettings->DeltaPressure + pacSettings->peep);
    (void)breathControlSet(BREATH_CONTROL_TYPE, (float)PRESSURE_CONTROL);
    (void)breathControlSet(BREATH_INSP_RISE_TIME, lEffectiveRiseTime);
    (void)breathControlSet(BREATH_INSP_HOLD_TIME,
                           NUMFILTER_MAX(0.0F,
                                         (float)pacSettings->inspiratoryTimeMs - lEffectiveRiseTime));
    (void)breathControlSet(BREATH_INSP_PEEP_TIME,
                           NUMFILTER_MAX(BREATH_PEEP_LOCK_TIME_MS,
                                         lBreathPeriodMs - (float)pacSettings->inspiratoryTimeMs));
    (void)breathControlSet(BREATH_FIO2, pacSettings->oxygen);
    (void)breathControlSet(BREATH_TRIGGER_STATE, (float)pacSettings->triggerType);
    (void)breathControlSet(BREATH_TRIGGER_FLOW, pacSettings->flowTriggerLpm);
    (void)breathControlSet(BREATH_TRIGGER_PRESSURE, pacSettings->pressureTriggerCmh2o);
}

/** Validate the VAC settings used to derive phase controller parameters. */
static bool breathSchedulerVacSettingsValid(const stVentPatientSettings *patientSettings,
                                            const stVentLimitSettings *limitSettings,
                                            const stVentVacSettings *vacSettings)
{
    float lBreathPeriodMs;
    float lMinuteVolumeLpm;

    if ((patientSettings == NULL) || (limitSettings == NULL) || (vacSettings == NULL)) {
        return false;
    }

    lMinuteVolumeLpm = vacSettings->tidalVolume * vacSettings->freq / 1000.0F;
    lBreathPeriodMs = 60000.0F / vacSettings->freq;
    return (lMinuteVolumeLpm >= limitSettings->minuteVolumeLow) &&
           (lMinuteVolumeLpm <= limitSettings->minuteVolumeHigh) &&
           (lBreathPeriodMs >= ((float)vacSettings->inspTimeMs + BREATH_PEEP_LOCK_TIME_MS));
}

/** Apply validated VAC settings to the phase controller data exchange. */
static void breathSchedulerVacSettingsApply(const stVentPatientSettings *patientSettings,
                                             const stVentVacSettings *vacSettings)
{
    float lBreathPeriodMs = 60000.0F / vacSettings->freq;
    float lPauseTimeMs = (float)vacSettings->inspTimeMs * vacSettings->inspPausePct / 100.0F;
    float lFlowTimeMs = (float)vacSettings->inspTimeMs - lPauseTimeMs;
    float lInspiratoryFlowLpm = vacSettings->tidalVolume * 60.0F / lFlowTimeMs;

    (void)breathControlSet(BREATH_PATIENT_TYPE, (float)patientSettings->Type);
    (void)breathControlSet(BREATH_IDEAL_BODY_WEIGHT, (float)patientSettings->IdealBodyWeightKg);
    (void)breathControlSet(BREATH_IDEAL_BODY_HEIGHT, (float)patientSettings->IdealBodyHeightCm);
    (void)breathControlSet(BREATH_FLOW, lInspiratoryFlowLpm);
    (void)breathControlSet(BREATH_PEEP_PRESSURE, vacSettings->peep);
    (void)breathControlSet(BREATH_CONTROL_TYPE, (float)VOLUME_CONTROL);
    (void)breathControlSet(BREATH_INSP_RISE_TIME, lFlowTimeMs);
    (void)breathControlSet(BREATH_INSP_HOLD_TIME, lPauseTimeMs);
    (void)breathControlSet(BREATH_INSP_PEEP_TIME,
                           NUMFILTER_MAX(BREATH_PEEP_LOCK_TIME_MS,
                                         lBreathPeriodMs - (float)vacSettings->inspTimeMs));
    (void)breathControlSet(BREATH_FIO2, vacSettings->oxygen);
    (void)breathControlSet(BREATH_TIDAL_VOLUME, vacSettings->tidalVolume);
    (void)breathControlSet(BREATH_INSP_PAUSE_PERCENT, vacSettings->inspPausePct);
    (void)breathControlSet(BREATH_TRIGGER_STATE, (float)vacSettings->triggerType);
    (void)breathControlSet(BREATH_TRIGGER_FLOW, vacSettings->flowTriggerLpm);
    (void)breathControlSet(BREATH_TRIGGER_PRESSURE, vacSettings->pressureTriggerCmh2o);
}

int8_t breathSchedulerInit(void)
{
    gBreathRunning = false;
    gBreathMode = VENT_MD_IDLE;
    gBreathSettingsApplied = false;
    (void)breathControlSet(BREATH_RUN, 0.0F);
    return BREATH_CONTROL_SUCCESS;
}

int8_t breathSchedulerStart(eVentMode mode)
{
    int8_t lStatus = breathSchedulerSettingsUpdate(mode);

    if (lStatus != BREATH_CONTROL_SUCCESS) {
        return lStatus;
    }

    gBreathRunning = true;
    (void)breathControlSet(BREATH_RUN, 1.0F);
    return BREATH_CONTROL_SUCCESS;
}

int8_t breathSchedulerStop(void)
{
    gBreathRunning = false;
    (void)breathControlSet(BREATH_RUN, 0.0F);
    return BREATH_CONTROL_SUCCESS;
}

int8_t breathSchedulerTestModeSet(uint8_t mode)
{
    if (mode != (uint8_t)VENT_MD_PAC) {
        return BREATH_CONTROL_ERROR_PARAM;
    }

    return breathSchedulerSettingsUpdate(VENT_MD_PAC);
}

int8_t breathSchedulerTestRunSet(uint8_t run)
{
    if (run > 1U) {
        return BREATH_CONTROL_ERROR_PARAM;
    }

    if (run == 0U) {
        return breathSchedulerStop();
    }

    return breathSchedulerStart(gBreathMode);
}

eVentMode breathSchedulerModeGet(void)
{
    return gBreathMode;
}

uint8_t breathSchedulerRunningGet(void)
{
    return gBreathRunning ? 1U : 0U;
}

int8_t breathSchedulerSettingsUpdate(eVentMode mode)
{
    stVentPatientSettings lPatientSettings;
    stVentLimitSettings lLimitSettings;
    stVentPacSettings lPacSettings;
    stVentVacSettings lVacSettings;

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
        breathSchedulerPacSettingsApply(&lPatientSettings, &lPacSettings);
        gBreathAppliedPacSettings = lPacSettings;
    } else {
        lVacSettings = *GetVentVacSettings();
        if (!breathSchedulerVacSettingsValid(&lPatientSettings, &lLimitSettings, &lVacSettings)) {
            return BREATH_CONTROL_ERROR_SETTINGS;
        }
        breathSchedulerVacSettingsApply(&lPatientSettings, &lVacSettings);
        gBreathAppliedVacSettings = lVacSettings;
    }
    gBreathAppliedPatientSettings = lPatientSettings;
    gBreathAppliedLimitSettings = lLimitSettings;
    gBreathSettingsApplied = true;
    gBreathMode = mode;
    return BREATH_CONTROL_SUCCESS;
}

int8_t breathControlSet(eBreathControlType type, float value) {
    if ((type <= BREATH_NONE) || (type >= BREATH_COUNT)) {
        return BREATH_CONTROL_ERROR_PARAM;
    }

    gBreathData[type] = value;
    return BREATH_CONTROL_SUCCESS;
}

float breathControlGet(eBreathControlType type) {
    if ((type <= BREATH_NONE) || (type >= BREATH_COUNT)) {
        return 0.0F;
    }

    return gBreathData[type];
}

void breathSchedulerProcess(void)
{
    bool lModeRunning = gBreathRunning &&
                        ((gBreathMode == VENT_MD_PAC) || (gBreathMode == VENT_MD_VAC));

    if (lModeRunning &&
        (((gBreathMode == VENT_MD_PAC) && breathSchedulerPacSettingsChanged()) ||
         ((gBreathMode == VENT_MD_VAC) && breathSchedulerVacSettingsChanged()))) {
        /* Invalid live edits leave the last validated controller settings active. */
        (void)breathSchedulerSettingsUpdate(gBreathMode);
    }
    (void)breathControlSet(BREATH_RUN, lModeRunning ? 1.0F : 0.0F);
}
/**************************End of file********************************/
