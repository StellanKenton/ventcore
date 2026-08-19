/************************************************************************************
* @file     : calibtrans.c
* @brief    : Calibration table conversion implementation.
* @details  : Uses linear interpolation and clamps inputs at calibrated endpoints.
* @author   :
* @date     :
* @version  :
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "calibtrans.h"

#include <stddef.h>

#include "calibration.h"

static uint8_t calibtransPointIsValid(const uint8_t *validFlags, uint8_t index) {
    return ((validFlags == NULL) || (validFlags[index] != 0U)) ? 1U : 0U;
}

static int8_t calibtransInterpolate(const float *inputValues, const float *outputValues,
                                    const uint8_t *validFlags, uint8_t pointCount,
                                    uint8_t validCount, float inputValue, float *outputValue) {
    uint8_t lFirstIndex = 0U;
    uint8_t lLastIndex = 0U;
    uint8_t lPreviousIndex = 0U;
    uint8_t lIndex;
    uint8_t lFoundCount = 0U;
    float lInputDelta;

    if ((inputValues == NULL) || (outputValues == NULL) || (outputValue == NULL) ||
        (pointCount == 0U) || (validCount == 0U) || (inputValue != inputValue)) {
        return CALIBTRANS_ERROR_ARGUMENT;
    }

    for (lIndex = 0U; (lIndex < pointCount) && (lFoundCount < validCount); lIndex++) {
        if (calibtransPointIsValid(validFlags, lIndex) == 0U) {
            continue;
        }
        if ((inputValues[lIndex] != inputValues[lIndex]) ||
            (outputValues[lIndex] != outputValues[lIndex])) {
            return CALIBTRANS_ERROR_TABLE;
        }
        if (lFoundCount == 0U) {
            lFirstIndex = lIndex;
        }
        lLastIndex = lIndex;
        lFoundCount++;
    }
    if (lFoundCount != validCount) {
        return CALIBTRANS_ERROR_TABLE;
    }
    if (lFoundCount == 1U) {
        *outputValue = outputValues[lFirstIndex];
        return CALIBTRANS_STATUS_OK;
    }

    if (((inputValues[lFirstIndex] <= inputValues[lLastIndex]) &&
         (inputValue <= inputValues[lFirstIndex])) ||
        ((inputValues[lFirstIndex] > inputValues[lLastIndex]) &&
         (inputValue >= inputValues[lFirstIndex]))) {
        *outputValue = outputValues[lFirstIndex];
        return CALIBTRANS_STATUS_OK;
    }
    if (((inputValues[lFirstIndex] <= inputValues[lLastIndex]) &&
         (inputValue >= inputValues[lLastIndex])) ||
        ((inputValues[lFirstIndex] > inputValues[lLastIndex]) &&
         (inputValue <= inputValues[lLastIndex]))) {
        *outputValue = outputValues[lLastIndex];
        return CALIBTRANS_STATUS_OK;
    }

    lPreviousIndex = lFirstIndex;
    lFoundCount = 1U;
    for (lIndex = (uint8_t)(lFirstIndex + 1U);
         (lIndex < pointCount) && (lFoundCount < validCount); lIndex++) {
        if (calibtransPointIsValid(validFlags, lIndex) == 0U) {
            continue;
        }
        lFoundCount++;
        if (((inputValue >= inputValues[lPreviousIndex]) && (inputValue <= inputValues[lIndex])) ||
            ((inputValue <= inputValues[lPreviousIndex]) && (inputValue >= inputValues[lIndex]))) {
            lInputDelta = inputValues[lIndex] - inputValues[lPreviousIndex];
            if (lInputDelta == 0.0f) {
                return CALIBTRANS_ERROR_TABLE;
            }
            *outputValue = outputValues[lPreviousIndex] +
                           ((inputValue - inputValues[lPreviousIndex]) /
                            lInputDelta) *
                           (outputValues[lIndex] - outputValues[lPreviousIndex]);
            return CALIBTRANS_STATUS_OK;
        }
        lPreviousIndex = lIndex;
    }

    return CALIBTRANS_ERROR_TABLE;
}

static int8_t calibtransPressure(uint8_t channelIndex, float adcValue, float *pressureValue) {
    const stCalibrationPressure *lCalibration;

    if ((channelIndex >= CALIBRATION_PRESSURE_CHANNEL_COUNT) || (pressureValue == NULL)) {
        return CALIBTRANS_ERROR_ARGUMENT;
    }
    lCalibration = calibrationGetPressure();
    if (lCalibration == NULL) {
        return CALIBTRANS_ERROR_NOT_READY;
    }
    return calibtransInterpolate(lCalibration->adcValues[channelIndex],
                                 lCalibration->pressureValues, NULL,
                                 CALIBRATION_PRESSURE_POINT_COUNT,
                                 CALIBRATION_PRESSURE_POINT_COUNT,
                                 adcValue, pressureValue);
}

int8_t calibtransInspPrs(float adcValue, float *pressureValue) {
    return calibtransPressure(1U, adcValue, pressureValue);
}

int8_t calibtransPeepPrs(float adcValue, float *pressureValue) {
    return calibtransPressure(0U, adcValue, pressureValue);
}

int8_t calibtransExpPrs(float adcValue, float *pressureValue) {
    return calibtransPressure(2U, adcValue, pressureValue);
}

int8_t calibtransAdultProxFlow(float adcValue, float *flowValue) {
    const stCalibrationProxFlow *lCalibration = calibrationGetProxFlow();

    if (flowValue == NULL) {
        return CALIBTRANS_ERROR_ARGUMENT;
    }
    if (lCalibration == NULL) {
        return CALIBTRANS_ERROR_NOT_READY;
    }
    return calibtransInterpolate(lCalibration->adultFlowAd, lCalibration->adultFlow,
                                 NULL, CALIBRATION_DIFF_FLOW_POINT_COUNT,
                                 CALIBRATION_DIFF_FLOW_POINT_COUNT,
                                 adcValue, flowValue);
}

int8_t calibtransNeoProxFlow(float adcValue, float *flowValue) {
    const stCalibrationProxFlow *lCalibration = calibrationGetProxFlow();

    if (flowValue == NULL) {
        return CALIBTRANS_ERROR_ARGUMENT;
    }
    if (lCalibration == NULL) {
        return CALIBTRANS_ERROR_NOT_READY;
    }
    return calibtransInterpolate(lCalibration->neoFlowAd, lCalibration->neoFlow,
                                 NULL, CALIBRATION_DIFF_FLOW_POINT_COUNT,
                                 CALIBRATION_DIFF_FLOW_POINT_COUNT,
                                 adcValue, flowValue);
}

int8_t calibtransOxygenValve(float dutyCycle, float *flowValue) {
    const stCalibrationOxygenValve *lCalibration;

    if (flowValue == NULL) {
        return CALIBTRANS_ERROR_ARGUMENT;
    }
    lCalibration = calibrationGetOxygenValve();
    if (lCalibration == NULL) {
        return CALIBTRANS_ERROR_NOT_READY;
    }
    return calibtransInterpolate(lCalibration->dutyCycle, lCalibration->flowValues,
                                 lCalibration->dataValid, CALIBRATION_MIX_POINT_COUNT,
                                 lCalibration->validCount, dutyCycle, flowValue);
}

/**************************End of file********************************/
