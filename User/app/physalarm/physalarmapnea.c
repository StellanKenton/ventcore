/************************************************************************************
* @file     : physalarmapnea.c
* @brief    : Apnea physiological alarm detector.
* @details  : Publishes the ventilation task apnea state to the alarm manager.
* @author   :
* @date     : 2026-09-07
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/

#include "physalarmmanager.h"
#include "apneaengine.h"
#include "rtos.h"

/** Snapshot the shared engine state; stopped ventilation clears the alarm. */
bool physAlarmApneaDetect(uint32_t nowMs) {
    bool lActive;
    eApneaEngineState lState;
    (void)nowMs;
    repRtosEnterCritical();
    lState = apneaEngineStateGet();
    lActive = (breathSchedulerRunningGet() != 0U) &&
              ((phaseControllerStateGet() == PHASE_INSP) ||
               (phaseControllerStateGet() == PHASE_EXP)) &&
              ((lState == APNEA_ENGINE_ALARM) || (lState == APNEA_ENGINE_BACKUP));
    repRtosExitCritical();
    return lActive;
}

/*************************************** End of file ********************************/
