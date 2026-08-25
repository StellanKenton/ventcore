/************************************************************************************
* @file     : monitordata.c
* @brief    : Test waveform monitor data implementation.
***********************************************************************************/
#include "monitordata.h"

#include "controldata.h"
#include "phasecontroller.h"
#include "monitorengine.h"

volatile stMonitorWaveformData gMonitorWaveformData;

void monitorDataUpdate(void) {
    gMonitorWaveformData.airFlow = controlDataGet(INSP_FLOW_FILTERED)/2.0F;
    gMonitorWaveformData.oxygenFlow = controlDataGet(O2_FLOW_FILTERED)/2.0F;
    gMonitorWaveformData.proximalFlow = controlDataGet(MDIFF_REAL_FLOW)/2.0F;
    gMonitorWaveformData.inspPressure = controlDataGet(INSP_REAL_PRS);
    gMonitorWaveformData.peepPressure = controlDataGet(PEEP_REAL_PRS);
    gMonitorWaveformData.expPressure = controlDataGet(EXP_REAL_PRS);
    gMonitorWaveformData.patientPressure = controlDataGet(PAT_REAL_PRS);
    gMonitorWaveformData.blowerSpeed = controlDataGet(RAW_BLOWER_SPEED)/10.0F;
    gMonitorWaveformData.patientRefPressure = phaseControlGet(PHASE_REF_PRESSURE);
    gMonitorWaveformData.patientFastRefPressure = phaseControlGet(PHASE_REF_FAST_PRESSURE);
    gMonitorWaveformData.tidalVolume = monitorEngineGet(MONITOR_TIDA_VOL)/10.0f;
    gMonitorWaveformData.tidalVolumeInsp = monitorEngineGet(MONITOR_TIDA_VOL_INSP)/10.0f;
    gMonitorWaveformData.tidalVolumeExp = monitorEngineGet(MONITOR_TIDA_VOL_EXP)/10.0f;
}

/**************************End of file********************************/
