/************************************************************************************
* @file     : databus.h
* @brief    : Ventilation data acquisition and filtering interface.
***********************************************************************************/
#ifndef USER_APP_DATABUS_DATABUS_H
#define USER_APP_DATABUS_DATABUS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DATABUS_STATUS_OK                    1
#define DATABUS_ERROR_ARGUMENT             (-1)
#define DATABUS_ERROR_CALIBRATION          (-2)

#define VENT_DATA_CHANNEL_COUNT            6U

/* Store the latest sensor values and retain the preceding raw sample. */
void controlDataRawProcess(void);

/* Update Butterworth and cascaded low-pass results. */
void controlDataFilterProcess(void);

/* Convert filtered pressure and proximal ADCs with zero/density/gas correction. */
void controlDataCalibrationProcess(void);

/* Add the difference from the cumulative offset as an ADC-domain zero correction.
 * Call only with a measured zero-flow residual plus the current getter value. */
int8_t controlDataMdiffFlowZeroOffsetSet(float offsetLpm);

/* Return the cumulative re-zero token in current gas-corrected L/min units. */
float controlDataMdiffFlowZeroOffsetGet(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_DATABUS_DATABUS_H */
