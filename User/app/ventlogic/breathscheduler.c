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
#include "settingdata.h"
#include <stdbool.h>
#include "numfilter.h"

stBreathInfo gBreathInfo;
static float gBreathData[BREATH_COUNT];

int8_t breathSchedulerInit(void)
{
    gBreathInfo.runState = false;
    gBreathInfo.currentMode = VENT_MD_IDLE;
    return 0;
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
    if(gBreathInfo.runState != gBreathInfo.runPrevious)
    {
        stVentPatientSettings *lPatientSettings = GetVentPatientSettings();
        gBreathInfo.runPrevious = gBreathInfo.runState;
        switch(gBreathInfo.currentMode)
        {
            case VENT_MD_IDLE:
                break;
            case VENT_MD_PAC: {
                stVentPacSettings *lPacSettings = GetVentPacSettings();
                float lBreathPeriodMs = 0.0f;

                if (lPacSettings->Rate > 0.0f) {
                    lBreathPeriodMs = 60000.0f / lPacSettings->Rate;
                }

                breathControlSet(BREATH_PATIENT_TYPE, (float)lPatientSettings->Type);
                breathControlSet(BREATH_IDEAL_BODY_WEIGHT, (float)lPatientSettings->IdealBodyWeightKg);
                breathControlSet(BREATH_IDEAL_BODY_HEIGHT, (float)lPatientSettings->IdealBodyHeightCm);
                breathControlSet(BREATH_PEEP_PRESSURE, lPacSettings->peep);
                breathControlSet(BREATH_INSP_PRESSURE, lPacSettings->DeltaPressure+lPacSettings->peep);
                breathControlSet(BREATH_CONTROL_TYPE, (float)PRESSURE_CONTROL);
                breathControlSet(BREATH_INSP_RISE_TIME, NUMFILTER_MIN((float)lPacSettings->inspiratoryTimeMs, (float)lPacSettings->riseTimeMs));
                breathControlSet(BREATH_INSP_HOLD_TIME, NUMFILTER_MAX(0.0f, (float)(lPacSettings->inspiratoryTimeMs - lPacSettings->riseTimeMs)));
                breathControlSet(BREATH_INSP_PEEP_TIME, NUMFILTER_MAX(BREATH_PEEP_LOCK_TIME_MS, lBreathPeriodMs - (float)lPacSettings->inspiratoryTimeMs));
                breathControlSet(BREATH_FIO2, lPacSettings->oxygen);
                breathControlSet(BREATH_RUN, 1.0f);
                break;
            }
            case VENT_MD_VAC:
                break;
            case VENT_MD_CPAP_PSV:
                break;
            case VENT_MD_PSV_ST:
                break;
            case VENT_MD_P_SIMV:
                break;
            case VENT_MD_V_SIMV:
                break;
            case VENT_MD_PRVC:
                break;
            case VENT_MD_PRVC_SIMV:
                break;
            case VENT_MD_VS:
                break;
            case VENT_MD_BAPAP:
                break;
            case VENT_MD_APRV:
                break;
            case VENT_MD_NCPAP:
                break;
            case VENT_MD_NCPAP_PC:
                break;
            case VENT_MD_NIPPV:
                break;
            case VENT_MD_SNIPPV:
                break;
            case VENT_MD_AMV:
                break;
            case VENT_MD_IAMV:
                break;
            case VENT_MD_PPS:
                break;
            case VENT_MD_CPRV:
                break;
            case VENT_MD_HFO:
                break;
            case VENT_MD_COUNT:
                break;
        }
    }
}
/**************************End of file********************************/
