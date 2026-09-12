"""Host regression for MCM wire framing, caches and ventilation settings binding."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[2]
HARNESS = r'''

/************************************************************************************
* @file     : protocol_test.c
* @brief    : Production protocol framing and device binding regression.
***********************************************************************************/
#include <assert.h>
#include <string.h>
#include <math.h>
#include "ProtoclOfMcm.h"
#include "breathscheduler.h"
#include "phasecontroller.h"
#include "monitorengine.h"
#include "controldata.h"
#include "physalarmmanager.h"
#include "techalarmmanager.h"
#include "rtos.h"
#include "log.h"
static uint8_t gRx[2048], gTx[256];
static uint16_t gRxSize, gTxSize;
static uint8_t gRunning;
static eVentMode gMode = VENT_MD_IDLE;
static unsigned gUpdates;
static bool gUartBusy;
static uint32_t gHeartbeatTx;
static bool gAlarmStates[PHYS_ALARM_COUNT];
bool physAlarmManagerStateGet(ePhysAlarmType type) { return gAlarmStates[type]; }
static stMcmTechAlarmStatusSnapshot gTechStatus;
void techAlarmManagerSnapshotGet(stMcmTechAlarmStatusSnapshot *status) { *status = gTechStatus; }

/** Verify union sizes and every documented field position. */
static void testAlarmBitfields(void) {
    {
        stMcmPhysAlarmStatus lStatus = {0};
        assert(sizeof(lStatus) == 4U);
        lStatus.value = 0U; lStatus.bits.airwayPressureHigh = 1U;
        assert(lStatus.value == (1UL << 0));
        lStatus.value = 0U; lStatus.bits.airwayPressureLow = 1U;
        assert(lStatus.value == (1UL << 1));
        lStatus.value = 0U; lStatus.bits.fio2High = 1U;
        assert(lStatus.value == (1UL << 2));
        lStatus.value = 0U; lStatus.bits.fio2Low = 1U;
        assert(lStatus.value == (1UL << 3));
        lStatus.value = 0U; lStatus.bits.expiratoryTidalVolumeHigh = 1U;
        assert(lStatus.value == (1UL << 4));
        lStatus.value = 0U; lStatus.bits.expiratoryTidalVolumeLow = 1U;
        assert(lStatus.value == (1UL << 5));
        lStatus.value = 0U; lStatus.bits.expiratoryMinuteVentilationHigh = 1U;
        assert(lStatus.value == (1UL << 6));
        lStatus.value = 0U; lStatus.bits.expiratoryMinuteVentilationLow = 1U;
        assert(lStatus.value == (1UL << 7));
        lStatus.value = 0U; lStatus.bits.apneaAlarm = 1U;
        assert(lStatus.value == (1UL << 8));
        lStatus.value = 0U; lStatus.bits.apneaVentilationAlarm = 1U;
        assert(lStatus.value == (1UL << 9));
        lStatus.value = 0U; lStatus.bits.apneaVentilationEnd = 1U;
        assert(lStatus.value == (1UL << 10));
        lStatus.value = 0U; lStatus.bits.respiratoryRateHigh = 1U;
        assert(lStatus.value == (1UL << 11));
        lStatus.value = 0U; lStatus.bits.respiratoryRateLow = 1U;
        assert(lStatus.value == (1UL << 12));
        lStatus.value = 0U; lStatus.bits.physalarmReserve1 = 1U;
        assert(lStatus.value == (1UL << 13));
        lStatus.value = 0U; lStatus.bits.physalarmReserve2 = 1U;
        assert(lStatus.value == (1UL << 14));
        lStatus.value = 0U; lStatus.bits.inverseVentilationAlarm = 1U;
        assert(lStatus.value == (1UL << 15));
    }
    {
        stMcmTechPhysAlarmStatus lStatus = {0};
        assert(sizeof(lStatus) == 4U);
        lStatus.value = 0U; lStatus.bits.physioFaultPeepTooHigh = 1U;
        assert(lStatus.value == (1UL << 0));
        lStatus.value = 0U; lStatus.bits.physioFaultPeepTooLow = 1U;
        assert(lStatus.value == (1UL << 1));
        lStatus.value = 0U; lStatus.bits.physioFaultPipelineBlockage = 1U;
        assert(lStatus.value == (1UL << 2));
        lStatus.value = 0U; lStatus.bits.physioFaultInspBranchBlockage = 1U;
        assert(lStatus.value == (1UL << 3));
        lStatus.value = 0U; lStatus.bits.physioFaultCpapTooHigh = 1U;
        assert(lStatus.value == (1UL << 4));
        lStatus.value = 0U; lStatus.bits.physioFaultPipelineLeak = 1U;
        assert(lStatus.value == (1UL << 5));
        lStatus.value = 0U; lStatus.bits.physioFaultPipelineDisconnect = 1U;
        assert(lStatus.value == (1UL << 6));
        lStatus.value = 0U; lStatus.bits.physioFaultPressureLimit = 1U;
        assert(lStatus.value == (1UL << 7));
        lStatus.value = 0U; lStatus.bits.physioFaultVolumeLimit = 1U;
        assert(lStatus.value == (1UL << 8));
        lStatus.value = 0U; lStatus.bits.physioFaultInspPressNotReached = 1U;
        assert(lStatus.value == (1UL << 9));
        lStatus.value = 0U; lStatus.bits.physioFaultTidalVolNotReached = 1U;
        assert(lStatus.value == (1UL << 10));
        lStatus.value = 0U; lStatus.bits.physioFaultSighCyclePressLimit = 1U;
        assert(lStatus.value == (1UL << 11));
        lStatus.value = 0U; lStatus.bits.physioFaultReserved = 1U;
        assert(lStatus.value == (1UL << 12));
        lStatus.value = 0U; lStatus.bits.physioFaultInspTimeTooLong = 1U;
        assert(lStatus.value == (1UL << 13));
        lStatus.value = 0U; lStatus.bits.physioFaultInhaledGasTempHigh = 1U;
        assert(lStatus.value == (1UL << 14));
        lStatus.value = 0U; lStatus.bits.physioFaultAmvTargetNotReached = 1U;
        assert(lStatus.value == (1UL << 15));
        lStatus.value = 0U; lStatus.bits.physioFaultO2FlowNotReached = 1U;
        assert(lStatus.value == (1UL << 16));
        lStatus.value = 0U; lStatus.bits.physioFaultPatFlowSensorFault = 1U;
        assert(lStatus.value == (1UL << 17));
        lStatus.value = 0U; lStatus.bits.physioFaultPatPressSensorFault = 1U;
        assert(lStatus.value == (1UL << 18));
        lStatus.value = 0U; lStatus.bits.physioFaultMechPipelineDisconnect = 1U;
        assert(lStatus.value == (1UL << 19));
        lStatus.value = 0U; lStatus.bits.physioFaultExpBranchBlockage = 1U;
        assert(lStatus.value == (1UL << 20));
        lStatus.value = 0U; lStatus.bits.physioFaultMaxInspNegPressure = 1U;
        assert(lStatus.value == (1UL << 21));
        lStatus.value = 0U; lStatus.bits.physioFaultInspPressureNotReleased = 1U;
        assert(lStatus.value == (1UL << 22));
        lStatus.value = 0U; lStatus.bits.physioFaultO2SourceFailure = 1U;
        assert(lStatus.value == (1UL << 23));
        lStatus.value = 0U; lStatus.bits.physioFaultProximalPressTubeDisconnect = 1U;
        assert(lStatus.value == (1UL << 24));
    }
    {
        stMcmTechAlarmStatus lStatus = {0};
        assert(sizeof(lStatus) == 4U);
        lStatus.value = 0U; lStatus.bits.techFaultInspPressSensor = 1U;
        assert(lStatus.value == (1UL << 0));
        lStatus.value = 0U; lStatus.bits.techFaultExpPressSensor = 1U;
        assert(lStatus.value == (1UL << 1));
        lStatus.value = 0U; lStatus.bits.techFaultProximalPressSensor = 1U;
        assert(lStatus.value == (1UL << 2));
        lStatus.value = 0U; lStatus.bits.techFaultInspFlowSensor = 1U;
        assert(lStatus.value == (1UL << 3));
        lStatus.value = 0U; lStatus.bits.techFaultO2FlowSensor = 1U;
        assert(lStatus.value == (1UL << 4));
        lStatus.value = 0U; lStatus.bits.techFaultAirFlowSensorTypeErr = 1U;
        assert(lStatus.value == (1UL << 5));
        lStatus.value = 0U; lStatus.bits.techFaultTriO2FlowSensorTypeErr = 1U;
        assert(lStatus.value == (1UL << 6));
        lStatus.value = 0U; lStatus.bits.techFaultProximalFlowSensorDisconnect = 1U;
        assert(lStatus.value == (1UL << 7));
        lStatus.value = 0U; lStatus.bits.techFaultProximalFlowSensorTypeErr = 1U;
        assert(lStatus.value == (1UL << 8));
        lStatus.value = 0U; lStatus.bits.techFaultProximalFlowSensorReversed = 1U;
        assert(lStatus.value == (1UL << 9));
        lStatus.value = 0U; lStatus.bits.techFaultTurbineTempSensor = 1U;
        assert(lStatus.value == (1UL << 10));
        lStatus.value = 0U; lStatus.bits.techFaultTurbineHallSignalErr = 1U;
        assert(lStatus.value == (1UL << 11));
        lStatus.value = 0U; lStatus.bits.techFaultNegativePressSensor = 1U;
        assert(lStatus.value == (1UL << 12));
        lStatus.value = 0U; lStatus.bits.techFaultO2Sensor = 1U;
        assert(lStatus.value == (1UL << 13));
        lStatus.value = 0U; lStatus.bits.techFaultAtmosphericPressSensor = 1U;
        assert(lStatus.value == (1UL << 14));
        lStatus.value = 0U; lStatus.bits.techFaultPressSensorZeroError = 1U;
        assert(lStatus.value == (1UL << 15));
        lStatus.value = 0U; lStatus.bits.techFaultSafetyValve = 1U;
        assert(lStatus.value == (1UL << 16));
        lStatus.value = 0U; lStatus.bits.techFaultThreeWayValve = 1U;
        assert(lStatus.value == (1UL << 17));
        lStatus.value = 0U; lStatus.bits.techFaultTotalInspManifold = 1U;
        assert(lStatus.value == (1UL << 18));
        lStatus.value = 0U; lStatus.bits.techFaultO2BranchDisconnect = 1U;
        assert(lStatus.value == (1UL << 19));
        lStatus.value = 0U; lStatus.bits.techFaultPowerCapDisconnect = 1U;
        assert(lStatus.value == (1UL << 20));
        lStatus.value = 0U; lStatus.bits.techFaultPeepValve = 1U;
        assert(lStatus.value == (1UL << 21));
        lStatus.value = 0U; lStatus.bits.techFaultTurbineShaft = 1U;
        assert(lStatus.value == (1UL << 22));
        lStatus.value = 0U; lStatus.bits.techFaultTurbineTempHigh = 1U;
        assert(lStatus.value == (1UL << 23));
        lStatus.value = 0U; lStatus.bits.techFaultTurbineTempOverhigh = 1U;
        assert(lStatus.value == (1UL << 24));
        lStatus.value = 0U; lStatus.bits.techFaultHepaFilterMissing = 1U;
        assert(lStatus.value == (1UL << 25));
        lStatus.value = 0U; lStatus.bits.techFaultReplaceHepaFilter = 1U;
        assert(lStatus.value == (1UL << 26));
        lStatus.value = 0U; lStatus.bits.techFaultAtmosphericCommErr = 1U;
        assert(lStatus.value == (1UL << 27));
        lStatus.value = 0U; lStatus.bits.techFaultHepaFilterPressureSensor = 1U;
        assert(lStatus.value == (1UL << 28));
        lStatus.value = 0U; lStatus.bits.techFaultMemoryError = 1U;
        assert(lStatus.value == (1UL << 29));
        lStatus.value = 0U; lStatus.bits.techFaultO2SourceLow = 1U;
        assert(lStatus.value == (1UL << 30));
    }
    {
        stMcmPowerAlarmStatus lStatus = {0};
        assert(sizeof(lStatus) == 1U);
        lStatus.value = 0U; lStatus.bits.powerFaultPcm3V3 = 1U;
        assert(lStatus.value == (1UL << 0));
        lStatus.value = 0U; lStatus.bits.powerFaultVdd24V = 1U;
        assert(lStatus.value == (1UL << 1));
        lStatus.value = 0U; lStatus.bits.powerFaultAvdd5V = 1U;
        assert(lStatus.value == (1UL << 2));
    }
    {
        stMcmCommAlarmStatus lStatus = {0};
        assert(sizeof(lStatus) == 1U);
        lStatus.value = 0U; lStatus.bits.commFaultMotorDisconnect = 1U;
        assert(lStatus.value == (1UL << 0));
        lStatus.value = 0U; lStatus.bits.commFaultPcmDisconnect = 1U;
        assert(lStatus.value == (1UL << 1));
    }
    {
        stMcmCalAlarmStatus lStatus = {0};
        assert(sizeof(lStatus) == 1U);
        lStatus.value = 0U; lStatus.bits.techAlarmCalPressureSensor = 1U;
        assert(lStatus.value == (1UL << 0));
        lStatus.value = 0U; lStatus.bits.techAlarmCalOxygenSensor = 1U;
        assert(lStatus.value == (1UL << 1));
        lStatus.value = 0U; lStatus.bits.techAlarmCalAirOxygenRatio = 1U;
        assert(lStatus.value == (1UL << 2));
        lStatus.value = 0U; lStatus.bits.techAlarmCalOxygenRatioValve = 1U;
        assert(lStatus.value == (1UL << 3));
        lStatus.value = 0U; lStatus.bits.techAlarmCalExhalationValve = 1U;
        assert(lStatus.value == (1UL << 4));
        lStatus.value = 0U; lStatus.bits.techAlarmCalProximalFlowSensor = 1U;
        assert(lStatus.value == (1UL << 5));
        lStatus.value = 0U; lStatus.bits.techAlarmCalGasSourcePressureSensor = 1U;
        assert(lStatus.value == (1UL << 6));
    }
}

/** Verify alarm wire bits, periodic full-state reporting and recovery reporting. */
static void testPhysAlarms(void) {
    const ePhysAlarmType lTypes[] = {PHYS_ALARM_AIRWAY_PRESSURE_HIGH,
        PHYS_ALARM_AIRWAY_PRESSURE_LOW, PHYS_ALARM_EXHALED_VOLUME_HIGH,
        PHYS_ALARM_EXHALED_VOLUME_LOW};
    const uint32_t lMasks[] = {1U, 2U, 16U, 32U};
    uint8_t lExpected[32];
    ProtocolProcessInit(0);
    for (unsigned lIndex = 0U; lIndex < sizeof(lTypes) / sizeof(lTypes[0]); lIndex++) {
        gAlarmStates[lTypes[lIndex]] = true;
        for (unsigned lClear = 0U; lClear < 2U; lClear++) {
            uint32_t lMask = lClear ? 0U : lMasks[lIndex];
            if (lClear) { gAlarmStates[lTypes[lIndex]] = false; }
            uint16_t lLength = ProtocolCreateDirectData(lExpected, PROTOCOL_ADDR_VCM_TO_MCM,
                false, PROTOCOL_TX_MID_PHYS_ALARM, (const uint8_t *)&lMask, sizeof(lMask));
            gTxSize = 0U;
            ProtocolPhysAlarmDataProcess(0, 490U);
            ProtocolSchedulerProcess(0);
            assert(gTxSize == 0U);
            ProtocolPhysAlarmDataProcess(0, 500U);
            ProtocolSchedulerProcess(0);
            assert(gTxSize == lLength && memcmp(gTx, lExpected, lLength) == 0);
            assert(ProtocolCheckCRC(gTx));
            gTxSize = 0U;
            ProtocolPhysAlarmDataProcess(0, 1000U);
            ProtocolSchedulerProcess(0);
            assert(gTxSize == lLength && memcmp(gTx, lExpected, lLength) == 0);
        }
    }
}

/** Check all five module widths, high/low bits, repeat reporting and recovery. */
static void testTechAlarms(void) {
    const uint8_t lValues[] = {0x11, 0, 0, 1, 1, 0, 0, 0x40, 7, 3, 0x7f};
    const uint8_t lSizes[] = {4, 4, 1, 1, 1};
    ProtocolProcessInit(0);
    for (unsigned lClear = 0U; lClear < 2U; lClear++) {
        gTechStatus.phys.value = lClear ? 0U : 0x01000011U;
        gTechStatus.tech.value = lClear ? 0U : 0x40000001U;
        gTechStatus.power.value = lClear ? 0U : 7U;
        gTechStatus.comm.value = lClear ? 0U : 3U;
        gTechStatus.cal.value = lClear ? 0U : 0x7fU;
        gTxSize = 0U;
        ProtocolTechAlarmDataProcess(0, 490U);
        ProtocolSchedulerProcess(0);
        assert(gTxSize == 0U);
        for (unsigned lRepeat = 1U; lRepeat <= 2U; lRepeat++) {
            gTxSize = 0U;
            ProtocolTechAlarmDataProcess(0, lRepeat * 500U);
            ProtocolSchedulerProcess(0);
            assert(gTxSize == 28U && gTx[2] == 0xAB && gTx[4] == 21U);
            assert(ProtocolCheckCRC(gTx));
            unsigned lOffset = 5U, lValue = 0U;
            for (unsigned lModule = 0U; lModule < 5U; lModule++) {
                assert(gTx[lOffset++] == lModule);
                assert(gTx[lOffset++] == (uint8_t)((lSizes[lModule] << 3U) | 1U));
                for (unsigned lByte = 0U; lByte < lSizes[lModule]; lByte++) {
                    assert(gTx[lOffset++] == (lClear ? 0U : lValues[lValue]));
                    lValue++;
                }
            }
        }
    }
}

void repRtosEnterCritical(void) {}
void repRtosExitCritical(void) {}
void logWrite(eLogLevel level, const char *tag, const char *format, ...) {}
bool uartIsTxBusy(uint8_t instance) { return gUartBusy; }
uint16_t uartGetRxDataCount(uint8_t instance) { return gRxSize; }
int8_t uartGetRxData(uint8_t instance, uint8_t *data, uint16_t length) {
    assert(length <= gRxSize); memcpy(data, gRx, length);
    memmove(gRx, gRx + length, gRxSize - length); gRxSize -= length;
    return UART_STATUS_OK;
}
int8_t uartSendData(uint8_t instance, const uint8_t *data, uint16_t length) {
    if (gUartBusy) { return UART_ERROR_BUSY; }
    memcpy(gTx, data, length); gTxSize = length;
    if (length == 7U && data[0] == 0xFE && data[1] == 0xFF && data[2] == 0x7F) {
        assert(data[3] == 1U && data[4] == 0U && ProtocolCheckCRC(data));
        ++gHeartbeatTx;
    }
    return UART_STATUS_OK;
}
uint8_t breathSchedulerRunningGet(void) { return gRunning; }
eVentMode breathSchedulerModeGet(void) { return gMode; }
int8_t breathSchedulerSettingsUpdate(eVentMode mode) {
    if (mode != VENT_MD_PAC && mode != VENT_MD_VAC && mode != VENT_MD_CPAP_PSV && mode != VENT_MD_PSV_ST) { return -1; }
    gMode = mode; ++gUpdates; return 1;
}
int8_t breathSchedulerStart(eVentMode mode) {
    int8_t status = breathSchedulerSettingsUpdate(mode);
    if (status == 1) { gRunning = 1; } return status;
}
int8_t breathSchedulerStop(void) { gRunning = 0; return 1; }
ePhaseControllerState phaseControllerStateGet(void) { return PHASE_INSP; }
float controlDataGet(ControlData_Index_EnumDef index) { return index == PAT_REAL_PRS ? 12.3f : -25.0f; }
float monitorEngineGet(eMonitorDataType type) { return 456.0f; }
static stBreathResult gBreathResult;
int8_t monitorEngineBreathResultGet(stBreathResult *result) {
    *result = gBreathResult;
    return MONITOR_ENGINE_SUCCESS;
}

/** Verify completed mean pressure reaches the wire with signed scaling. */
static void testMeanPressure(void) {
    uint8_t lExpected[32];
    gRunning = 1U;
    ProtocolProcessInit(0);
    for (uint32_t lIndex = 1U; lIndex <= 2U; lIndex++) {
        gBreathResult = (stBreathResult){.sequence = lIndex,
            .meanPressureCmh2o = lIndex == 1U ? 12.5F : -2.5F,
            .validMask = BREATH_RESULT_VALID_COMPLETE | BREATH_RESULT_VALID_MEAN_PRESSURE};
        SubIDCache_t lItem = {.m_id = 0x03U,
            .m_value = (uint16_t)(int16_t)(gBreathResult.meanPressureCmh2o * 10.0F),
            .m_size = E_PMEAN_SIZE, .m_scale = E_PMEAN_SCALE};
        uint16_t lLength = ProtocolCreateSubIdData(lExpected, PROTOCOL_ADDR_VCM_TO_MCM,
            false, PROTOCOL_TX_MID_MONITOR_PARAMS, (const uint8_t *)&lItem, 1U);
        ProtocolDetectDataPreProcess(0, 50U);
        ProtocolSchedulerProcess(0);
        assert(gTxSize == lLength && memcmp(gTx, lExpected, lLength) == 0);
        assert(ProtocolCheckCRC(gTx));
        gTxSize = 0U;
        ProtocolDetectDataPreProcess(0, 100U);
        ProtocolSchedulerProcess(0);
        assert(gTxSize == 0U);
    }
    gBreathResult.sequence++;
    gBreathResult.validMask = BREATH_RESULT_VALID_COMPLETE;
    ProtocolDetectDataPreProcess(0, 150U);
    ProtocolSchedulerProcess(0);
    assert(gTxSize == 0U);
    gRunning = 0U;
}

/** Verify completed minute leak reaches the wire with decimal scaling. */
static void testMinuteLeak(void) {
    uint8_t lExpected[32];
    gRunning = 1U;
    ProtocolProcessInit(0);
    for (uint32_t lIndex = 1U; lIndex <= 2U; lIndex++) {
        gBreathResult = (stBreathResult){.sequence = lIndex,
            .minuteLeakLpm = lIndex == 1U ? 12.5F : 0.0F,
            .validMask = BREATH_RESULT_VALID_COMPLETE | BREATH_RESULT_VALID_MINUTE_LEAK};
        SubIDCache_t lItem = {.m_id = 0x0BU,
            .m_value = (uint16_t)(int16_t)(gBreathResult.minuteLeakLpm * 10.0F),
            .m_size = E_MVLEAK_SIZE, .m_scale = E_MVLEAK_SCALE};
        uint16_t lLength = ProtocolCreateSubIdData(lExpected, PROTOCOL_ADDR_VCM_TO_MCM,
            false, PROTOCOL_TX_MID_MONITOR_PARAMS, (const uint8_t *)&lItem, 1U);
        ProtocolDetectDataPreProcess(0, 50U);
        ProtocolSchedulerProcess(0);
        assert(gTxSize == lLength && memcmp(gTx, lExpected, lLength) == 0);
        assert(ProtocolCheckCRC(gTx));
        gTxSize = 0U;
        ProtocolDetectDataPreProcess(0, 100U);
        ProtocolSchedulerProcess(0);
        assert(gTxSize == 0U);
    }
    gBreathResult.sequence++;
    gBreathResult.validMask = BREATH_RESULT_VALID_COMPLETE;
    ProtocolDetectDataPreProcess(0, 150U);
    ProtocolSchedulerProcess(0);
    assert(gTxSize == 0U);
    gRunning = 0U;
}

/** Verify completed leak percentage reaches the wire as an integer. */
static void testLeakPercent(void) {
    uint8_t lExpected[32];
    gRunning = 1U;
    ProtocolProcessInit(0);
    for (uint32_t lIndex = 1U; lIndex <= 2U; lIndex++) {
        gBreathResult = (stBreathResult){.sequence = lIndex,
            .leakPercent = lIndex == 1U ? 25.0F : 0.0F,
            .validMask = BREATH_RESULT_VALID_COMPLETE | BREATH_RESULT_VALID_LEAK_PERCENT};
        SubIDCache_t lItem = {.m_id = 0x0CU,
            .m_value = (uint16_t)(int16_t)(gBreathResult.leakPercent),
            .m_size = E_LEAKPERCENT_SIZE, .m_scale = E_LEAKPERCENT_SCALE};
        uint16_t lLength = ProtocolCreateSubIdData(lExpected, PROTOCOL_ADDR_VCM_TO_MCM,
            false, PROTOCOL_TX_MID_MONITOR_PARAMS, (const uint8_t *)&lItem, 1U);
        ProtocolDetectDataPreProcess(0, 50U);
        ProtocolSchedulerProcess(0);
        assert(gTxSize == lLength && memcmp(gTx, lExpected, lLength) == 0);
        assert(ProtocolCheckCRC(gTx));
        gTxSize = 0U;
        ProtocolDetectDataPreProcess(0, 100U);
        ProtocolSchedulerProcess(0);
        assert(gTxSize == 0U);
    }
    gBreathResult.sequence++;
    gBreathResult.validMask = BREATH_RESULT_VALID_COMPLETE;
    ProtocolDetectDataPreProcess(0, 150U);
    ProtocolSchedulerProcess(0);
    assert(gTxSize == 0U);
    gRunning = 0U;
}

/** Verify completed resistance reaches the wire as an integer. */
static void testResistanceInspiratory(void) {
    uint8_t lExpected[32];
    gRunning = 1U;
    ProtocolProcessInit(0);
    for (uint32_t lIndex = 1U; lIndex <= 2U; lIndex++) {
        gBreathResult = (stBreathResult){.sequence = lIndex,
            .resistanceInspiratory = lIndex == 1U ? 25.0F : 0.0F,
            .validMask = BREATH_RESULT_VALID_COMPLETE | BREATH_RESULT_VALID_RES_INSP};
        SubIDCache_t lItem = {.m_id = 0x16U,
            .m_value = (uint16_t)(int16_t)(gBreathResult.resistanceInspiratory),
            .m_size = E_RINSP_SIZE, .m_scale = E_RINSP_SCALE};
        uint16_t lLength = ProtocolCreateSubIdData(lExpected, PROTOCOL_ADDR_VCM_TO_MCM,
            false, PROTOCOL_TX_MID_MONITOR_PARAMS, (const uint8_t *)&lItem, 1U);
        ProtocolDetectDataPreProcess(0, 50U);
        ProtocolSchedulerProcess(0);
        assert(gTxSize == lLength && memcmp(gTx, lExpected, lLength) == 0);
        assert(ProtocolCheckCRC(gTx));
        gTxSize = 0U;
        ProtocolDetectDataPreProcess(0, 100U);
        ProtocolSchedulerProcess(0);
        assert(gTxSize == 0U);
    }
    gBreathResult.sequence++;
    gBreathResult.validMask = BREATH_RESULT_VALID_COMPLETE;
    ProtocolDetectDataPreProcess(0, 150U);
    ProtocolSchedulerProcess(0);
    assert(gTxSize == 0U);
    gRunning = 0U;
}

/** Verify completed resistance reaches the wire as an integer. */
static void testResistanceExpiratory(void) {
    uint8_t lExpected[32];
    gRunning = 1U;
    ProtocolProcessInit(0);
    for (uint32_t lIndex = 1U; lIndex <= 2U; lIndex++) {
        gBreathResult = (stBreathResult){.sequence = lIndex,
            .resistanceExpiratory = lIndex == 1U ? 25.0F : 0.0F,
            .validMask = BREATH_RESULT_VALID_COMPLETE | BREATH_RESULT_VALID_RES_EXP};
        SubIDCache_t lItem = {.m_id = 0x17U,
            .m_value = (uint16_t)(int16_t)(gBreathResult.resistanceExpiratory),
            .m_size = E_REXP_SIZE, .m_scale = E_REXP_SCALE};
        uint16_t lLength = ProtocolCreateSubIdData(lExpected, PROTOCOL_ADDR_VCM_TO_MCM,
            false, PROTOCOL_TX_MID_MONITOR_PARAMS, (const uint8_t *)&lItem, 1U);
        ProtocolDetectDataPreProcess(0, 50U);
        ProtocolSchedulerProcess(0);
        assert(gTxSize == lLength && memcmp(gTx, lExpected, lLength) == 0);
        assert(ProtocolCheckCRC(gTx));
        gTxSize = 0U;
        ProtocolDetectDataPreProcess(0, 100U);
        ProtocolSchedulerProcess(0);
        assert(gTxSize == 0U);
    }
    gBreathResult.sequence++;
    gBreathResult.validMask = BREATH_RESULT_VALID_COMPLETE;
    ProtocolDetectDataPreProcess(0, 150U);
    ProtocolSchedulerProcess(0);
    assert(gTxSize == 0U);
    gRunning = 0U;
}

/** Verify completed compliance reaches the wire with decimal scaling. */
static void testComplianceDynamic(void) {
    uint8_t lExpected[32];
    gRunning = 1U;
    ProtocolProcessInit(0);
    for (uint32_t lIndex = 1U; lIndex <= 2U; lIndex++) {
        gBreathResult = (stBreathResult){.sequence = lIndex,
            .complianceDynamic = lIndex == 1U ? 25.5F : 0.0F,
            .validMask = BREATH_RESULT_VALID_COMPLETE | BREATH_RESULT_VALID_C_DYNC};
        SubIDCache_t lItem = {.m_id = 0x19U,
            .m_value = (uint16_t)(int16_t)(gBreathResult.complianceDynamic * 10.0F),
            .m_size = E_CDYN_SIZE, .m_scale = E_CDYN_SCALE};
        uint16_t lLength = ProtocolCreateSubIdData(lExpected, PROTOCOL_ADDR_VCM_TO_MCM,
            false, PROTOCOL_TX_MID_MONITOR_PARAMS, (const uint8_t *)&lItem, 1U);
        ProtocolDetectDataPreProcess(0, 50U);
        ProtocolSchedulerProcess(0);
        assert(gTxSize == lLength && memcmp(gTx, lExpected, lLength) == 0);
        assert(ProtocolCheckCRC(gTx));
        gTxSize = 0U;
        ProtocolDetectDataPreProcess(0, 100U);
        ProtocolSchedulerProcess(0);
        assert(gTxSize == 0U);
    }
    gBreathResult.sequence++;
    gBreathResult.validMask = BREATH_RESULT_VALID_COMPLETE;
    ProtocolDetectDataPreProcess(0, 150U);
    ProtocolSchedulerProcess(0);
    assert(gTxSize == 0U);
    gRunning = 0U;
}

/** Verify completed compliance reaches the wire with decimal scaling. */
static void testComplianceStatic(void) {
    uint8_t lExpected[32];
    gRunning = 1U;
    ProtocolProcessInit(0);
    for (uint32_t lIndex = 1U; lIndex <= 2U; lIndex++) {
        gBreathResult = (stBreathResult){.sequence = lIndex,
            .complianceStatic = lIndex == 1U ? 25.5F : 0.0F,
            .validMask = BREATH_RESULT_VALID_COMPLETE | BREATH_RESULT_VALID_C_STAT};
        SubIDCache_t lItem = {.m_id = 0x18U,
            .m_value = (uint16_t)(int16_t)(gBreathResult.complianceStatic * 10.0F),
            .m_size = E_CSTAT_SIZE, .m_scale = E_CSTAT_SCALE};
        uint16_t lLength = ProtocolCreateSubIdData(lExpected, PROTOCOL_ADDR_VCM_TO_MCM,
            false, PROTOCOL_TX_MID_MONITOR_PARAMS, (const uint8_t *)&lItem, 1U);
        ProtocolDetectDataPreProcess(0, 50U);
        ProtocolSchedulerProcess(0);
        assert(gTxSize == lLength && memcmp(gTx, lExpected, lLength) == 0);
        assert(ProtocolCheckCRC(gTx));
        gTxSize = 0U;
        ProtocolDetectDataPreProcess(0, 100U);
        ProtocolSchedulerProcess(0);
        assert(gTxSize == 0U);
    }
    gBreathResult.sequence++;
    gBreathResult.validMask = BREATH_RESULT_VALID_COMPLETE;
    ProtocolDetectDataPreProcess(0, 150U);
    ProtocolSchedulerProcess(0);
    assert(gTxSize == 0U);
    gRunning = 0U;
}

/** Deliver bytes through the production UART-to-parser path. */
static void feed(const uint8_t *data, uint16_t length) {
    memcpy(gRx + gRxSize, data, length); gRxSize += length;
    assert(ProtocolReceiveData(0) == PROTOCOL_OK);
}
/** Create a single-sub-ID MCM frame using the original wire format. */
static uint16_t frame(uint8_t *data, uint8_t mid, uint8_t id, uint32_t value, uint8_t size, uint8_t scale) {
    SubIDCache_t item = {.m_id=id, .m_value=value, .m_size=size, .m_scale=scale};
    return ProtocolCreateSubIdData(data, PROTOCOL_ADDR_MCM_TO_VCM, false, mid, (const uint8_t *)&item, 1);
}
/** Receive one setting, then let VentTask consume it. */
static void send(uint8_t mid, uint8_t id, uint32_t value, uint8_t size, uint8_t scale) {
    uint8_t bytes[32]; uint16_t length = frame(bytes, mid, id, value, size, scale);
    feed(bytes, length); assert(ProtocolProcessRxData(0) == PROTOCOL_OK);
    protocolApplyReceivedSettings();
}
/** Model UART completions between CommTask ticks for simultaneous alarm frames. */
static void processWithTxCompletion(void) {
    ProtocolProcessMain(0);
    for (unsigned lFrame = 0U; lFrame < 3U; lFrame++) {
        ProtocolSchedulerProcess(0);
    }
}

/** Exercise the full task processing path with continuous and burst heartbeats. */
static void testHeartbeat(void) {
    uint8_t heartbeat[7];
    stProtocolHeartbeatStats stats;
    gRunning = 0;
    ProtocolProcessInit(0);
    uint16_t length = ProtocolCreateDirectData(heartbeat, PROTOCOL_ADDR_MCM_TO_VCM, false, 0x7F, NULL, 0);
    assert(length == 7);
    /* Every request receives exactly one response, without an ACK/retry storm. */
    for (unsigned i = 0; i < 1000; ++i) {
        memcpy(gRx, heartbeat, length); gRxSize = length;
        processWithTxCompletion();
        assert(gHeartbeatTx == i + 1U);
    }
    for (unsigned i = 0; i < 510; ++i) { processWithTxCompletion(); }
    assert(gHeartbeatTx == 1000U && !ProtocolIsMCMConnected());
    /* A burst cannot collapse into one reply. Busy transport must retain replies. */
    gUartBusy = true;
    for (unsigned i = 0; i < 8; ++i) { memcpy(gRx + i * 7, heartbeat, 7); }
    gRxSize = 56;
    for (unsigned i = 0; i < 8; ++i) { processWithTxCompletion(); }
    assert(gHeartbeatTx == 1000U && ProtocolIsMCMConnected());
    gUartBusy = false;
    for (unsigned i = 0; i < 8; ++i) { processWithTxCompletion(); }
    assert(gHeartbeatTx == 1008U);
    /* Fill the high-priority queue; keep pending responses until space is available. */
    gUartBusy = true;
    for (unsigned i = 0; i < 100; ++i) {
        memcpy(gRx, heartbeat, 7); gRxSize = 7; processWithTxCompletion();
    }
    protocolHeartbeatStatsGet(&stats);
    assert(stats.received == 1108U && stats.pending > 0U && stats.overflow == 0U);
    gUartBusy = false;
    for (unsigned i = 0; i < 110; ++i) { processWithTxCompletion(); }
    protocolHeartbeatStatsGet(&stats);
    assert(stats.received == 1108U && stats.transmitted == 1108U && stats.pending == 0U);
    /* CRC-corrupt and fragmented requests cannot elicit an early reply. */
    heartbeat[6] ^= 1;
    memcpy(gRx, heartbeat, 7); gRxSize = 7; processWithTxCompletion();
    assert(gHeartbeatTx == 1108U);
    ProtocolProcessInit(0);
    heartbeat[6] ^= 1;
    memcpy(gRx, heartbeat, 4); gRxSize = 4; processWithTxCompletion();
    assert(gHeartbeatTx == 1108U);
    memcpy(gRx, heartbeat + 4, 3); gRxSize = 3; processWithTxCompletion();
    assert(gHeartbeatTx == 1109U);
    /* A request with needAck retains generic echo behavior plus one heartbeat reply. */
    ProtocolCreateDirectData(heartbeat, PROTOCOL_ADDR_MCM_TO_VCM, true, 0x7F, NULL, 0);
    memcpy(gRx, heartbeat, 7); gRxSize = 7; ProtocolProcessMain(0);
    assert(gTxSize == 7 && memcmp(gTx, heartbeat, 7) == 0);
    processWithTxCompletion(); assert(gHeartbeatTx == 1110U);
    for (unsigned i = 0; i < 510; ++i) { processWithTxCompletion(); }
    assert(gHeartbeatTx == 1110U);
}

int main(void) {
    uint8_t bytes[256];
    ProtocolPacket_t packet;
    assert(ProtocolProcessInit(0) == PROTOCOL_OK);
    assert(!ProtocolIsMCMConnected());
    float fixedVt = GetVentVacSettings()->tidalVolume;
    uint16_t fixedTveLow = GetVentLimitSettings()->tidalVolumeLow;
    uint16_t length = frame(bytes, 0xAF, 0x0E, 650, 2, 0);
    feed(bytes, 4); assert(ProtocolProcessRxData(0) == PROTOCOL_INVALID_PACKET);
    assert(!ProtocolGetRxVentParamsCache()->m_valid[14]);
    feed(bytes + 4, length - 4); assert(ProtocolProcessRxData(0) == PROTOCOL_OK);
    assert(ProtocolGetRxVentParamsCache()->m_tidalVolume == 650);
    assert(ProtocolIsMCMConnected());
    protocolApplyReceivedSettings(); assert(GetVentVacSettings()->tidalVolume == fixedVt);
    send(0xAF, 1, 1, 1, 0); /* VAC mode alone cannot start ventilation. */
    assert(!gRunning);
    send(0xAC, 1, 550, 2, 1);
    assert(ProtocolGetRxAlarmLimitsCache()->m_pAirwayHigh == 550);
    assert(GetVentLimitSettings()->pressureHigh == 55.0f);
    assert(GetVentLimitSettings()->tidalVolumeLow == fixedTveLow);
    /* Alarm limits apply with local ventilation settings and retain absent fields. */
    send(0xAC, 0, 125, 2, 1);
    send(0xAC, 2, 123, 2, 1);
    send(0xAC, 3, 25, 2, 1);
    send(0xAC, 4, 800, 2, 0);
    send(0xAC, 5, 250, 2, 0);
    send(0xAC, 6, 90, 1, 0);
    send(0xAC, 7, 20, 1, 0);
    send(0xAC, 8, 40, 1, 0);
    send(0xAC, 9, 8, 1, 0);
    send(0xAC, 10, 15, 1, 0);
    assert(GetVentLimitSettings()->pressureLow == 12.5f);
    assert(fabsf(GetVentLimitSettings()->minuteVolumeHigh - 12.3f) < 0.001f);
    assert(GetVentLimitSettings()->minuteVolumeLow == 2.5f);
    assert(GetVentLimitSettings()->tidalVolumeHigh == 800U);
    assert(GetVentLimitSettings()->tidalVolumeLow == 250U);
    assert(GetVentLimitSettings()->o2PercentHigh == 90U);
    assert(GetVentLimitSettings()->o2PercentLow == 20U);
    assert(GetVentLimitSettings()->frequencyHigh == 40U);
    assert(GetVentLimitSettings()->frequencyLow == 8U);
    assert(GetVentLimitSettings()->apneaTimeHigh == 15U);
    assert(GetVentCpapPsvSettings()->apneaAlarmTimeMs == 15000U);
    assert(GetVentLimitSettings()->apneaTimeHigh == 15U);
    assert(GetVentLimitSettings()->pressureHigh == 55.0f);
    /* Corrupt alarm frames must leave cached and applied limits intact. */
    length = frame(bytes, 0xAC, 1, 600, 2, 1); bytes[length - 1] ^= 1U;
    feed(bytes, length); assert(ProtocolProcessRxData(0) == PROTOCOL_CRC_ERROR);
    protocolApplyReceivedSettings();
    assert(GetVentLimitSettings()->pressureHigh == 55.0f);
    assert(ProtocolGetRxAlarmLimitsCache()->m_pAirwayHigh == 550U);
    ProtocolProcessInit(0); /* Clear the rejected frame from transport. */
    GetVentPatientSettings()->useHostSettings = 1;
    protocolApplyReceivedSettings();
    assert(GetVentVacSettings()->tidalVolume == 650);
    assert(GetVentLimitSettings()->pressureHigh == 55.0f);
    assert(GetVentLimitSettings()->tidalVolumeHigh == 800U);
    assert(GetVentLimitSettings()->apneaTimeHigh == 15U);
    send(0xAC, 1, 600, 2, 1);
    assert(GetVentLimitSettings()->pressureHigh == 60.0f);
    send(0xAF, 0x17, 125, 2, 2); assert(GetVentVacSettings()->inspTimeMs == 1250);
    send(0xAF, 0x12, (uint16_t)-20, 2, 1); assert(GetVentVacSettings()->pressureTriggerCmh2o == -2.0f);
    send(0xAE, 0, 1, 1, 0); assert(gRunning && gMode == VENT_MD_VAC);
    gTxSize = 0; ProtocolWaveDataProcess(0, 20); ProtocolSchedulerProcess(0);
    assert(gTxSize == 18 && gTx[0] == 0xFE && gTx[1] == 0xFF && gTx[2] == 0xAF);
    assert(ProtocolCheckCRC(gTx) && gTx[4] == 11 && gTx[5] == 1);
    assert((gTx[6] | (gTx[7] << 8)) == 123);
    assert((gTx[8] | (gTx[9] << 8)) == 1750);
    assert((gTx[10] | (gTx[11] << 8)) == 456);
    assert(gTx[12] == 0 && gTx[13] == 0 && gTx[14] == 0 && gTx[15] == 20);
    send(0xAE, 0, 0, 1, 0); assert(!gRunning);
    gTxSize = 0; ProtocolWaveDataProcess(0, 40); ProtocolSchedulerProcess(0); assert(gTxSize == 0);
    send(0xAE, 0, 2, 1, 0); assert(!gRunning && ProtocolGetRxVentSwitchCache()->m_command == 0);
    GetVentPatientSettings()->useHostSettings = 0; protocolApplyReceivedSettings();
    assert(GetVentVacSettings()->tidalVolume == fixedVt && GetVentLimitSettings()->pressureHigh == 60.0f);
    send(0xAE, 0, 1, 1, 0); assert(gRunning && gMode == VENT_MD_VAC);
    unsigned updates = gUpdates;
    send(0xAF, 0x0E, 700, 2, 0); assert(gUpdates == updates && GetVentVacSettings()->tidalVolume == fixedVt);
    send(0xAE, 0, 0, 1, 0);
    GetVentPatientSettings()->useHostSettings = 1; protocolApplyReceivedSettings();
    assert(GetVentVacSettings()->tidalVolume == 700);
    /* Bad CRC must not alter a valid cached value. */
    length = frame(bytes, 0xAF, 14, 800, 2, 0); bytes[length-1] ^= 1;
    feed(bytes, length); assert(ProtocolProcessRxData(0) == PROTOCOL_CRC_ERROR);
    assert(ProtocolGetRxVentParamsCache()->m_tidalVolume == 700);
    ProtocolProcessInit(0); /* Clear transport only. */
    /* CRC-valid malformed payload must fail atomically before callback/ACK. */
    length = frame(bytes, 0xAF, 14, 800, 2, 0); bytes[6] = ProtocolGetSubIdLength(3, true, 0);
    uint16_t crc = Crc16Compute(bytes + 2, length - 4); bytes[length-2] = crc; bytes[length-1] = crc >> 8;
    feed(bytes, length); assert(ProtocolProcessRxData(0) == PROTOCOL_ERROR);
    assert(ProtocolGetRxVentParamsCache()->m_tidalVolume == 700);
    assert(ProtocolParsePacket(bytes, 4, &packet) != PROTOCOL_OK);
    assert(ProtocolParsePacket(bytes, length-1, &packet) != PROTOCOL_OK);
    /* Valid requested ACK echoes the exact received frame. */
    length = frame(bytes, 0xAF, 14, 750, 2, 0); bytes[3] = 1;
    crc = Crc16Compute(bytes + 2, length - 4); bytes[length-2] = crc; bytes[length-1] = crc >> 8;
    feed(bytes, length); assert(ProtocolProcessRxData(0) == PROTOCOL_OK);
    ProtocolSchedulerProcess(0); assert(gTxSize == length && memcmp(bytes, gTx, length) == 0);
    testAlarmBitfields();
    testPhysAlarms();
    testTechAlarms();
    testMeanPressure();
    testMinuteLeak();
    testComplianceDynamic();
    testComplianceStatic();
    testResistanceInspiratory();
    testResistanceExpiratory();
    testLeakPercent();
    testHeartbeat();
    return 0;
}
/**************************End of file********************************/

'''

def main():
    compiler = os.environ.get("CC") or shutil.which("gcc") or shutil.which("clang")
    if not compiler:
        compiler = next((str(path) for path in (
            Path("C:/msys64/mingw64/bin/gcc.exe"),
            Path("C:/Qt/Tools/mingw1310_64/bin/gcc.exe"),
        ) if path.is_file()), None)
    if not compiler:
        raise SystemExit("Set CC to a native GCC or Clang compiler for this host test.")
    with tempfile.TemporaryDirectory(prefix="ventcore-protocol-") as directory:
        harness = Path(directory) / "protocol_test.c"
        harness.write_text(HARNESS, encoding="utf-8", newline="\n")
        executable = Path(directory) / "protocol_test.exe"
        includes = ["user/app/protocol", "user/bsp/uart", "user/app/databus", "user/app/ventlogic",
                    "user/app/ventalgo", "user/module/log", "user/module/rtos", "user/tools/ringbuffer",
                    "user/tools/controller", "user/app/physalarm", "user/app/techalarm"]
        sources = ["user/app/protocol/ProtoclOfMcm.c", "user/app/protocol/ProtoclOfTrasn.c", "user/app/protocol/ProtoclOfPackets.c",
                   "user/app/protocol/ProtoclOfProcess.c", "user/app/databus/settingdata.c",
                   "user/tools/ringbuffer/ringbuffer.c"]
        command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
                   *[f"-I{ROOT / path}" for path in includes], str(harness),
                   *[str(ROOT / path) for path in sources], "-o", str(executable)]
        environment = os.environ.copy()
        environment["PATH"] = str(Path(compiler).parent) + os.pathsep + environment["PATH"]
        subprocess.run(command, check=True, env=environment)
        subprocess.run([str(executable)], check=True, env=environment)
    print("PASS: MCM alarm limits in local/host settings, partial updates and CRC rejection, physiological alarm wire bits/recovery/500ms full reports, fragmented RX, CRC rejection, parameter/alarm caches, scaling, source switching, start/stop, waveform, ACK, 1110 heartbeat replies, bursts, TX backpressure and reconnect")

if __name__ == "__main__":
    main()
