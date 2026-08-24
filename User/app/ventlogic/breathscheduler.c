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

#include "numfilter.h"
#include "calibration.h"
#include "settingdata.h"

static volatile bool gBreathRunning = false;
static volatile eVentMode gBreathMode = VENT_MD_IDLE;
static float gBreathData[BREATH_COUNT];

/** Validate the PAC settings used to derive phase controller parameters. */
static bool breathSchedulerPacSettingsValid(const stVentPatientSettings *patientSettings,
                                            const stVentLimitSettings *limitSettings,
                                            const stVentPacSettings *pacSettings)
{
    float lBreathPeriodMs;
    float lInspPressure;

    if ((patientSettings == NULL) || (limitSettings == NULL) || (pacSettings == NULL)) {
        return false;
    }
    if (!isfinite(limitSettings->pressureLow) ||
        !isfinite(limitSettings->pressureHigh) ||
        (limitSettings->pressureLow > limitSettings->pressureHigh) ||
        (limitSettings->o2PercentLow > limitSettings->o2PercentHigh) ||
        (limitSettings->frequencyLow > limitSettings->frequencyHigh) ||
        !isfinite(pacSettings->oxygen) ||
        !isfinite(pacSettings->peep) ||
        !isfinite(pacSettings->Rate) ||
        !isfinite(pacSettings->DeltaPressure)) {
        return false;
    }
    if ((patientSettings->Type >= VENT_PATIENT_TYPE_COUNT) ||
        (patientSettings->IdealBodyWeightKg < 1U) ||
        (patientSettings->IdealBodyWeightKg > 300U) ||
        (patientSettings->IdealBodyHeightCm < 2U) ||
        (patientSettings->IdealBodyHeightCm > 440U)) {
        return false;
    }
    if ((pacSettings->oxygen < (float)limitSettings->o2PercentLow) ||
        (pacSettings->oxygen > (float)limitSettings->o2PercentHigh) ||
        (pacSettings->peep < limitSettings->pressureLow) ||
        (pacSettings->Rate < (float)limitSettings->frequencyLow) ||
        (pacSettings->Rate > (float)limitSettings->frequencyHigh) ||
        (pacSettings->DeltaPressure < 0.0F) ||
        (pacSettings->inspiratoryTimeMs == 0U) ||
        (pacSettings->riseTimeMs > pacSettings->inspiratoryTimeMs) ||
        (pacSettings->triggerType >= VENT_TRIGGER_COUNT)) {
        return false;
    }

    lInspPressure = pacSettings->peep + pacSettings->DeltaPressure;
    lBreathPeriodMs = 60000.0F / pacSettings->Rate;
    return (lInspPressure <= limitSettings->pressureHigh) &&
           (lBreathPeriodMs >= ((float)pacSettings->inspiratoryTimeMs + BREATH_PEEP_LOCK_TIME_MS));
}

/** Apply validated PAC settings to the phase controller data exchange. */
static void breathSchedulerPacSettingsApply(const stVentPatientSettings *patientSettings,
                                             const stVentPacSettings *pacSettings)
{
    float lBreathPeriodMs = 60000.0F / pacSettings->Rate;

    (void)breathControlSet(BREATH_PATIENT_TYPE, (float)patientSettings->Type);
    (void)breathControlSet(BREATH_IDEAL_BODY_WEIGHT, (float)patientSettings->IdealBodyWeightKg);
    (void)breathControlSet(BREATH_IDEAL_BODY_HEIGHT, (float)patientSettings->IdealBodyHeightCm);
    (void)breathControlSet(BREATH_PEEP_PRESSURE, pacSettings->peep);
    (void)breathControlSet(BREATH_INSP_PRESSURE, pacSettings->DeltaPressure + pacSettings->peep);
    (void)breathControlSet(BREATH_CONTROL_TYPE, (float)PRESSURE_CONTROL);
    (void)breathControlSet(BREATH_INSP_RISE_TIME,
                           NUMFILTER_MIN((float)pacSettings->inspiratoryTimeMs,
                                         (float)pacSettings->riseTimeMs));
    (void)breathControlSet(BREATH_INSP_HOLD_TIME,
                           NUMFILTER_MAX(0.0F,
                                         (float)(pacSettings->inspiratoryTimeMs - pacSettings->riseTimeMs)));
    (void)breathControlSet(BREATH_INSP_PEEP_TIME,
                           NUMFILTER_MAX(BREATH_PEEP_LOCK_TIME_MS,
                                         lBreathPeriodMs - (float)pacSettings->inspiratoryTimeMs));
    (void)breathControlSet(BREATH_FIO2, pacSettings->oxygen);
}

int8_t breathSchedulerInit(void)
{
    gBreathRunning = false;
    gBreathMode = VENT_MD_IDLE;
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
    stVentPatientSettings *lPatientSettings;
    stVentLimitSettings *lLimitSettings;
    stVentPacSettings *lPacSettings;

    if ((mode <= VENT_MD_IDLE) || (mode >= VENT_MD_COUNT)) {
        return BREATH_CONTROL_ERROR_PARAM;
    }
    if (mode != VENT_MD_PAC) {
        return BREATH_CONTROL_ERROR_UNSUPPORTED;
    }
    if ((calibrationIsValid(CALIBRATION_TYPE_ZERO) == 0U) ||
        (calibrationIsValid(CALIBRATION_TYPE_PRESSURE) == 0U)) {
        return BREATH_CONTROL_ERROR_SETTINGS;
    }

    lPatientSettings = GetVentPatientSettings();
    lLimitSettings = GetVentLimitSettings();
    lPacSettings = GetVentPacSettings();
    if (!breathSchedulerPacSettingsValid(lPatientSettings, lLimitSettings, lPacSettings)) {
        return BREATH_CONTROL_ERROR_SETTINGS;
    }

    breathSchedulerPacSettingsApply(lPatientSettings, lPacSettings);
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
    bool lPacRunning = gBreathRunning && (gBreathMode == VENT_MD_PAC);

    (void)breathControlSet(BREATH_RUN, lPacRunning ? 1.0F : 0.0F);
}
/**************************End of file********************************/
