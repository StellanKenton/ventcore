/************************************************************************************
* @file     : calibration.c
* @brief    : Persistent calibration data loader.
* @details  : Loads existing EEPROM records after validating their header and CRC-16.
* @author   :
* @date     :
* @version  :
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "calibration.h"

#include <stddef.h>
#include <string.h>

#include "log.h"
#include "m24512r.h"

_Static_assert(sizeof(stCalibrationZero) == 10U, "zero calibration layout changed");
_Static_assert(sizeof(stCalibrationPressure) == 160U, "pressure calibration layout changed");
_Static_assert(sizeof(stCalibrationProxFlow) == 512U, "prox flow calibration layout changed");
_Static_assert(sizeof(stCalibrationOxygenValve) == 212U, "oxygen valve calibration layout changed");
_Static_assert(sizeof(stCalibrationAirOxygenMix) == 276U, "air oxygen mix calibration layout changed");

static const char *const gCalibrationTag = "calibration";
static stCalibrationZero gCalibrationZero;
static stCalibrationPressure gCalibrationPressure;
static stCalibrationProxFlow gCalibrationProxFlow;
static stCalibrationOxygenValve gCalibrationOxygenValve;
static stCalibrationAirOxygenMix gCalibrationAirOxygenMix;
static uint8_t gCalibrationValidMask = 0U;

static uint16_t calibrationCalculateCrc(const uint8_t *data, uint16_t length) {
    uint16_t lCrc = 0xFFFFU;
    uint16_t lIndex;
    uint8_t lBit;

    for (lIndex = 0U; lIndex < length; lIndex++) {
        lCrc ^= data[lIndex];
        for (lBit = 0U; lBit < 8U; lBit++) {
            lCrc = ((lCrc & 1U) != 0U) ? (uint16_t)((lCrc >> 1U) ^ 0xA001U) : (uint16_t)(lCrc >> 1U);
        }
    }
    return lCrc;
}

static int8_t calibrationLoadRecord(eCalibrationType type, uint16_t address, void *record, uint16_t size) {
    uint8_t lBytes[2U];
    uint16_t lStoredValue;
    int8_t lStatus;

    lStatus = m24512rReadBytes(address, lBytes, sizeof(lBytes));
    if (lStatus != M24512R_STATUS_OK) {
        return CALIBRATION_ERROR_EEPROM;
    }
    lStoredValue = (uint16_t)((uint16_t)lBytes[0U] | ((uint16_t)lBytes[1U] << 8U));
    if (lStoredValue != (uint16_t)(CALIBRATION_HEADER_BASE | (uint16_t)type)) {
        return CALIBRATION_ERROR_HEADER;
    }

    lStatus = m24512rReadBytes((uint16_t)(address + 2U), (uint8_t *)record, size);
    if (lStatus != M24512R_STATUS_OK) {
        return CALIBRATION_ERROR_EEPROM;
    }
    lStatus = m24512rReadBytes((uint16_t)(address + 2U + size), lBytes, sizeof(lBytes));
    if (lStatus != M24512R_STATUS_OK) {
        return CALIBRATION_ERROR_EEPROM;
    }
    lStoredValue = (uint16_t)((uint16_t)lBytes[0U] | ((uint16_t)lBytes[1U] << 8U));
    return (lStoredValue == calibrationCalculateCrc((const uint8_t *)record, size)) ?
           CALIBRATION_STATUS_OK : CALIBRATION_ERROR_CRC;
}

static void calibrationLoadOne(eCalibrationType type, uint16_t address, void *record, uint16_t size) {
    int8_t lStatus = calibrationLoadRecord(type, address, record, size);

    if (lStatus == CALIBRATION_STATUS_OK) {
        gCalibrationValidMask |= (uint8_t)(1U << (uint8_t)type);
        LOG_I(gCalibrationTag, "loaded type=%u size=%u", (unsigned int)type, (unsigned int)size);
    } else {
        memset(record, 0, size);
        LOG_W(gCalibrationTag, "load failed type=%u status=%d", (unsigned int)type, (int)lStatus);
    }
}

int8_t calibrationInit(void) {
    if (m24512rInit() != M24512R_STATUS_OK) {
        LOG_E(gCalibrationTag, "EEPROM init failed");
        return CALIBRATION_ERROR_EEPROM;
    }

    gCalibrationValidMask = 0U;
    calibrationLoadOne(CALIBRATION_TYPE_ZERO, CALIBRATION_ZERO_ADDRESS,
                       &gCalibrationZero, sizeof(gCalibrationZero));
    calibrationLoadOne(CALIBRATION_TYPE_PRESSURE, CALIBRATION_PRESSURE_ADDRESS,
                       &gCalibrationPressure, sizeof(gCalibrationPressure));
    calibrationLoadOne(CALIBRATION_TYPE_PROX_FLOW, CALIBRATION_PROX_FLOW_ADDRESS,
                       &gCalibrationProxFlow, sizeof(gCalibrationProxFlow));
    calibrationLoadOne(CALIBRATION_TYPE_OXYGEN_VALVE, CALIBRATION_OXYGEN_VALVE_ADDRESS,
                       &gCalibrationOxygenValve, sizeof(gCalibrationOxygenValve));
    calibrationLoadOne(CALIBRATION_TYPE_AIR_OXYGEN_MIX, CALIBRATION_AIR_OXYGEN_MIX_ADDRESS,
                       &gCalibrationAirOxygenMix, sizeof(gCalibrationAirOxygenMix));

    return (gCalibrationValidMask == (uint8_t)((1U << CALIBRATION_TYPE_COUNT) - 1U)) ?
           CALIBRATION_STATUS_OK : CALIBRATION_ERROR_INCOMPLETE;
}

uint8_t calibrationIsValid(eCalibrationType type) {
    if ((uint32_t)type >= (uint32_t)CALIBRATION_TYPE_COUNT) {
        return 0U;
    }
    return ((gCalibrationValidMask & (uint8_t)(1U << (uint8_t)type)) != 0U) ? 1U : 0U;
}

const stCalibrationZero *calibrationGetZero(void) {
    return (calibrationIsValid(CALIBRATION_TYPE_ZERO) != 0U) ? &gCalibrationZero : NULL;
}

const stCalibrationPressure *calibrationGetPressure(void) {
    return (calibrationIsValid(CALIBRATION_TYPE_PRESSURE) != 0U) ? &gCalibrationPressure : NULL;
}

const stCalibrationProxFlow *calibrationGetProxFlow(void) {
    return (calibrationIsValid(CALIBRATION_TYPE_PROX_FLOW) != 0U) ? &gCalibrationProxFlow : NULL;
}

const stCalibrationOxygenValve *calibrationGetOxygenValve(void) {
    return (calibrationIsValid(CALIBRATION_TYPE_OXYGEN_VALVE) != 0U) ? &gCalibrationOxygenValve : NULL;
}

const stCalibrationAirOxygenMix *calibrationGetAirOxygenMix(void) {
    return (calibrationIsValid(CALIBRATION_TYPE_AIR_OXYGEN_MIX) != 0U) ? &gCalibrationAirOxygenMix : NULL;
}

/**************************End of file********************************/
