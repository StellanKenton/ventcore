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

        if (lParams->m_valid[0x06]) {
            lPac->oxygen = (float)lParams->m_fio2 / ProtocolGetScale(lParams->m_fio2_scale);
            lVac->oxygen = (float)lParams->m_fio2 / ProtocolGetScale(lParams->m_fio2_scale);
            lPsv->oxygenPercent = (float)lParams->m_fio2 / ProtocolGetScale(lParams->m_fio2_scale);
            lSt->oxygenPercent = (float)lParams->m_fio2 / ProtocolGetScale(lParams->m_fio2_scale);
        }
        if (lParams->m_valid[0x07]) {
            lPac->DeltaPressure = (float)lParams->m_deltaPinsp / ProtocolGetScale(lParams->m_deltaPinsp_scale);
        }
        if (lParams->m_valid[0x08]) {
            lPac->peep = (float)lParams->m_peep / ProtocolGetScale(lParams->m_peep_scale);
            lVac->peep = (float)lParams->m_peep / ProtocolGetScale(lParams->m_peep_scale);
            lPsv->peepCmh2o = (float)lParams->m_peep / ProtocolGetScale(lParams->m_peep_scale);
            lSt->peepCmh2o = (float)lParams->m_peep / ProtocolGetScale(lParams->m_peep_scale);
        }
        if (lParams->m_valid[0x09]) {
            lPsv->pressureSupportCmh2o = (float)lParams->m_deltaPsupp / ProtocolGetScale(lParams->m_deltaPsupp_scale);
            lSt->pressureSupportCmh2o = (float)lParams->m_deltaPsupp / ProtocolGetScale(lParams->m_deltaPsupp_scale);
        }
        if (lParams->m_valid[0x0C]) {
            lSt->backupInspiratoryPressureCmh2o = (float)lParams->m_deltaApneaP / ProtocolGetScale(lParams->m_deltaApneaP_scale);
        }
        if (lParams->m_valid[0x0E]) {
            lVac->tidalVolume = (float)lParams->m_tidalVolume / ProtocolGetScale(lParams->m_tidalVolume_scale);
        }
        if (lParams->m_valid[0x11]) {
            lPac->flowTriggerLpm = (float)lParams->m_trigFlow / ProtocolGetScale(lParams->m_trigFlow_scale);
            lVac->flowTriggerLpm = (float)lParams->m_trigFlow / ProtocolGetScale(lParams->m_trigFlow_scale);
            lPsv->flowTriggerLpm = (float)lParams->m_trigFlow / ProtocolGetScale(lParams->m_trigFlow_scale);
            lSt->flowTriggerLpm = (float)lParams->m_trigFlow / ProtocolGetScale(lParams->m_trigFlow_scale);
        }
        if (lParams->m_valid[0x12]) {
            lPac->pressureTriggerCmh2o = (float)lParams->m_trigPress / ProtocolGetScale(lParams->m_trigPress_scale);
            lVac->pressureTriggerCmh2o = (float)lParams->m_trigPress / ProtocolGetScale(lParams->m_trigPress_scale);
            lPsv->pressureTriggerCmh2o = (float)lParams->m_trigPress / ProtocolGetScale(lParams->m_trigPress_scale);
            lSt->pressureTriggerCmh2o = (float)lParams->m_trigPress / ProtocolGetScale(lParams->m_trigPress_scale);
        }
        if (lParams->m_valid[0x13]) {
            lPsv->cycleOffPercent = (float)lParams->m_exhTrigPercent / ProtocolGetScale(lParams->m_exhTrigPercent_scale);
            lSt->cycleOffPercent = (float)lParams->m_exhTrigPercent / ProtocolGetScale(lParams->m_exhTrigPercent_scale);
        }
        if (lParams->m_valid[0x14]) {
            lPac->Rate = (float)lParams->m_rate / ProtocolGetScale(lParams->m_rate_scale);
            lVac->freq = (float)lParams->m_rate / ProtocolGetScale(lParams->m_rate_scale);
        }
        if (lParams->m_valid[0x16]) {
            lSt->backupRespiratoryRateBpm = (float)lParams->m_apneaRate / ProtocolGetScale(lParams->m_apneaRate_scale);
        }
        if (lParams->m_valid[0x17]) {
            lPac->inspiratoryTimeMs = (float)lParams->m_ti * 1000.0f / ProtocolGetScale(lParams->m_ti_scale);
            lVac->inspTimeMs = (float)lParams->m_ti * 1000.0f / ProtocolGetScale(lParams->m_ti_scale);
        }
        if (lParams->m_valid[0x18]) {
            lPsv->maxInspiratoryTimeMs = (float)lParams->m_tiMax * 1000.0f / ProtocolGetScale(lParams->m_tiMax_scale);
            lSt->maxInspiratoryTimeMs = (float)lParams->m_tiMax * 1000.0f / ProtocolGetScale(lParams->m_tiMax_scale);
        }
        if (lParams->m_valid[0x19]) {
            lSt->backupInspiratoryTimeMs = (float)lParams->m_apneaTi * 1000.0f / ProtocolGetScale(lParams->m_apneaTi_scale);
        }
        if (lParams->m_valid[0x1A]) {
            lPac->riseTimeMs = (float)lParams->m_riseTime * 1000.0f / ProtocolGetScale(lParams->m_riseTime_scale);
            lPsv->riseTimeMs = (float)lParams->m_riseTime * 1000.0f / ProtocolGetScale(lParams->m_riseTime_scale);
            lSt->riseTimeMs = (float)lParams->m_riseTime * 1000.0f / ProtocolGetScale(lParams->m_riseTime_scale);
            lSt->backupRiseTimeMs = (float)lParams->m_riseTime * 1000.0f / ProtocolGetScale(lParams->m_riseTime_scale);
        }
        if (lParams->m_valid[0x26]) {
            lVac->inspPausePct = (float)lParams->m_inspPausePercent / ProtocolGetScale(lParams->m_inspPausePercent_scale);
        }
        if (lParams->m_valid[0x29]) {
            lPsv->pressureLimitCmh2o = (float)lParams->m_peakPressure / ProtocolGetScale(lParams->m_peakPressure_scale);
            lSt->pressureLimitCmh2o = (float)lParams->m_peakPressure / ProtocolGetScale(lParams->m_peakPressure_scale);
        }
        if (lParams->m_valid[0x24] || lParams->m_valid[0x25]) {
            eVentTriggerType lTrigger = lParams->m_assistTrig == 0U ? VENT_TRIGGER_OFF :
                (lParams->m_FlowTrigger == 1U ? VENT_TRIGGER_FLOW : VENT_TRIGGER_PRESSURE);
            lPac->triggerType = lTrigger;
            lVac->triggerType = lTrigger;
            lPsv->triggerType = lTrigger;
            lSt->triggerType = lTrigger;
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
        stVentCpapPsvSettings *lPsv = GetVentCpapPsvSettings();
        stVentPsvStSettings *lSt = GetVentPsvStSettings();

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
            lAlarm->apneaTimeHigh = (float)lLimits->m_apneaTime / ProtocolGetScale(lLimits->m_apneaTime_scale);
            lPsv->apneaAlarmTimeMs = (uint32_t)lAlarm->apneaTimeHigh * 1000U;
            lSt->apneaTimeMs = lPsv->apneaAlarmTimeMs;
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
