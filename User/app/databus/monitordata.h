/************************************************************************************
* @file     : monitordata.h
* @brief    : Test waveform monitor data interface.
* @details  : Exposes the latest ventilation signals for debugger waveform display.
***********************************************************************************/
#ifndef USER_APP_DATABUS_MONITORDATA_H
#define USER_APP_DATABUS_MONITORDATA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The Xn suffix is the multiplier required to recover the real value. */
typedef struct stMonitorWaveformData {
    float airFlowX2;
    float oxygenFlowX2;
    float proximalFlowX2;
    float inspPressureX1;
    float peepPressureX1;
    float expPressureX1;
    float patientPressureX1;
    float predictedPressureX1;
    float blowerSpeedX10;
    float patientRefPressureX1;
    float fastRefX1;
    float flowCompensationX1;
    float patientCorrectionX1;
    float innerEffortX1;
    float blowerFeedforwardX1;
    float tidalVolumeX10;
    float tidalVolumeInspX10;
    float tidalVolumeExpX10;

    /* Adaptive PEEP diagnostics. Feedforward values use the blower-target scale. */
    float peepBaseFeedforwardX100;
    float peepAdaptiveBiasX100;
    float peepAdaptiveFeedforwardX100;
    float peepFeedbackEffortX1;
    float expirationPressureSlopeX1;

    uint16_t blowerTargetX100;
    uint8_t valveDutyX2;
    uint8_t phaseStateX1;
} stMonitorWaveformData;

extern volatile stMonitorWaveformData gMonitorWaveformData;

void monitorDataUpdate(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_DATABUS_MONITORDATA_H */

/**************************End of file********************************/
