/************************************************************************************
* @file     : apneaengine.c
* @brief    : Spontaneous ventilation apnea and backup scheduler.
* @details  : Monitors patient-triggered breaths and requests PSV-ST backup breaths.
***********************************************************************************/
#include "apneaengine.h"

#include <string.h>

#include "breathscheduler.h"

static stApneaEngine gApneaEngine;

/** Reset apnea monitoring outside a supported spontaneous mode. */
static void apneaEngineIdleEnter(void)
{
    (void)memset(&gApneaEngine, 0, sizeof(gApneaEngine));
    gApneaEngine.state = APNEA_ENGINE_IDLE;
    gApneaEngine.mode = VENT_MD_IDLE;
    gApneaEngine.previousPhase = PHASE_IDLE;
}

void apneaEngineInit(void)
{
    apneaEngineIdleEnter();
}

eApneaEngineState apneaEngineStateGet(void)
{
    return gApneaEngine.state;
}

void apneaEngineProcess(uint32_t nowMs)
{
    stBreathPlan lPlan;
    ePhaseControllerState lPhase = phaseControllerStateGet();
    eVentMode lMode = breathSchedulerModeGet();
    uint32_t lDeadlineMs;

    if ((breathSchedulerRunningGet() == 0U) ||
        ((lMode != VENT_MD_CPAP_PSV) && (lMode != VENT_MD_PSV_ST)) ||
        (phaseControllerActivePlanGet(&lPlan) != PHASE_CONTROL_SUCCESS)) {
        apneaEngineIdleEnter();
        return;
    }

    if ((gApneaEngine.initialized == 0U) || (gApneaEngine.mode != lMode)) {
        gApneaEngine.state = APNEA_ENGINE_MONITORING;
        gApneaEngine.mode = lMode;
        gApneaEngine.previousPhase = lPhase;
        gApneaEngine.referenceMs = nowMs;
        gApneaEngine.initialized = 1U;
        return;
    }

    if ((lPhase == PHASE_INSP_RISE) &&
        (gApneaEngine.previousPhase != PHASE_INSP_RISE)) {
        if ((lPlan.triggerReason == BREATH_TRIGGER_REASON_PRESSURE) ||
            (lPlan.triggerReason == BREATH_TRIGGER_REASON_FLOW)) {
            gApneaEngine.state = APNEA_ENGINE_MONITORING;
        } else if (lPlan.triggerReason == BREATH_TRIGGER_REASON_APNEA_BACKUP) {
            gApneaEngine.state = APNEA_ENGINE_BACKUP;
        }
        gApneaEngine.referenceMs = nowMs;
    }
    gApneaEngine.previousPhase = lPhase;

    if (gApneaEngine.state == APNEA_ENGINE_MONITORING) {
        lDeadlineMs = lPlan.apneaTimeMs;
    } else if (gApneaEngine.state == APNEA_ENGINE_BACKUP) {
        lDeadlineMs = lPlan.backupBreathIntervalMs;
    } else {
        return;
    }
    if ((lDeadlineMs == 0U) ||
        ((nowMs - gApneaEngine.referenceMs) < lDeadlineMs)) {
        return;
    }

    if (lMode == VENT_MD_CPAP_PSV) {
        gApneaEngine.state = APNEA_ENGINE_ALARM;
        return;
    }
    if ((lPhase == PHASE_EXP_PEEP) &&
        (phaseControllerTrigger(BREATH_TRIGGER_REASON_APNEA_BACKUP, nowMs) ==
         PHASE_CONTROL_SUCCESS)) {
        gApneaEngine.state = APNEA_ENGINE_BACKUP;
        gApneaEngine.referenceMs = nowMs;
        gApneaEngine.previousPhase = phaseControllerStateGet();
    }
}

/**************************End of file********************************/
