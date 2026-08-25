/************************************************************************************
* @file     : settingdata.c
* @brief    : Ventilator setting data definitions.
***********************************************************************************/
#include "settingdata.h"

stVentLimitSettings gVentLimitSettings = {
    .pressureLow = 1.0f,
    .pressureHigh = 100.0f,
    .minuteVolumeLow = 0.1f,
    .minuteVolumeHigh = 100.0f,
    .tidalVolumeLow = 1U,
    .tidalVolumeHigh = 6000U,
    .o2PercentLow = 18U,
    .o2PercentHigh = 100U,
    .frequencyLow = 1U,
    .frequencyHigh = 160U,
    .apneaTimeHigh = 60U,
};

stVentPatientSettings gVentPatientSettings = {
    .Type = VENT_PATIENT_ADULT,
    .IdealBodyWeightKg = 70U,
    .IdealBodyHeightCm = 170U,
};

stVentPacSettings gVentPacSettings = {
    .oxygen = 21.0f,
    .peep = 5.0f,
    .Rate = 20.0f,
    .inspiratoryTimeMs = 1350U,
    .DeltaPressure = 25.0f,
    .riseTimeMs = 200U,
    .triggerType = VENT_TRIGGER_OFF,
    .pressureTriggerCmh2o = -2.0f,
    .flowTriggerLpm = 3.0f,
};

stVentVacSettings gVentVacSettings = {
    .oxygen = 21.0f,
    .peep = 5.0f,
    .freq = 15.0f,
    .inspTimeMs = 2500.0f,
    .tidalVolume = 250.0f,
    .triggerType = VENT_TRIGGER_OFF,
    .pressureTriggerCmh2o = -6.0f,
    .flowTriggerLpm = 3.0f,
    .inspPausePct = 40.0f,
};

stVentLimitSettings *GetVentLimitSettings(void)
{
    return &gVentLimitSettings;
}

stVentPatientSettings *GetVentPatientSettings(void)
{
    return &gVentPatientSettings;
}

stVentPacSettings *GetVentPacSettings(void)
{
    return &gVentPacSettings;
}

stVentVacSettings *GetVentVacSettings(void)
{
    return &gVentVacSettings;
}


/*************************************** End of file ********************************/
