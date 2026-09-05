/************************************************************************************
* @file     : monitordata.c
* @brief    : Test waveform monitor data implementation.
***********************************************************************************/
#include "monitordata.h"

#include "actuatorcontroller.h"
#include "controldata.h"
#include "expirationcontroller.h"
#include "phasecontroller.h"
#include "monitorengine.h"
#include "pressurecontroller.h"

volatile stMonitorWaveformData gMonitorWaveformData;

void monitorDataUpdate(void) {
    stActuatorRequest lActuatorRequest;
    stPressureControllerDiagnostic lPressureDiagnostic;

    pressureControllerDiagnosticGet(&lPressureDiagnostic);

    gMonitorWaveformData.airFlowX2 = controlDataGet(INSP_FLOW_FILTERED) / 2.0F;
    gMonitorWaveformData.oxygenFlowX2 = controlDataGet(O2_FLOW_FILTERED) / 2.0F;
    gMonitorWaveformData.proximalFlowX2 = controlDataGet(MDIFF_REAL_FLOW) / 2.0F;
    gMonitorWaveformData.inspPressureX1 = controlDataGet(INSP_REAL_PRS);
    gMonitorWaveformData.peepPressureX1 = controlDataGet(PEEP_REAL_PRS);
    gMonitorWaveformData.expPressureX1 = controlDataGet(EXP_REAL_PRS);
    gMonitorWaveformData.patientPressureX1 = controlDataGet(PAT_REAL_PRS);
    gMonitorWaveformData.predictedPressureX1 = controlDataGet(PREDICT_PAT_PRS);
    gMonitorWaveformData.blowerSpeedX10 = controlDataGet(RAW_BLOWER_SPEED) / 10.0F;
    gMonitorWaveformData.patientRefPressureX1 = phaseControlGet(PHASE_REF_PRESSURE);
    gMonitorWaveformData.flowCompensationX1 = lPressureDiagnostic.flowCompensation;
    gMonitorWaveformData.patientCorrectionX1 = lPressureDiagnostic.patientCorrection;
    gMonitorWaveformData.innerEffortX1 = lPressureDiagnostic.innerEffort;
    gMonitorWaveformData.blowerFeedforwardX1 = lPressureDiagnostic.blowerFeedforward;
    gMonitorWaveformData.tidalVolumeX10 = monitorEngineGet(MONITOR_TIDA_VOL) / 10.0F;
    gMonitorWaveformData.tidalVolumeInspX10 = monitorEngineGet(MONITOR_TIDA_VOL_INSP) / 10.0F;
    gMonitorWaveformData.tidalVolumeExpX10 = monitorEngineGet(MONITOR_TIDA_VOL_EXP) / 10.0F;
    gMonitorWaveformData.plateauPressureX1 = monitorEngineGet(MONITOR_PLATEAU_PRS);
    gMonitorWaveformData.expirationControllerState = expirationControllerStateGet();
    gMonitorWaveformData.pressureControllerState = pressureControllerStateGet();
    if (actuatorControllerLastRequestGet(&lActuatorRequest) == ACTUATOR_REQUEST_SUCCESS) {
        gMonitorWaveformData.blowerTargetX100 = lActuatorRequest.blowerTarget / 100.0f;
        gMonitorWaveformData.valveDutyX2 = lActuatorRequest.expiratoryValveDuty /2.0f;
    }
}

/**************************End of file********************************/
