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
#define VENT_DATA_10HZ_WINDOW_SIZE         20U

/* Store the latest sensor values and retain the preceding raw sample. */
void controlDataRawProcess(void);

/* Update the 10 Hz low-pass results from the latest raw sensor values. */
void controlDataFilterProcess(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_DATABUS_DATABUS_H */
