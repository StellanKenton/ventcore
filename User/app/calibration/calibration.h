/************************************************************************************
* @file     : calibration.h
* @brief    : Persistent calibration data interface.
* @details  : Declares the legacy EEPROM record layouts and read-only runtime access.
* @author   :
* @date     :
* @version  :
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_CALIBRATION_CALIBRATION_H
#define USER_APP_CALIBRATION_CALIBRATION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CALIBRATION_STATUS_OK                    1
#define CALIBRATION_ERROR_EEPROM               (-1)
#define CALIBRATION_ERROR_HEADER               (-2)
#define CALIBRATION_ERROR_CRC                  (-3)
#define CALIBRATION_ERROR_INCOMPLETE           (-4)

#define CALIBRATION_HEADER_BASE             0xA500U
#define CALIBRATION_ZERO_ADDRESS            0x8000U
#define CALIBRATION_PRESSURE_ADDRESS        0x8200U
#define CALIBRATION_PROX_FLOW_ADDRESS       0x8400U
#define CALIBRATION_OXYGEN_VALVE_ADDRESS    0x8800U
#define CALIBRATION_AIR_OXYGEN_MIX_ADDRESS  0x8A00U
#define CALIBRATION_PRESSURE_POINT_COUNT          8U
#define CALIBRATION_PRESSURE_CHANNEL_COUNT        3U
#define CALIBRATION_DIFF_FLOW_POINT_COUNT        32U
#define CALIBRATION_MIX_POINT_COUNT              16U

typedef enum eCalibrationType {
    CALIBRATION_TYPE_ZERO = 0,
    CALIBRATION_TYPE_PRESSURE,
    CALIBRATION_TYPE_PROX_FLOW,
    CALIBRATION_TYPE_OXYGEN_VALVE,
    CALIBRATION_TYPE_AIR_OXYGEN_MIX,
    CALIBRATION_TYPE_COUNT
} eCalibrationType;

typedef struct stCalibrationZero {
    uint16_t inspPressureAd;
    uint16_t peepPressureAd;
    uint16_t expPressureAd;
    uint16_t proxPressureAd;
    uint16_t sampleCount;
} stCalibrationZero;

typedef struct stCalibrationPressure {
    float speedRps[CALIBRATION_PRESSURE_POINT_COUNT];
    float adcValues[CALIBRATION_PRESSURE_CHANNEL_COUNT][CALIBRATION_PRESSURE_POINT_COUNT];
    float pressureValues[CALIBRATION_PRESSURE_POINT_COUNT];
} stCalibrationPressure;

typedef struct stCalibrationProxFlow {
    float adultFlowAd[CALIBRATION_DIFF_FLOW_POINT_COUNT];
    float adultFlow[CALIBRATION_DIFF_FLOW_POINT_COUNT];
    float neoFlowAd[CALIBRATION_DIFF_FLOW_POINT_COUNT];
    float neoFlow[CALIBRATION_DIFF_FLOW_POINT_COUNT];
} stCalibrationProxFlow;

typedef struct stCalibrationOxygenValve {
    float dutyCycle[CALIBRATION_MIX_POINT_COUNT];
    float currentAd[CALIBRATION_MIX_POINT_COUNT];
    float flowValues[CALIBRATION_MIX_POINT_COUNT];
    uint8_t dataValid[CALIBRATION_MIX_POINT_COUNT];
    uint8_t validCount;
} stCalibrationOxygenValve;

typedef struct stCalibrationAirOxygenMix {
    float targetFlowLpm[CALIBRATION_MIX_POINT_COUNT];
    float turbineFlowLpm[CALIBRATION_MIX_POINT_COUNT];
    float oxygenFlowLpm[CALIBRATION_MIX_POINT_COUNT];
    float mixCoefficient[CALIBRATION_MIX_POINT_COUNT];
    uint8_t dataValid[CALIBRATION_MIX_POINT_COUNT];
    uint8_t validCount;
} stCalibrationAirOxygenMix;

int8_t calibrationInit(void);
uint8_t calibrationIsValid(eCalibrationType type);
const stCalibrationZero *calibrationGetZero(void);
const stCalibrationPressure *calibrationGetPressure(void);
const stCalibrationProxFlow *calibrationGetProxFlow(void);
const stCalibrationOxygenValve *calibrationGetOxygenValve(void);
const stCalibrationAirOxygenMix *calibrationGetAirOxygenMix(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_CALIBRATION_CALIBRATION_H */
/**************************End of file********************************/
