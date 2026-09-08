/************************************************************************************
* @file     : calibtrans.c
* @brief    : Calibration table conversion implementation.
* @details  : Uses linear interpolation with optional endpoint extrapolation.
* @author   :
* @date     :
* @version  :
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "calibtrans.h"

#include <stddef.h>
#include <math.h>

#include "calibration.h"

static uint8_t calibtransPointIsValid(const uint8_t *validFlags, uint8_t index) {
    return ((validFlags == NULL) || (validFlags[index] != 0U)) ? 1U : 0U;
}

static int8_t calibtransLinear(const float *inputValues, const float *outputValues,
                               uint8_t firstIndex, uint8_t secondIndex,
                               float inputValue, float *outputValue) {
    float lInputDelta = inputValues[secondIndex] - inputValues[firstIndex];

    if (lInputDelta == 0.0F) {
        return CALIBTRANS_ERROR_TABLE;
    }
    *outputValue = outputValues[firstIndex] +
                   ((inputValue - inputValues[firstIndex]) / lInputDelta) *
                   (outputValues[secondIndex] - outputValues[firstIndex]);
    return CALIBTRANS_STATUS_OK;
}

static int8_t calibtransInterpolate(const float *inputValues, const float *outputValues,
                                    const uint8_t *validFlags, uint8_t pointCount,
                                    uint8_t validCount, uint8_t extrapolateEndpoints,
                                    float inputValue, float *outputValue) {
    uint8_t lFirstIndex = 0U;
    uint8_t lSecondIndex = 0U;
    uint8_t lPenultimateIndex = 0U;
    uint8_t lLastIndex = 0U;
    uint8_t lPreviousIndex = 0U;
    uint8_t lIndex;
    uint8_t lFoundCount = 0U;

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
        } else {
            if (lFoundCount == 1U) {
                lSecondIndex = lIndex;
            }
            lPenultimateIndex = lLastIndex;
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
        if (extrapolateEndpoints != 0U) {
            return calibtransLinear(inputValues, outputValues, lFirstIndex, lSecondIndex,
                                    inputValue, outputValue);
        }
        *outputValue = outputValues[lFirstIndex];
        return CALIBTRANS_STATUS_OK;
    }
    if (((inputValues[lFirstIndex] <= inputValues[lLastIndex]) &&
         (inputValue >= inputValues[lLastIndex])) ||
        ((inputValues[lFirstIndex] > inputValues[lLastIndex]) &&
         (inputValue <= inputValues[lLastIndex]))) {
        if (extrapolateEndpoints != 0U) {
            return calibtransLinear(inputValues, outputValues, lPenultimateIndex, lLastIndex,
                                    inputValue, outputValue);
        }
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
            return calibtransLinear(inputValues, outputValues, lPreviousIndex, lIndex,
                                    inputValue, outputValue);
        }
        lPreviousIndex = lIndex;
    }

    return CALIBTRANS_ERROR_TABLE;
}

static int8_t calibtransPressure(const float *calibrationAdcValues, uint16_t currentZeroAd,
                                 const float *calibrationPressureValues, float adcValue,
                                 float *pressureValue) {
    float lCompensatedAd;

    if ((calibrationAdcValues == NULL) || (calibrationPressureValues == NULL) ||
        (pressureValue == NULL)) {
        return CALIBTRANS_ERROR_ARGUMENT;
    }
    lCompensatedAd = adcValue - (float)currentZeroAd + calibrationAdcValues[0U];
    return calibtransInterpolate(calibrationAdcValues,
                                 calibrationPressureValues, NULL,
                                 CALIBRATION_PRESSURE_POINT_COUNT,
                                 CALIBRATION_PRESSURE_POINT_COUNT,
                                 0U,
                                 lCompensatedAd, pressureValue);
}

int8_t calibtransInspPrs(float adcValue, float *pressureValue) {
    const stCalibrationZero *lZero;
    const stCalibrationPressure *lCalibration;

    if (pressureValue == NULL) {
        return CALIBTRANS_ERROR_ARGUMENT;
    }
    lZero = calibrationGetZero();
    lCalibration = calibrationGetPressure();
    if ((lZero == NULL) || (lCalibration == NULL)) {
        return CALIBTRANS_ERROR_NOT_READY;
    }
    return calibtransPressure(lCalibration->inspAdcValues, lZero->inspPressureAd,
                              lCalibration->pressureValues, adcValue, pressureValue);
}

int8_t calibtransPeepPrs(float adcValue, float *pressureValue) {
    const stCalibrationZero *lZero;
    const stCalibrationPressure *lCalibration;

    if (pressureValue == NULL) {
        return CALIBTRANS_ERROR_ARGUMENT;
    }
    lZero = calibrationGetZero();
    lCalibration = calibrationGetPressure();
    if ((lZero == NULL) || (lCalibration == NULL)) {
        return CALIBTRANS_ERROR_NOT_READY;
    }
    return calibtransPressure(lCalibration->peepAdcValues, lZero->peepPressureAd,
                              lCalibration->pressureValues, adcValue, pressureValue);
}

int8_t calibtransExpPrs(float adcValue, float *pressureValue) {
    const stCalibrationZero *lZero;
    const stCalibrationPressure *lCalibration;

    if (pressureValue == NULL) {
        return CALIBTRANS_ERROR_ARGUMENT;
    }
    lZero = calibrationGetZero();
    lCalibration = calibrationGetPressure();
    if ((lZero == NULL) || (lCalibration == NULL)) {
        return CALIBTRANS_ERROR_NOT_READY;
    }
    return calibtransPressure(lCalibration->expAdcValues, lZero->expPressureAd,
                              lCalibration->pressureValues, adcValue, pressureValue);
}

int8_t calibtransPrsSpeed(float pressureValue, float *speedRps) {
    const stCalibrationPressure *lCalibration;

    if (speedRps == NULL) {
        return CALIBTRANS_ERROR_ARGUMENT;
    }
    lCalibration = calibrationGetPressure();
    if (lCalibration == NULL) {
        return CALIBTRANS_ERROR_NOT_READY;
    }
    return calibtransInterpolate(lCalibration->pressureValues, lCalibration->speedRps,
                                 NULL, CALIBRATION_PRESSURE_POINT_COUNT,
                                 CALIBRATION_PRESSURE_POINT_COUNT,
                                 0U,
                                 pressureValue, speedRps);
}

int8_t calibtransAdultProxFlow(float adcValue, float *flowValue) {
    return calibtransAdultProxFlowAtPressure(adcValue, 0.0F, flowValue);
}

/** At fixed mass flow and temperature, differential pressure varies as 1/density. */
int8_t calibtransAdultProxFlowAtPressure(float adcValue, float pressureCmh2o, float *flowValue) {
    const stCalibrationProxFlow *lCalibration = calibrationGetProxFlow();
    const stCalibrationZero *lZero = calibrationGetZero();
    float lCalibrationZeroAd;
    float lCompensatedAd;
    float lZeroFlowAd;
    float lDensityRatio = 1.0F + pressureCmh2o / CALIBTRANS_AMBIENT_PRESSURE_CMH2O;
    int8_t lStatus;

    if ((flowValue == NULL) || !isfinite(lDensityRatio) || (lDensityRatio <= 0.0F)) {
        return CALIBTRANS_ERROR_ARGUMENT;
    }
    if ((lCalibration == NULL) || (lZero == NULL)) {
        return CALIBTRANS_ERROR_NOT_READY;
    }
    lCalibrationZeroAd = (lCalibration->adultFlowAd[15U] +
                          lCalibration->adultFlowAd[16U]) * 0.5f;
    lCompensatedAd = adcValue - (float)lZero->proxPressureAd + lCalibrationZeroAd;
    if (pressureCmh2o != 0.0F) {
        lStatus = calibtransInterpolate(lCalibration->adultFlow, lCalibration->adultFlowAd,
            NULL, CALIBRATION_DIFF_FLOW_POINT_COUNT, CALIBRATION_DIFF_FLOW_POINT_COUNT,
            1U, 0.0F, &lZeroFlowAd);
        if (lStatus != CALIBTRANS_STATUS_OK) {
            return lStatus;
        }
        /* Scale differential ADC around true zero, never the absolute ADC count. */
        lCompensatedAd = lZeroFlowAd + (lCompensatedAd - lZeroFlowAd) * lDensityRatio;
    }
    return calibtransInterpolate(lCalibration->adultFlowAd, lCalibration->adultFlow,
                                 NULL, CALIBRATION_DIFF_FLOW_POINT_COUNT,
                                 CALIBRATION_DIFF_FLOW_POINT_COUNT,
                                 1U,
                                 lCompensatedAd, flowValue);
}

/** Invert the same table so zero drift is removed before nonlinear conversion. */
int8_t calibtransAdultProxFlowZeroShift(float flowValue, float *adcShift) {
    const stCalibrationProxFlow *lCalibration = calibrationGetProxFlow();
    float lZeroAd;
    float lFlowAd;
    int8_t lStatus;

    if (adcShift == NULL) {
        return CALIBTRANS_ERROR_ARGUMENT;
    }
    if (lCalibration == NULL) {
        return CALIBTRANS_ERROR_NOT_READY;
    }
    lStatus = calibtransInterpolate(lCalibration->adultFlow, lCalibration->adultFlowAd,
        NULL, CALIBRATION_DIFF_FLOW_POINT_COUNT, CALIBRATION_DIFF_FLOW_POINT_COUNT,
        1U, 0.0F, &lZeroAd);
    if (lStatus != CALIBTRANS_STATUS_OK) {
        return lStatus;
    }
    lStatus = calibtransInterpolate(lCalibration->adultFlow, lCalibration->adultFlowAd,
        NULL, CALIBRATION_DIFF_FLOW_POINT_COUNT, CALIBRATION_DIFF_FLOW_POINT_COUNT,
        1U, flowValue, &lFlowAd);
    if (lStatus == CALIBTRANS_STATUS_OK) {
        *adcShift = lFlowAd - lZeroAd;
    }
    return lStatus;
}

int8_t calibtransNeoProxFlow(float adcValue, float *flowValue) {
    const stCalibrationProxFlow *lCalibration = calibrationGetProxFlow();
    const stCalibrationZero *lZero = calibrationGetZero();
    float lCalibrationZeroAd;
    float lCompensatedAd;

    if (flowValue == NULL) {
        return CALIBTRANS_ERROR_ARGUMENT;
    }
    if ((lCalibration == NULL) || (lZero == NULL)) {
        return CALIBTRANS_ERROR_NOT_READY;
    }
    lCalibrationZeroAd = (lCalibration->neoFlowAd[15U] +
                          lCalibration->neoFlowAd[16U]) * 0.5f;
    lCompensatedAd = adcValue - (float)lZero->proxPressureAd + lCalibrationZeroAd;
    return calibtransInterpolate(lCalibration->neoFlowAd, lCalibration->neoFlow,
                                 NULL, CALIBRATION_DIFF_FLOW_POINT_COUNT,
                                 CALIBRATION_DIFF_FLOW_POINT_COUNT,
                                 1U,
                                 lCompensatedAd, flowValue);
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
                                 lCalibration->validCount, 0U, dutyCycle, flowValue);
}

/**************************End of file********************************/
