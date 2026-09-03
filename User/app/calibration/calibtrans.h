/************************************************************************************
* @file     : calibtrans.h
* @brief    : Calibration table conversion interface.
* @details  : Converts raw inputs to physical values by linear interpolation.
* @author   :
* @date     :
* @version  :
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_CALIBRATION_CALIBTRANS_H
#define USER_APP_CALIBRATION_CALIBTRANS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CALIBTRANS_STATUS_OK               1
#define CALIBTRANS_ERROR_ARGUMENT        (-1)
#define CALIBTRANS_ERROR_NOT_READY       (-2)
#define CALIBTRANS_ERROR_TABLE           (-3)

/**
 * @brief Convert an inspiratory pressure ADC value to pressure.
 * @param adcValue ADC value to convert.
 * @param pressureValue Converted pressure output.
 * @return CALIBTRANS_STATUS_OK on success, otherwise a negative error code.
 */
int8_t calibtransInspPrs(float adcValue, float *pressureValue);

/**
 * @brief Convert a PEEP pressure ADC value to pressure.
 * @param adcValue ADC value to convert.
 * @param pressureValue Converted pressure output.
 * @return CALIBTRANS_STATUS_OK on success, otherwise a negative error code.
 */
int8_t calibtransPeepPrs(float adcValue, float *pressureValue);

/**
 * @brief Convert an expiratory pressure ADC value to pressure.
 * @param adcValue ADC value to convert.
 * @param pressureValue Converted pressure output.
 * @return CALIBTRANS_STATUS_OK on success, otherwise a negative error code.
 */
int8_t calibtransExpPrs(float adcValue, float *pressureValue);

/**
 * @brief Convert pressure to blower speed.
 * @param pressureValue Pressure value to convert.
 * @param speedRps Converted blower speed output in revolutions per second.
 * @return CALIBTRANS_STATUS_OK on success, otherwise a negative error code.
 */
int8_t calibtransPrsSpeed(float pressureValue, float *speedRps);

/**
 * @brief Convert an adult proximal-flow ADC value to flow, extrapolating past table endpoints.
 * @param adcValue ADC value to convert.
 * @param flowValue Converted flow output.
 * @return CALIBTRANS_STATUS_OK on success, otherwise a negative error code.
 */
int8_t calibtransAdultProxFlow(float adcValue, float *flowValue);

/**
 * @brief Convert a neonatal proximal-flow ADC value to flow, extrapolating past table endpoints.
 * @param adcValue ADC value to convert.
 * @param flowValue Converted flow output.
 * @return CALIBTRANS_STATUS_OK on success, otherwise a negative error code.
 */
int8_t calibtransNeoProxFlow(float adcValue, float *flowValue);

/**
 * @brief Convert an oxygen-valve duty cycle to flow.
 * @param dutyCycle Duty cycle to convert.
 * @param flowValue Converted flow output.
 * @return CALIBTRANS_STATUS_OK on success, otherwise a negative error code.
 */
int8_t calibtransOxygenValve(float dutyCycle, float *flowValue);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_CALIBRATION_CALIBTRANS_H */
/**************************End of file********************************/
