/************************************************************************************
* @file     : databus.h
* @brief    : Ventilation data acquisition and filtering interface.
***********************************************************************************/
#ifndef USER_APP_DATABUS_DATABUS_H
#define USER_APP_DATABUS_DATABUS_H

#ifdef __cplusplus
extern "C" {
#endif

#define VENT_DATA_CHANNEL_COUNT            6U

/* Store the latest sensor values and retain the preceding raw sample. */
void controlDataRawProcess(void);

/* Update Butterworth and cascaded low-pass results. */
void controlDataFilterProcess(void);

/* Convert Butterworth-filtered ADC values to calibrated physical values. */
void controlDataCalibrationProcess(void);

/* Set the proximal-flow zero offset subtracted from calibrated samples. */
void controlDataMdiffFlowZeroOffsetSet(float offsetLpm);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_DATABUS_DATABUS_H */
