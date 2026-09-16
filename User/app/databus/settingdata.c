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
    .apneaTimeAlarm = 60U,
};

stVentPatientSettings gVentPatientSettings = {
    .useHostSettings = 0U,
    .Type = VENT_PATIENT_ADULT,
    .Gas = VENT_GAS_BTPS,
    .IdealBodyWeightKg = 70U,
    .IdealBodyHeightCm = 170U,
};

stVentPacSettings gVentPacSettings = {
    .oxygen = 21.0f,
    .peep = 5.0f,
    .Rate = 15.0f,
    .inspiratoryTimeMs = 800U,
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
    .inspTimeMs = 2000.0f,
    .tidalVolume = 500.0f,
    .triggerType = VENT_TRIGGER_OFF,
    .pressureTriggerCmh2o = -6.0f,
    .flowTriggerLpm = 3.0f,
    .inspPausePct = 0.0f,
};

stVentCpapPsvSettings gVentCpapPsvSettings = {
    .oxygenPercent = 21.0f,
    .peepCmh2o = 5.0f,
    .triggerType = VENT_TRIGGER_PRESSURE,
    .pressureTriggerCmh2o = -2.0f,
    .flowTriggerLpm = 3.0f,
    .pressureSupportCmh2o = 10.0f,
    .riseTimeMs = 200U,
    .cycleOffPercent = 25.0f,
    .maxInspiratoryTimeMs = 2000U,
    .apneaPressureCmh2o = 20.0f,
    .apneaRateBpm = 15.0f,
    .apneaInspTimeMs = 1300U,
};

stVentPsvStSettings gVentPsvStSettings = {
    .oxygenPercent = 21.0f,
    .peepCmh2o = 5.0f,
    .triggerType = VENT_TRIGGER_PRESSURE,
    .pressureTriggerCmh2o = -2.0f,
    .flowTriggerLpm = 3.0f,
    .pressureSupportCmh2o = 10.0f,
    .riseTimeMs = 200U,
    .cycleOffPercent = 25.0f,
    .inspRateBpm = 15.0f,
    .inspTimeMs = 1300U,
    .maxInspiratoryTimeMs = 2000U,
};



stVentPSimvSettings gVentPSimvSettings = {
    .oxygenPercent = 21.0F, .peepCmh2o = 5.0F, .SIMVRateBpm = 10.0F,
    .inspiratoryTimeMs = 1000U, .inspiratoryPressureCmh2o = 20.0F,
    .pressureRiseTimeMs = 200U, .triggerType = VENT_TRIGGER_PRESSURE,
    .pressureTriggerCmh2o = -2.0F, .flowTriggerLpm = 3.0F,
    .pressureSupportCmh2o = 10.0F, .supportRiseTimeMs = 200U,
    .cycleOffPercent = 25.0F, .apneaSwitch = VENT_APNEA_OFF,
    .apneaPressureCmh2o = 20.0F, .apneaVolumeTidalMl = 500.0F,
    .apneaRateBpm = 15.0F, .apneaInspTimeMs = 1000U,
};

stVentVSimvSettings gVentVSimvSettings = {
    .oxygenPercent = 21.0F, .peepCmh2o = 5.0F, .SIMVRateBpm = 10.0F,
    .inspiratoryTimeMs = 1000U, .inspiratoryPressureCmh2o = 20.0F,
    .pressureRiseTimeMs = 200U, .triggerType = VENT_TRIGGER_PRESSURE,
    .pressureTriggerCmh2o = -2.0F, .flowTriggerLpm = 3.0F,
    .pressureSupportCmh2o = 10.0F, .supportRiseTimeMs = 200U,
    .cycleOffPercent = 25.0F, .apneaSwitch = VENT_APNEA_OFF,
    .apneaPressureCmh2o = 20.0F, .apneaVolumeTidalMl = 500.0F,
    .apneaRateBpm = 15.0F, .apneaInspTimeMs = 1000U,
    .tidalVolumeMl = 500.0F, .inspPausePct = 0.0F,
};

stVentPrvcSettings gVentPrvcSettings = {
    .oxygenPercent = 21.0F, .peepCmh2o = 5.0F, .respiratoryRateBpm = 15.0F,
    .inspiratoryTimeMs = 1000U, .triggerType = VENT_TRIGGER_OFF,
    .pressureTriggerCmh2o = -2.0F, .flowTriggerLpm = 3.0F,
    .targetTidalVolumeMl = 500.0F, .maximumPressureStepCmh2o = 10.0F,
};

static stVentPrvcSettings gHostVentPrvcSettings = {
    .oxygenPercent = 21.0F, .peepCmh2o = 5.0F, .respiratoryRateBpm = 15.0F,
    .inspiratoryTimeMs = 1000U, .triggerType = VENT_TRIGGER_OFF,
    .pressureTriggerCmh2o = -2.0F, .flowTriggerLpm = 3.0F,
    .targetTidalVolumeMl = 500.0F, .maximumPressureStepCmh2o = 10.0F,
};

stVentPrvcSimvSettings gVentPrvcSimvSettings = {
    .oxygenPercent = 21.0F, .peepCmh2o = 5.0F, .SIMVRateBpm = 10.0F,
    .inspiratoryTimeMs = 1000U, .triggerType = VENT_TRIGGER_PRESSURE,
    .pressureTriggerCmh2o = -2.0F, .flowTriggerLpm = 3.0F,
    .targetTidalVolumeMl = 500.0F, .supportPressureCmh2o = 10.0F,
    .maximumPressureStepCmh2o = 10.0F, .cycleOffPercent = 25.0F,
    .apneaSwitch = VENT_APNEA_OFF, .apneaPressureCmh2o = 20.0F,
    .apneaVolumeTidalMl = 500.0F, .apneaRateBpm = 15.0F, .apneaInspTimeMs = 1000U,
};

static stVentPrvcSimvSettings gHostVentPrvcSimvSettings = {
    .oxygenPercent = 21.0F, .peepCmh2o = 5.0F, .SIMVRateBpm = 10.0F,
    .inspiratoryTimeMs = 1000U, .triggerType = VENT_TRIGGER_PRESSURE,
    .pressureTriggerCmh2o = -2.0F, .flowTriggerLpm = 3.0F,
    .targetTidalVolumeMl = 500.0F, .supportPressureCmh2o = 10.0F,
    .maximumPressureStepCmh2o = 10.0F, .cycleOffPercent = 25.0F,
    .apneaSwitch = VENT_APNEA_OFF, .apneaPressureCmh2o = 20.0F,
    .apneaVolumeTidalMl = 500.0F, .apneaRateBpm = 15.0F, .apneaInspTimeMs = 1000U,
};

stVentVsSettings gVentVsSettings = {
    .oxygenPercent = 21.0F, .peepCmh2o = 5.0F, .targetTidalVolumeMl = 500.0F,
    .triggerType = VENT_TRIGGER_PRESSURE, .pressureTriggerCmh2o = -2.0F,
    .flowTriggerLpm = 3.0F, .riseTimeMs = 200U, .apneaSwitch = VENT_APNEA_PRESSURE,
    .apneaPressureCmh2o = 20.0F, .apneaVolumeTidalMl = 500.0F,
    .apneaRateBpm = 15.0F, .apneaInspTimeMs = 1000U, .apneaAlarmTimeMs = 15000U, .cycleOffPercent = 25.0F,
};

static stVentVsSettings gHostVentVsSettings = {
    .oxygenPercent = 21.0F, .peepCmh2o = 5.0F, .targetTidalVolumeMl = 500.0F,
    .triggerType = VENT_TRIGGER_PRESSURE, .pressureTriggerCmh2o = -2.0F,
    .flowTriggerLpm = 3.0F, .riseTimeMs = 200U, .apneaSwitch = VENT_APNEA_PRESSURE,
    .apneaPressureCmh2o = 20.0F, .apneaVolumeTidalMl = 500.0F,
    .apneaRateBpm = 15.0F, .apneaInspTimeMs = 1000U, .apneaAlarmTimeMs = 15000U, .cycleOffPercent = 25.0F,
};

stVentAprvSettings gVentAprvSettings = {
    .oxygenPercent = 21.0F, .pressureHighCmh2o = 20.0F, .pressureLowCmh2o = 5.0F,
    .timeHighMs = 4000U, .timeLowMs = 500U, .riseTimeMs = 200U,
    .triggerType = VENT_TRIGGER_PRESSURE, .pressureTriggerCmh2o = -2.0F, .flowTriggerLpm = 3.0F,
    .apneaSwitch = VENT_APNEA_PRESSURE, .apneaPressureCmh2o = 20.0F,
    .apneaVolumeTidalMl = 500.0F, .apneaRateBpm = 15.0F, .apneaInspTimeMs = 1000U,
};

static stVentAprvSettings gHostVentAprvSettings = {
    .oxygenPercent = 21.0F, .pressureHighCmh2o = 20.0F, .pressureLowCmh2o = 5.0F,
    .timeHighMs = 4000U, .timeLowMs = 500U, .riseTimeMs = 200U,
    .triggerType = VENT_TRIGGER_PRESSURE, .pressureTriggerCmh2o = -2.0F, .flowTriggerLpm = 3.0F,
    .apneaSwitch = VENT_APNEA_PRESSURE, .apneaPressureCmh2o = 20.0F,
    .apneaVolumeTidalMl = 500.0F, .apneaRateBpm = 15.0F, .apneaInspTimeMs = 1000U,
};

stVentBapapSettings gVentBapapSettings = {
    .oxygenPercent = 21.0F, .pressureHighCmh2o = 20.0F, .pressureLowCmh2o = 5.0F,
    .timeHighMs = 2000U, .timeLowMs = 4000U, .triggerType = VENT_TRIGGER_PRESSURE,
    .pressureTriggerCmh2o = -2.0F, .flowTriggerLpm = 3.0F,
    .pressureSupportCmh2o = 10.0F, .riseTimeMs = 200U,
    .cycleOffPercent = 25.0F, .maxInspiratoryTimeMs = 2000U,
    .apneaSwitch = VENT_APNEA_PRESSURE, .apneaPressureCmh2o = 20.0F,
    .apneaVolumeTidalMl = 500.0F, .apneaRateBpm = 15.0F, .apneaInspTimeMs = 1000U,
};

static stVentBapapSettings gHostVentBapapSettings = {
    .oxygenPercent = 21.0F, .pressureHighCmh2o = 20.0F, .pressureLowCmh2o = 5.0F,
    .timeHighMs = 2000U, .timeLowMs = 4000U, .triggerType = VENT_TRIGGER_PRESSURE,
    .pressureTriggerCmh2o = -2.0F, .flowTriggerLpm = 3.0F,
    .pressureSupportCmh2o = 10.0F, .riseTimeMs = 200U,
    .cycleOffPercent = 25.0F, .maxInspiratoryTimeMs = 2000U,
    .apneaSwitch = VENT_APNEA_PRESSURE, .apneaPressureCmh2o = 20.0F,
    .apneaVolumeTidalMl = 500.0F, .apneaRateBpm = 15.0F, .apneaInspTimeMs = 1000U,
};

static stVentPSimvSettings gHostVentPSimvSettings = {
    .oxygenPercent = 21.0F, .peepCmh2o = 5.0F, .SIMVRateBpm = 10.0F,
    .inspiratoryTimeMs = 1000U, .inspiratoryPressureCmh2o = 20.0F,
    .pressureRiseTimeMs = 200U, .triggerType = VENT_TRIGGER_PRESSURE,
    .pressureTriggerCmh2o = -2.0F, .flowTriggerLpm = 3.0F,
    .pressureSupportCmh2o = 10.0F, .supportRiseTimeMs = 200U,
    .cycleOffPercent = 25.0F, .apneaSwitch = VENT_APNEA_OFF,
    .apneaPressureCmh2o = 20.0F, .apneaVolumeTidalMl = 500.0F,
    .apneaRateBpm = 15.0F, .apneaInspTimeMs = 1000U,
};

static stVentVSimvSettings gHostVentVSimvSettings = {
    .oxygenPercent = 21.0F, .peepCmh2o = 5.0F, .SIMVRateBpm = 10.0F,
    .inspiratoryTimeMs = 1000U, .inspiratoryPressureCmh2o = 20.0F,
    .pressureRiseTimeMs = 200U, .triggerType = VENT_TRIGGER_PRESSURE,
    .pressureTriggerCmh2o = -2.0F, .flowTriggerLpm = 3.0F,
    .pressureSupportCmh2o = 10.0F, .supportRiseTimeMs = 200U,
    .cycleOffPercent = 25.0F, .apneaSwitch = VENT_APNEA_OFF,
    .apneaPressureCmh2o = 20.0F, .apneaVolumeTidalMl = 500.0F,
    .apneaRateBpm = 15.0F, .apneaInspTimeMs = 1000U,
    .tidalVolumeMl = 500.0F, .inspPausePct = 0.0F,
};

static stVentLimitSettings gHostVentLimitSettings = {
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
    .apneaTimeAlarm = 60U,
};
static stVentPacSettings gHostVentPacSettings = {
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
static stVentVacSettings gHostVentVacSettings = {
    .oxygen = 21.0f,
    .peep = 5.0f,
    .freq = 15.0f,
    .inspTimeMs = 2000.0f,
    .tidalVolume = 500.0f,
    .triggerType = VENT_TRIGGER_OFF,
    .pressureTriggerCmh2o = -6.0f,
    .flowTriggerLpm = 3.0f,
    .inspPausePct = 0.0f,
};
static stVentCpapPsvSettings gHostVentCpapPsvSettings = {
    .oxygenPercent = 21.0f,
    .peepCmh2o = 5.0f,
    .triggerType = VENT_TRIGGER_PRESSURE,
    .pressureTriggerCmh2o = -2.0f,
    .flowTriggerLpm = 3.0f,
    .pressureSupportCmh2o = 10.0f,
    .riseTimeMs = 200U,
    .cycleOffPercent = 25.0f,
    .maxInspiratoryTimeMs = 2000U,
    .apneaPressureCmh2o = 20.0f,
    .apneaRateBpm = 15.0f,
    .apneaInspTimeMs = 1300U,
};
static stVentPsvStSettings gHostVentPsvStSettings = {
    .oxygenPercent = 21.0f,
    .peepCmh2o = 5.0f,
    .triggerType = VENT_TRIGGER_PRESSURE,
    .pressureTriggerCmh2o = -2.0f,
    .flowTriggerLpm = 3.0f,
    .pressureSupportCmh2o = 10.0f,
    .riseTimeMs = 200U,
    .cycleOffPercent = 25.0f,
    .inspRateBpm = 15.0f,
    .inspTimeMs = 1300U,
    .maxInspiratoryTimeMs = 2000U,
};

stVentLimitSettings *GetVentLimitSettings(void)
{
    return gVentPatientSettings.useHostSettings == 1U ? &gHostVentLimitSettings : &gVentLimitSettings;
}

stVentPatientSettings *GetVentPatientSettings(void)
{
    return &gVentPatientSettings;
}

stVentPacSettings *GetVentPacSettings(void)
{
    return gVentPatientSettings.useHostSettings == 1U ? &gHostVentPacSettings : &gVentPacSettings;
}

stVentVacSettings *GetVentVacSettings(void)
{
    return gVentPatientSettings.useHostSettings == 1U ? &gHostVentVacSettings : &gVentVacSettings;
}

stVentCpapPsvSettings *GetVentCpapPsvSettings(void)
{
    return gVentPatientSettings.useHostSettings == 1U ? &gHostVentCpapPsvSettings : &gVentCpapPsvSettings;
}

stVentPsvStSettings *GetVentPsvStSettings(void)
{
    return gVentPatientSettings.useHostSettings == 1U ? &gHostVentPsvStSettings : &gVentPsvStSettings;
}


/** Select local or host P-SIMV settings. */
stVentPSimvSettings *GetVentPSimvSettings(void) {
    return gVentPatientSettings.useHostSettings == 1U ? &gHostVentPSimvSettings : &gVentPSimvSettings;
}

/** Select local or host V-SIMV settings. */
stVentVSimvSettings *GetVentVSimvSettings(void) {
    return gVentPatientSettings.useHostSettings == 1U ? &gHostVentVSimvSettings : &gVentVSimvSettings;
}

/** Select local or host PRVC settings. */
stVentPrvcSettings *GetVentPrvcSettings(void) {
    return gVentPatientSettings.useHostSettings == 1U ? &gHostVentPrvcSettings : &gVentPrvcSettings;
}

/** Select local or host PRVC-SIMV settings. */
stVentPrvcSimvSettings *GetVentPrvcSimvSettings(void) {
    return gVentPatientSettings.useHostSettings == 1U ? &gHostVentPrvcSimvSettings : &gVentPrvcSimvSettings;
}

/** Select local or host VS settings. */
stVentVsSettings *GetVentVsSettings(void) {
    return gVentPatientSettings.useHostSettings == 1U ? &gHostVentVsSettings : &gVentVsSettings;
}

/** Select local or host DuoLevel settings. */
stVentBapapSettings *GetVentBapapSettings(void) {
    return gVentPatientSettings.useHostSettings == 1U ? &gHostVentBapapSettings : &gVentBapapSettings;
}

/** Select local or host APRV settings. */
stVentAprvSettings *GetVentAprvSettings(void) {
    return gVentPatientSettings.useHostSettings == 1U ? &gHostVentAprvSettings : &gVentAprvSettings;
}

/*************************************** End of file ********************************/
