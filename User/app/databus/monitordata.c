/************************************************************************************
* @file     : monitordata.c
* @brief    : Test waveform monitor data implementation.
***********************************************************************************/
#include "monitordata.h"

#include "controldata.h"
#include "phasecontroller.h"

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
}

/**************************End of file********************************/
