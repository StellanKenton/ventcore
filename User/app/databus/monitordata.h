/************************************************************************************
* @file     : monitordata.h
* @brief    : Test waveform monitor data interface.
* @details  : Exposes the latest ventilation signals for debugger waveform display.
***********************************************************************************/
#ifndef USER_APP_DATABUS_MONITORDATA_H
#define USER_APP_DATABUS_MONITORDATA_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stMonitorWaveformData {
    float airFlow;
    float oxygenFlow;
    float proximalFlow;
    float inspPressure;
    float peepPressure;
    float expPressure;
    float patientPressure;
    float blowerSpeed;
    float patientRefPressure;
    float patientFastRefPressure;
    float tidalVolume;
} stMonitorWaveformData;

/* Volatile storage allows debugger waveform tools to observe every update. */
extern volatile stMonitorWaveformData gMonitorWaveformData;

/* Refresh all waveform signals from the current data buses. */
void monitorDataUpdate(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_DATABUS_MONITORDATA_H */

/**************************End of file********************************/
