/************************************************************************************
* @file     : monitorengine.c
* @brief    : Breath monitor engine.
* @details  : Provides monitor engine initialization and periodic processing.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "monitorengine.h"
#include "controldata.h"
#include "phasecontroller.h"

stMonitorEngine gMonitorEngine;
static float gMonitorData[MONITOR_DATA_COUNT];

/** Store a monitor result selected by type. */
static int8_t monitorEngineSet(eMonitorDataType type, float value) {
    if ((type <= MONITOR_DATA_NONE) || (type >= MONITOR_DATA_COUNT)) {
        return MONITOR_ENGINE_ERROR_PARAM;
    }

    gMonitorData[type] = value;
    return MONITOR_ENGINE_SUCCESS;
}

/** Integrate signed patient flow during inspiration. */
static void monitorEngineTidaVolProcess(void) {
    float lFlow = controlDataGet(MDIFF_REAL_FLOW);

    if ((lFlow > -MONITOR_FLOW_DEADBAND_LPM) &&
        (lFlow < MONITOR_FLOW_DEADBAND_LPM)) {
        return;
    }

    gMonitorData[MONITOR_TIDA_VOL] += lFlow * MONITOR_FLOW_SAMPLE_VOLUME_ML;
}

/** Integrate negative patient flow during expiration. */
static void monitorEngineExpTidaVolProcess(void) {
    float lFlow = controlDataGet(MDIFF_REAL_FLOW);

    if (lFlow > -MONITOR_FLOW_DEADBAND_LPM) {
        return;
    }

    gMonitorData[MONITOR_TIDA_VOL] += lFlow * MONITOR_FLOW_SAMPLE_VOLUME_ML;
}

/** Convert the breath phase to the corresponding monitor state. */
static eMonitorEngineState monitorEngineStateFromPhase(ePhaseControllerState phaseState) {
    switch (phaseState) {
        case PHASE_INSP_RISE:
            return MONITOR_STATE_INSP_RISE;
        case PHASE_INSP_HOLD:
            return MONITOR_STATE_INSP_HOLD;
        case PHASE_EXP_RELEASE:
            return MONITOR_STATE_EXP_RELEASE;
        case PHASE_EXP_PEEP:
            return MONITOR_STATE_EXP_PEEP;
        case PHASE_IDLE:
        default:
            return MONITOR_STATE_IDLE;
    }
}

void monitorEngineInit(void) {
    gMonitorEngine.runState = MONITOR_STATE_IDLE;
    (void)monitorEngineSet(MONITOR_TIDA_VOL, 0.0F);
}

float monitorEngineGet(eMonitorDataType type) {
    if ((type <= MONITOR_DATA_NONE) || (type >= MONITOR_DATA_COUNT)) {
        return 0.0F;
    }

    return gMonitorData[type];
}

void monitorEngineProcess(void) {
    eMonitorEngineState lNextState = monitorEngineStateFromPhase(phaseControllerStateGet());

    if ((lNextState == MONITOR_STATE_INSP_RISE) &&
        (gMonitorEngine.runState != MONITOR_STATE_INSP_RISE)) {
        (void)monitorEngineSet(MONITOR_TIDA_VOL, 0.0F);
    }
    gMonitorEngine.runState = lNextState;

    switch (gMonitorEngine.runState) {
        case MONITOR_STATE_INSP_RISE:
        case MONITOR_STATE_INSP_HOLD:
            monitorEngineTidaVolProcess();
            break;

        case MONITOR_STATE_EXP_RELEASE:
        case MONITOR_STATE_EXP_PEEP:
            monitorEngineExpTidaVolProcess();
            break;

        case MONITOR_STATE_IDLE:
            break;

        default:
            gMonitorEngine.runState = MONITOR_STATE_IDLE;
            break;
    }

}

/**************************End of file********************************/
