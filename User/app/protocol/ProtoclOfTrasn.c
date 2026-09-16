/************************************************************************************
* @file     : ProtoclOfTrasn.c
* @brief    : Received protocol settings transfer.
* @details  : Converts cached MCM values into ventilator runtime settings.
* @author   :
* @date     :
* @version  :
* @copyright: Copyright (c) 2050
***********************************************************************************/

#include "ProtoclOfMcm.h"

#include "breathscheduler.h"
#include "log.h"
#include "rtos.h"

static bool gProtocolSettingsDirty;
static bool gProtocolAlarmLimitsDirty;
static bool gProtocolCommandPending;

/** Mark received ventilation parameters for transfer. */
void protocolReceivedSettingsMark(void) {
    gProtocolSettingsDirty = true;
}

/** Apply received alarm limits independently of the ventilation settings source. */
void protocolReceivedAlarmLimitsMark(void) {
    gProtocolAlarmLimitsDirty = true;
}

/** Mark a received ventilation command for execution in VentTask. */
void protocolReceivedCommandMark(void) {
    gProtocolCommandPending = true;
}

/** Apply cached host fields in VentTask before the control chain runs. */
void protocolApplyReceivedSettings(void) {
    static uint8_t gLastSource;
    static stVentPatientSettings gLocalPatient;
    uint8_t lSource = GetVentPatientSettings()->useHostSettings == 1U;
    int8_t lStatus = BREATH_CONTROL_SUCCESS;

    repRtosEnterCritical();
    bool lChanged = (lSource && gProtocolSettingsDirty) ||
                    gProtocolAlarmLimitsDirty || lSource != gLastSource;
    if (lSource != gLastSource) {
        if (lSource) {
            gLocalPatient = *GetVentPatientSettings();
        } else {
            *GetVentPatientSettings() = gLocalPatient;
            GetVentPatientSettings()->useHostSettings = 0U;
        }
    }
    if (lSource && lChanged) {
        const RxVentParamsCache_t *lParams = ProtocolGetRxVentParamsCache();
        stVentPacSettings *lPac = GetVentPacSettings();
        stVentVacSettings *lVac = GetVentVacSettings();
        stVentCpapPsvSettings *lPsv = GetVentCpapPsvSettings();
        stVentPsvStSettings *lSt = GetVentPsvStSettings();
        stVentPSimvSettings *lPSimv = GetVentPSimvSettings();
        stVentVSimvSettings *lVSimv = GetVentVSimvSettings();
        stVentPrvcSettings *lPrvc = GetVentPrvcSettings();
        stVentPrvcSimvSettings *lPrvcSimv = GetVentPrvcSimvSettings();
        stVentVsSettings *lVs = GetVentVsSettings();
        stVentBapapSettings *lBapap = GetVentBapapSettings();

        if (ProtocolGetRxVentSwitchCache()->m_valid[0x0A]) {
            lPSimv->apneaSwitch = (eVentApneaType)ProtocolGetRxVentSwitchCache()->m_ApneaVentSwitch;
            lVSimv->apneaSwitch = lPSimv->apneaSwitch;
            lPrvcSimv->apneaSwitch = lPSimv->apneaSwitch;
            lBapap->apneaSwitch = lPSimv->apneaSwitch;
            lVs->apneaSwitch = lPSimv->apneaSwitch;
        }
        if (lParams->m_valid[0x06]) {
            lPac->oxygen = (float)lParams->m_fio2 / ProtocolGetScale(lParams->m_fio2_scale);
            lVac->oxygen = (float)lParams->m_fio2 / ProtocolGetScale(lParams->m_fio2_scale);
            lPrvc->oxygenPercent = (float)lParams->m_fio2 / ProtocolGetScale(lParams->m_fio2_scale);
            lPsv->oxygenPercent = (float)lParams->m_fio2 / ProtocolGetScale(lParams->m_fio2_scale);
            lSt->oxygenPercent = (float)lParams->m_fio2 / ProtocolGetScale(lParams->m_fio2_scale);
            lPSimv->oxygenPercent = (float)lParams->m_fio2 / ProtocolGetScale(lParams->m_fio2_scale);
            lVSimv->oxygenPercent = (float)lParams->m_fio2 / ProtocolGetScale(lParams->m_fio2_scale);
            lPrvcSimv->oxygenPercent = (float)lParams->m_fio2 / ProtocolGetScale(lParams->m_fio2_scale);
            lBapap->oxygenPercent = (float)lParams->m_fio2 / ProtocolGetScale(lParams->m_fio2_scale);
            lVs->oxygenPercent = (float)lParams->m_fio2 / ProtocolGetScale(lParams->m_fio2_scale);
        }
        if (lParams->m_valid[0x0A]) {
            lBapap->pressureHighCmh2o = (float)lParams->m_pHigh / ProtocolGetScale(lParams->m_pHigh_scale);
        }
        if (lParams->m_valid[0x0B]) {
            lBapap->pressureLowCmh2o = (float)lParams->m_pLow / ProtocolGetScale(lParams->m_pLow_scale);
        }
        if (lParams->m_valid[0x1B]) {
            lBapap->timeHighMs = (uint32_t)((float)lParams->m_tHigh * 1000.0F / ProtocolGetScale(lParams->m_tHigh_scale));
        }
        if (lParams->m_valid[0x1C]) {
            lBapap->timeLowMs = (uint32_t)((float)lParams->m_tLow * 1000.0F / ProtocolGetScale(lParams->m_tLow_scale));
        }
        if (lParams->m_valid[0x07]) {
            lPac->DeltaPressure = (float)lParams->m_deltaPinsp / ProtocolGetScale(lParams->m_deltaPinsp_scale);
            lPSimv->inspiratoryPressureCmh2o = (float)lParams->m_deltaPinsp / ProtocolGetScale(lParams->m_deltaPinsp_scale);
        }
        if (lParams->m_valid[0x08]) {
            lPac->peep = (float)lParams->m_peep / ProtocolGetScale(lParams->m_peep_scale);
            lVac->peep = (float)lParams->m_peep / ProtocolGetScale(lParams->m_peep_scale);
            lPrvc->peepCmh2o = (float)lParams->m_peep / ProtocolGetScale(lParams->m_peep_scale);
            lPsv->peepCmh2o = (float)lParams->m_peep / ProtocolGetScale(lParams->m_peep_scale);
            lSt->peepCmh2o = (float)lParams->m_peep / ProtocolGetScale(lParams->m_peep_scale);
            lPSimv->peepCmh2o = (float)lParams->m_peep / ProtocolGetScale(lParams->m_peep_scale);
            lVSimv->peepCmh2o = (float)lParams->m_peep / ProtocolGetScale(lParams->m_peep_scale);
            lPrvcSimv->peepCmh2o = (float)lParams->m_peep / ProtocolGetScale(lParams->m_peep_scale);
            lVs->peepCmh2o = (float)lParams->m_peep / ProtocolGetScale(lParams->m_peep_scale);
        }
        if (lParams->m_valid[0x09]) {
            lPsv->pressureSupportCmh2o = (float)lParams->m_deltaPsupp / ProtocolGetScale(lParams->m_deltaPsupp_scale);
            lBapap->pressureSupportCmh2o = (float)lParams->m_deltaPsupp / ProtocolGetScale(lParams->m_deltaPsupp_scale);
            lSt->pressureSupportCmh2o = (float)lParams->m_deltaPsupp / ProtocolGetScale(lParams->m_deltaPsupp_scale);
            lPSimv->pressureSupportCmh2o = (float)lParams->m_deltaPsupp / ProtocolGetScale(lParams->m_deltaPsupp_scale);
            lVSimv->pressureSupportCmh2o = (float)lParams->m_deltaPsupp / ProtocolGetScale(lParams->m_deltaPsupp_scale);
            lPrvcSimv->supportPressureCmh2o = (float)lParams->m_deltaPsupp / ProtocolGetScale(lParams->m_deltaPsupp_scale);
        }
        if (lParams->m_valid[0x0C]) {
            lPsv->apneaPressureCmh2o = (float)lParams->m_deltaApneaP / ProtocolGetScale(lParams->m_deltaApneaP_scale);
            lPSimv->apneaPressureCmh2o = (float)lParams->m_deltaApneaP / ProtocolGetScale(lParams->m_deltaApneaP_scale);
            lVSimv->apneaPressureCmh2o = (float)lParams->m_deltaApneaP / ProtocolGetScale(lParams->m_deltaApneaP_scale);
            lPrvcSimv->apneaPressureCmh2o = (float)lParams->m_deltaApneaP / ProtocolGetScale(lParams->m_deltaApneaP_scale);
            lBapap->apneaPressureCmh2o = (float)lParams->m_deltaApneaP / ProtocolGetScale(lParams->m_deltaApneaP_scale);
            lVs->apneaPressureCmh2o = (float)lParams->m_deltaApneaP / ProtocolGetScale(lParams->m_deltaApneaP_scale);
        }
        if (lParams->m_valid[0x0E]) {
            lVac->tidalVolume = (float)lParams->m_tidalVolume / ProtocolGetScale(lParams->m_tidalVolume_scale);
            lPrvc->targetTidalVolumeMl = (float)lParams->m_tidalVolume / ProtocolGetScale(lParams->m_tidalVolume_scale);
            lVSimv->tidalVolumeMl = (float)lParams->m_tidalVolume / ProtocolGetScale(lParams->m_tidalVolume_scale);
            lPrvcSimv->targetTidalVolumeMl = (float)lParams->m_tidalVolume / ProtocolGetScale(lParams->m_tidalVolume_scale);
            lVs->targetTidalVolumeMl = (float)lParams->m_tidalVolume / ProtocolGetScale(lParams->m_tidalVolume_scale);
        }
        if (lParams->m_valid[0x11]) {
            lPac->flowTriggerLpm = (float)lParams->m_trigFlow / ProtocolGetScale(lParams->m_trigFlow_scale);
            lVac->flowTriggerLpm = (float)lParams->m_trigFlow / ProtocolGetScale(lParams->m_trigFlow_scale);
            lPrvc->flowTriggerLpm = (float)lParams->m_trigFlow / ProtocolGetScale(lParams->m_trigFlow_scale);
            lPsv->flowTriggerLpm = (float)lParams->m_trigFlow / ProtocolGetScale(lParams->m_trigFlow_scale);
            lSt->flowTriggerLpm = (float)lParams->m_trigFlow / ProtocolGetScale(lParams->m_trigFlow_scale);
            lPSimv->flowTriggerLpm = (float)lParams->m_trigFlow / ProtocolGetScale(lParams->m_trigFlow_scale);
            lVSimv->flowTriggerLpm = (float)lParams->m_trigFlow / ProtocolGetScale(lParams->m_trigFlow_scale);
            lPrvcSimv->flowTriggerLpm = (float)lParams->m_trigFlow / ProtocolGetScale(lParams->m_trigFlow_scale);
            lBapap->flowTriggerLpm = (float)lParams->m_trigFlow / ProtocolGetScale(lParams->m_trigFlow_scale);
            lVs->flowTriggerLpm = (float)lParams->m_trigFlow / ProtocolGetScale(lParams->m_trigFlow_scale);
        }
        if (lParams->m_valid[0x12]) {
            lPac->pressureTriggerCmh2o = (float)lParams->m_trigPress / ProtocolGetScale(lParams->m_trigPress_scale);
            lVac->pressureTriggerCmh2o = (float)lParams->m_trigPress / ProtocolGetScale(lParams->m_trigPress_scale);
            lPrvc->pressureTriggerCmh2o = (float)lParams->m_trigPress / ProtocolGetScale(lParams->m_trigPress_scale);
            lPsv->pressureTriggerCmh2o = (float)lParams->m_trigPress / ProtocolGetScale(lParams->m_trigPress_scale);
            lSt->pressureTriggerCmh2o = (float)lParams->m_trigPress / ProtocolGetScale(lParams->m_trigPress_scale);
            lPSimv->pressureTriggerCmh2o = (float)lParams->m_trigPress / ProtocolGetScale(lParams->m_trigPress_scale);
            lVSimv->pressureTriggerCmh2o = (float)lParams->m_trigPress / ProtocolGetScale(lParams->m_trigPress_scale);
            lPrvcSimv->pressureTriggerCmh2o = (float)lParams->m_trigPress / ProtocolGetScale(lParams->m_trigPress_scale);
            lBapap->pressureTriggerCmh2o = (float)lParams->m_trigPress / ProtocolGetScale(lParams->m_trigPress_scale);
            lVs->pressureTriggerCmh2o = (float)lParams->m_trigPress / ProtocolGetScale(lParams->m_trigPress_scale);
        }
        if (lParams->m_valid[0x13]) {
            lPsv->cycleOffPercent = (float)lParams->m_exhTrigPercent / ProtocolGetScale(lParams->m_exhTrigPercent_scale);
            lSt->cycleOffPercent = (float)lParams->m_exhTrigPercent / ProtocolGetScale(lParams->m_exhTrigPercent_scale);
            lPSimv->cycleOffPercent = (float)lParams->m_exhTrigPercent / ProtocolGetScale(lParams->m_exhTrigPercent_scale);
            lVSimv->cycleOffPercent = (float)lParams->m_exhTrigPercent / ProtocolGetScale(lParams->m_exhTrigPercent_scale);
            lPrvcSimv->cycleOffPercent = (float)lParams->m_exhTrigPercent / ProtocolGetScale(lParams->m_exhTrigPercent_scale);
            lBapap->cycleOffPercent = (float)lParams->m_exhTrigPercent / ProtocolGetScale(lParams->m_exhTrigPercent_scale);
            lVs->cycleOffPercent = (float)lParams->m_exhTrigPercent / ProtocolGetScale(lParams->m_exhTrigPercent_scale);
        }
        if (lParams->m_valid[0x14]) {
            lPac->Rate = (float)lParams->m_rate / ProtocolGetScale(lParams->m_rate_scale);
            lVac->freq = (float)lParams->m_rate / ProtocolGetScale(lParams->m_rate_scale);
            lPrvc->respiratoryRateBpm = (float)lParams->m_rate / ProtocolGetScale(lParams->m_rate_scale);
            lSt->inspRateBpm = (float)lParams->m_rate / ProtocolGetScale(lParams->m_rate_scale);
        }
        if (lParams->m_valid[0x15]) {
            lPSimv->SIMVRateBpm = (float)lParams->m_simvRate / ProtocolGetScale(lParams->m_simvRate_scale);
            lVSimv->SIMVRateBpm = lPSimv->SIMVRateBpm;
            lPrvcSimv->SIMVRateBpm = lPSimv->SIMVRateBpm;
        }
        if (lParams->m_valid[0x0F]) {
            lPSimv->apneaVolumeTidalMl = (float)lParams->m_apneaTidalVolume / ProtocolGetScale(lParams->m_apneaTidalVolume_scale);
            lVSimv->apneaVolumeTidalMl = lPSimv->apneaVolumeTidalMl;
            lPrvcSimv->apneaVolumeTidalMl = lPSimv->apneaVolumeTidalMl;
            lBapap->apneaVolumeTidalMl = lPSimv->apneaVolumeTidalMl;
            lVs->apneaVolumeTidalMl = lPSimv->apneaVolumeTidalMl;
        }
        if (lParams->m_valid[0x16]) {
            lPsv->apneaRateBpm = (float)lParams->m_apneaRate / ProtocolGetScale(lParams->m_apneaRate_scale);
            lPSimv->apneaRateBpm = (float)lParams->m_apneaRate / ProtocolGetScale(lParams->m_apneaRate_scale);
            lVSimv->apneaRateBpm = (float)lParams->m_apneaRate / ProtocolGetScale(lParams->m_apneaRate_scale);
            lPrvcSimv->apneaRateBpm = (float)lParams->m_apneaRate / ProtocolGetScale(lParams->m_apneaRate_scale);
            lBapap->apneaRateBpm = (float)lParams->m_apneaRate / ProtocolGetScale(lParams->m_apneaRate_scale);
            lVs->apneaRateBpm = (float)lParams->m_apneaRate / ProtocolGetScale(lParams->m_apneaRate_scale);
        }
        if (lParams->m_valid[0x17]) {
            lPac->inspiratoryTimeMs = (float)lParams->m_ti * 1000.0f / ProtocolGetScale(lParams->m_ti_scale);
            lVac->inspTimeMs = (float)lParams->m_ti * 1000.0f / ProtocolGetScale(lParams->m_ti_scale);
            lPrvc->inspiratoryTimeMs = (float)lParams->m_ti * 1000.0f / ProtocolGetScale(lParams->m_ti_scale);
            lSt->inspTimeMs = (float)lParams->m_ti * 1000.0f / ProtocolGetScale(lParams->m_ti_scale);
            lPSimv->inspiratoryTimeMs = (float)lParams->m_ti * 1000.0f / ProtocolGetScale(lParams->m_ti_scale);
            lVSimv->inspiratoryTimeMs = (float)lParams->m_ti * 1000.0f / ProtocolGetScale(lParams->m_ti_scale);
            lPrvcSimv->inspiratoryTimeMs = (float)lParams->m_ti * 1000.0f / ProtocolGetScale(lParams->m_ti_scale);
        }
        if (lParams->m_valid[0x18]) {
            lPsv->maxInspiratoryTimeMs = (float)lParams->m_tiMax * 1000.0f / ProtocolGetScale(lParams->m_tiMax_scale);
            lBapap->maxInspiratoryTimeMs = (float)lParams->m_tiMax * 1000.0f / ProtocolGetScale(lParams->m_tiMax_scale);
            lSt->maxInspiratoryTimeMs = (float)lParams->m_tiMax * 1000.0f / ProtocolGetScale(lParams->m_tiMax_scale);
        }
        if (lParams->m_valid[0x19]) {
            lPsv->apneaInspTimeMs = (float)lParams->m_apneaTi * 1000.0f / ProtocolGetScale(lParams->m_apneaTi_scale);
            lPSimv->apneaInspTimeMs = (float)lParams->m_apneaTi * 1000.0f / ProtocolGetScale(lParams->m_apneaTi_scale);
            lVSimv->apneaInspTimeMs = (float)lParams->m_apneaTi * 1000.0f / ProtocolGetScale(lParams->m_apneaTi_scale);
            lPrvcSimv->apneaInspTimeMs = (float)lParams->m_apneaTi * 1000.0f / ProtocolGetScale(lParams->m_apneaTi_scale);
            lBapap->apneaInspTimeMs = (float)lParams->m_apneaTi * 1000.0f / ProtocolGetScale(lParams->m_apneaTi_scale);
            lVs->apneaInspTimeMs = (float)lParams->m_apneaTi * 1000.0f / ProtocolGetScale(lParams->m_apneaTi_scale);
        }
        if (lParams->m_valid[0x1A]) {
            lPac->riseTimeMs = (float)lParams->m_riseTime * 1000.0f / ProtocolGetScale(lParams->m_riseTime_scale);
            lPsv->riseTimeMs = (float)lParams->m_riseTime * 1000.0f / ProtocolGetScale(lParams->m_riseTime_scale);
            lBapap->riseTimeMs = (float)lParams->m_riseTime * 1000.0f / ProtocolGetScale(lParams->m_riseTime_scale);
            lVs->riseTimeMs = (float)lParams->m_riseTime * 1000.0f / ProtocolGetScale(lParams->m_riseTime_scale);
            lSt->riseTimeMs = (float)lParams->m_riseTime * 1000.0f / ProtocolGetScale(lParams->m_riseTime_scale);
            lPSimv->pressureRiseTimeMs = (float)lParams->m_riseTime * 1000.0f / ProtocolGetScale(lParams->m_riseTime_scale);
            lPSimv->supportRiseTimeMs = (float)lParams->m_riseTime * 1000.0f / ProtocolGetScale(lParams->m_riseTime_scale);
            lVSimv->pressureRiseTimeMs = (float)lParams->m_riseTime * 1000.0f / ProtocolGetScale(lParams->m_riseTime_scale);
            lVSimv->supportRiseTimeMs = (float)lParams->m_riseTime * 1000.0f / ProtocolGetScale(lParams->m_riseTime_scale);
        }
        if (lParams->m_valid[0x26]) {
            lVac->inspPausePct = (float)lParams->m_inspPausePercent / ProtocolGetScale(lParams->m_inspPausePercent_scale);
            lVSimv->inspPausePct = (float)lParams->m_inspPausePercent / ProtocolGetScale(lParams->m_inspPausePercent_scale);
        }
        if (lParams->m_valid[0x24] || lParams->m_valid[0x25]) {
            eVentTriggerType lTrigger = lParams->m_assistTrig == 0U ? VENT_TRIGGER_OFF :
                (lParams->m_FlowTrigger == 1U ? VENT_TRIGGER_FLOW : VENT_TRIGGER_PRESSURE);
            lPac->triggerType = lTrigger;
            lVac->triggerType = lTrigger;
            lPrvc->triggerType = lTrigger;
            lPsv->triggerType = lTrigger;
            lSt->triggerType = lTrigger;
            lPSimv->triggerType = lTrigger;
            lVSimv->triggerType = lTrigger;
            lPrvcSimv->triggerType = lTrigger;
            lBapap->triggerType = lTrigger;
            lVs->triggerType = lTrigger;
        }
        if (lParams->m_valid[2] && lParams->m_patientType < VENT_PATIENT_TYPE_COUNT) {
            GetVentPatientSettings()->Type = (eVentPatientType)lParams->m_patientType;
        }
        if (lParams->m_valid[4]) {
            GetVentPatientSettings()->IdealBodyHeightCm = lParams->m_idealHeight / ProtocolGetScale(lParams->m_idealHeight_scale);
        }
        if (lParams->m_valid[5]) {
            GetVentPatientSettings()->IdealBodyWeightKg = lParams->m_idealWeight / ProtocolGetScale(lParams->m_idealWeight_scale);
        }
    }

    /* Alarm limits are supplied by MCM even with local ventilation settings. */
    if (lChanged) {
        const RxAlarmLimitsCache_t *lLimits = ProtocolGetRxAlarmLimitsCache();
        stVentLimitSettings *lAlarm = GetVentLimitSettings();

        if (lLimits->m_valid[0]) {
            lAlarm->pressureLow = (float)lLimits->m_pAirwayLow / ProtocolGetScale(lLimits->m_pAirwayLow_scale);
        }
        if (lLimits->m_valid[1]) {
            lAlarm->pressureHigh = (float)lLimits->m_pAirwayHigh / ProtocolGetScale(lLimits->m_pAirwayHigh_scale);
        }
        if (lLimits->m_valid[2]) {
            lAlarm->minuteVolumeHigh = (float)lLimits->m_mvHigh / ProtocolGetScale(lLimits->m_mvHigh_scale);
        }
        if (lLimits->m_valid[3]) {
            lAlarm->minuteVolumeLow = (float)lLimits->m_mvLow / ProtocolGetScale(lLimits->m_mvLow_scale);
        }
        if (lLimits->m_valid[4]) {
            lAlarm->tidalVolumeHigh = (float)lLimits->m_tveHigh / ProtocolGetScale(lLimits->m_tveHigh_scale);
        }
        if (lLimits->m_valid[5]) {
            lAlarm->tidalVolumeLow = (float)lLimits->m_tveLow / ProtocolGetScale(lLimits->m_tveLow_scale);
        }
        if (lLimits->m_valid[6]) {
            lAlarm->o2PercentHigh = (float)lLimits->m_fio2High / ProtocolGetScale(lLimits->m_fio2High_scale);
        }
        if (lLimits->m_valid[7]) {
            lAlarm->o2PercentLow = (float)lLimits->m_fio2Low / ProtocolGetScale(lLimits->m_fio2Low_scale);
        }
        if (lLimits->m_valid[8]) {
            lAlarm->frequencyHigh = (float)lLimits->m_frTotalHigh / ProtocolGetScale(lLimits->m_frTotalHigh_scale);
        }
        if (lLimits->m_valid[9]) {
            lAlarm->frequencyLow = (float)lLimits->m_frTotalLow / ProtocolGetScale(lLimits->m_frTotalLow_scale);
        }
        if (lLimits->m_valid[10]) {
            lAlarm->apneaTimeAlarm = (float)lLimits->m_apneaTime / ProtocolGetScale(lLimits->m_apneaTime_scale);
            GetVentVsSettings()->apneaAlarmTimeMs = (uint32_t)lAlarm->apneaTimeAlarm * 1000U;
        }
    }

    bool lCommand = gProtocolCommandPending;
    uint8_t lRun = ProtocolGetRxVentSwitchCache()->m_command;
    const RxVentParamsCache_t *lParams = ProtocolGetRxVentParamsCache();
    eVentMode lMode = lSource ? (lParams->m_valid[1] ? lParams->m_modeRecv : VENT_MD_IDLE) : breathSchedulerModeGet();
    if (!lSource && lMode == VENT_MD_IDLE) {
        lMode = VENT_MD_PAC;
    }
    gProtocolSettingsDirty = false;
    gProtocolAlarmLimitsDirty = false;
    gProtocolCommandPending = false;
    gLastSource = lSource;
    repRtosExitCritical();

    if (lCommand && lRun == 0U) {
        lStatus = breathSchedulerStop();
    } else if (lCommand && lRun == 1U) {
        lStatus = breathSchedulerStart(lMode);
    } else if (lChanged && breathSchedulerRunningGet()) {
        lStatus = breathSchedulerSettingsUpdate(lMode);
    }
    if (lStatus != BREATH_CONTROL_SUCCESS) {
        LOG_W("protocol", "vent command/settings rejected: %d", (int)lStatus);
    }
}

/*************************************** End of file ********************************/
