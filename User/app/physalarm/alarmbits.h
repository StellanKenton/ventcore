/************************************************************************************
* @file     : alarmbits.h
* @brief    : MCM physiological and technical alarm bitmaps.
* @details  : Current alarm states using the legacy MCM wire mapping.
* @author   :
* @date     : 2026-09-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_PHYSALARM_ALARMBITS_H
#define USER_APP_PHYSALARM_ALARMBITS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Wire bit numbers are independent of the detector registration order.
 * Bitfields follow the target ARM GCC little-endian ABI; serialize value explicitly.
 */
#define MCM_ALARM_REPORT_PERIOD_MS 500U
#define MCM_PHYS_ALARM_PAYLOAD_SIZE 4U

typedef enum {
    TECH_ALARM_PHYS_MODULE_ID = 0,
    TECH_ALARM_TECH_MODULE_ID = 1,
    TECH_ALARM_POWER_MODULE_ID = 2,
    TECH_ALARM_COMM_MODULE_ID = 3,
    TECH_ALARM_CAL_MODULE_ID = 4,
    TECH_ALARM_MAX_MODULES = 5
} eMcmTechAlarmModule;

/* 0xAD direct payload. */
typedef enum {
    AIRWAY_PRESSURE_HIGH = 0,
    AIRWAY_PRESSURE_LOW = 1,
    FIO2_HIGH = 2,
    FIO2_LOW = 3,
    EXPIRATORY_TIDAL_VOLUME_HIGH = 4,
    EXPIRATORY_TIDAL_VOLUME_LOW = 5,
    EXPIRATORY_MINUTE_VENTILATION_HIGH = 6,
    EXPIRATORY_MINUTE_VENTILATION_LOW = 7,
    APNEA_ALARM = 8,
    APNEA_VENTILATION_ALARM = 9,
    APNEA_VENTILATION_END = 10,
    RESPIRATORY_RATE_HIGH = 11,
    RESPIRATORY_RATE_LOW = 12,
    PHYSALARM_RESERVE1 = 13,
    PHYSALARM_RESERVE2 = 14,
    INVERSE_VENTILATION_ALARM = 15,
} eMcmPhysAlarmBit;

typedef union {
    uint32_t value;
    uint8_t bytes[4];
    struct {
        uint32_t airwayPressureHigh : 1; /* Bit 0. */
        uint32_t airwayPressureLow : 1; /* Bit 1. */
        uint32_t fio2High : 1; /* Bit 2. */
        uint32_t fio2Low : 1; /* Bit 3. */
        uint32_t expiratoryTidalVolumeHigh : 1; /* Bit 4. */
        uint32_t expiratoryTidalVolumeLow : 1; /* Bit 5. */
        uint32_t expiratoryMinuteVentilationHigh : 1; /* Bit 6. */
        uint32_t expiratoryMinuteVentilationLow : 1; /* Bit 7. */
        uint32_t apneaAlarm : 1; /* Bit 8. */
        uint32_t apneaVentilationAlarm : 1; /* Bit 9. */
        uint32_t apneaVentilationEnd : 1; /* Bit 10. */
        uint32_t respiratoryRateHigh : 1; /* Bit 11. */
        uint32_t respiratoryRateLow : 1; /* Bit 12. */
        uint32_t physalarmReserve1 : 1; /* Bit 13. */
        uint32_t physalarmReserve2 : 1; /* Bit 14. */
        uint32_t inverseVentilationAlarm : 1; /* Bit 15. */
        uint32_t reserved : 16;
    } bits;
} stMcmPhysAlarmStatus;

/* 0xAB / SubId 0. */
typedef enum {
    PHYSIO_FAULT_PEEP_TOO_HIGH = 0,
    PHYSIO_FAULT_PEEP_TOO_LOW = 1,
    PHYSIO_FAULT_PIPELINE_BLOCKAGE = 2,
    PHYSIO_FAULT_INSP_BRANCH_BLOCKAGE = 3,
    PHYSIO_FAULT_CPAP_TOO_HIGH = 4,
    PHYSIO_FAULT_PIPELINE_LEAK = 5,
    PHYSIO_FAULT_PIPELINE_DISCONNECT = 6,
    PHYSIO_FAULT_PRESSURE_LIMIT = 7,
    PHYSIO_FAULT_VOLUME_LIMIT = 8,
    PHYSIO_FAULT_INSP_PRESS_NOT_REACHED = 9,
    PHYSIO_FAULT_TIDAL_VOL_NOT_REACHED = 10,
    PHYSIO_FAULT_SIGH_CYCLE_PRESS_LIMIT = 11,
    PHYSIO_FAULT_RESERVED = 12,
    PHYSIO_FAULT_INSP_TIME_TOO_LONG = 13,
    PHYSIO_FAULT_INHALED_GAS_TEMP_HIGH = 14,
    PHYSIO_FAULT_AMV_TARGET_NOT_REACHED = 15,
    PHYSIO_FAULT_O2_FLOW_NOT_REACHED = 16,
    PHYSIO_FAULT_PAT_FLOW_SENSOR_FAULT = 17,
    PHYSIO_FAULT_PAT_PRESS_SENSOR_FAULT = 18,
    PHYSIO_FAULT_MECH_PIPELINE_DISCONNECT = 19,
    PHYSIO_FAULT_EXP_BRANCH_BLOCKAGE = 20,
    PHYSIO_FAULT_MAX_INSP_NEG_PRESSURE = 21,
    PHYSIO_FAULT_INSP_PRESSURE_NOT_RELEASED = 22,
    PHYSIO_FAULT_O2_SOURCE_FAILURE = 23,
    PHYSIO_FAULT_PROXIMAL_PRESS_TUBE_DISCONNECT = 24,
} eMcmTechPhysAlarmBit;

typedef union {
    uint32_t value;
    uint8_t bytes[4];
    struct {
        uint32_t physioFaultPeepTooHigh : 1; /* Bit 0. */
        uint32_t physioFaultPeepTooLow : 1; /* Bit 1. */
        uint32_t physioFaultPipelineBlockage : 1; /* Bit 2. */
        uint32_t physioFaultInspBranchBlockage : 1; /* Bit 3. */
        uint32_t physioFaultCpapTooHigh : 1; /* Bit 4. */
        uint32_t physioFaultPipelineLeak : 1; /* Bit 5. */
        uint32_t physioFaultPipelineDisconnect : 1; /* Bit 6. */
        uint32_t physioFaultPressureLimit : 1; /* Bit 7. */
        uint32_t physioFaultVolumeLimit : 1; /* Bit 8. */
        uint32_t physioFaultInspPressNotReached : 1; /* Bit 9. */
        uint32_t physioFaultTidalVolNotReached : 1; /* Bit 10. */
        uint32_t physioFaultSighCyclePressLimit : 1; /* Bit 11. */
        uint32_t physioFaultReserved : 1; /* Bit 12. */
        uint32_t physioFaultInspTimeTooLong : 1; /* Bit 13. */
        uint32_t physioFaultInhaledGasTempHigh : 1; /* Bit 14. */
        uint32_t physioFaultAmvTargetNotReached : 1; /* Bit 15. */
        uint32_t physioFaultO2FlowNotReached : 1; /* Bit 16. */
        uint32_t physioFaultPatFlowSensorFault : 1; /* Bit 17. */
        uint32_t physioFaultPatPressSensorFault : 1; /* Bit 18. */
        uint32_t physioFaultMechPipelineDisconnect : 1; /* Bit 19. */
        uint32_t physioFaultExpBranchBlockage : 1; /* Bit 20. */
        uint32_t physioFaultMaxInspNegPressure : 1; /* Bit 21. */
        uint32_t physioFaultInspPressureNotReleased : 1; /* Bit 22. */
        uint32_t physioFaultO2SourceFailure : 1; /* Bit 23. */
        uint32_t physioFaultProximalPressTubeDisconnect : 1; /* Bit 24. */
        uint32_t reserved : 7;
    } bits;
} stMcmTechPhysAlarmStatus;

/* 0xAB / SubId 1. */
typedef enum {
    TECH_FAULT_INSP_PRESS_SENSOR = 0,
    TECH_FAULT_EXP_PRESS_SENSOR = 1,
    TECH_FAULT_PROXIMAL_PRESS_SENSOR = 2,
    TECH_FAULT_INSP_FLOW_SENSOR = 3,
    TECH_FAULT_O2_FLOW_SENSOR = 4,
    TECH_FAULT_AIR_FLOW_SENSOR_TYPE_ERR = 5,
    TECH_FAULT_TRI_O2_FLOW_SENSOR_TYPE_ERR = 6,
    TECH_FAULT_PROXIMAL_FLOW_SENSOR_DISCONNECT = 7,
    TECH_FAULT_PROXIMAL_FLOW_SENSOR_TYPE_ERR = 8,
    TECH_FAULT_PROXIMAL_FLOW_SENSOR_REVERSED = 9,
    TECH_FAULT_TURBINE_TEMP_SENSOR = 10,
    TECH_FAULT_TURBINE_HALL_SIGNAL_ERR = 11,
    TECH_FAULT_NEGATIVE_PRESS_SENSOR = 12,
    TECH_FAULT_O2_SENSOR = 13,
    TECH_FAULT_ATMOSPHERIC_PRESS_SENSOR = 14,
    TECH_FAULT_PRESS_SENSOR_ZERO_ERROR = 15,
    TECH_FAULT_SAFETY_VALVE = 16,
    TECH_FAULT_THREE_WAY_VALVE = 17,
    TECH_FAULT_TOTAL_INSP_MANIFOLD = 18,
    TECH_FAULT_O2_BRANCH_DISCONNECT = 19,
    TECH_FAULT_POWER_CAP_DISCONNECT = 20,
    TECH_FAULT_PEEP_VALVE = 21,
    TECH_FAULT_TURBINE_SHAFT = 22,
    TECH_FAULT_TURBINE_TEMP_HIGH = 23,
    TECH_FAULT_TURBINE_TEMP_OVERHIGH = 24,
    TECH_FAULT_HEPA_FILTER_MISSING = 25,
    TECH_FAULT_REPLACE_HEPA_FILTER = 26,
    TECH_FAULT_ATMOSPHERIC_COMM_ERR = 27,
    TECH_FAULT_HEPA_FILTER_PRESSURE_SENSOR = 28,
    TECH_FAULT_MEMORY_ERROR = 29,
    TECH_FAULT_O2_SOURCE_LOW = 30,
} eMcmTechAlarmBit;

typedef union {
    uint32_t value;
    uint8_t bytes[4];
    struct {
        uint32_t techFaultInspPressSensor : 1; /* Bit 0. */
        uint32_t techFaultExpPressSensor : 1; /* Bit 1. */
        uint32_t techFaultProximalPressSensor : 1; /* Bit 2. */
        uint32_t techFaultInspFlowSensor : 1; /* Bit 3. */
        uint32_t techFaultO2FlowSensor : 1; /* Bit 4. */
        uint32_t techFaultAirFlowSensorTypeErr : 1; /* Bit 5. */
        uint32_t techFaultTriO2FlowSensorTypeErr : 1; /* Bit 6. */
        uint32_t techFaultProximalFlowSensorDisconnect : 1; /* Bit 7. */
        uint32_t techFaultProximalFlowSensorTypeErr : 1; /* Bit 8. */
        uint32_t techFaultProximalFlowSensorReversed : 1; /* Bit 9. */
        uint32_t techFaultTurbineTempSensor : 1; /* Bit 10. */
        uint32_t techFaultTurbineHallSignalErr : 1; /* Bit 11. */
        uint32_t techFaultNegativePressSensor : 1; /* Bit 12. */
        uint32_t techFaultO2Sensor : 1; /* Bit 13. */
        uint32_t techFaultAtmosphericPressSensor : 1; /* Bit 14. */
        uint32_t techFaultPressSensorZeroError : 1; /* Bit 15. */
        uint32_t techFaultSafetyValve : 1; /* Bit 16. */
        uint32_t techFaultThreeWayValve : 1; /* Bit 17. */
        uint32_t techFaultTotalInspManifold : 1; /* Bit 18. */
        uint32_t techFaultO2BranchDisconnect : 1; /* Bit 19. */
        uint32_t techFaultPowerCapDisconnect : 1; /* Bit 20. */
        uint32_t techFaultPeepValve : 1; /* Bit 21. */
        uint32_t techFaultTurbineShaft : 1; /* Bit 22. */
        uint32_t techFaultTurbineTempHigh : 1; /* Bit 23. */
        uint32_t techFaultTurbineTempOverhigh : 1; /* Bit 24. */
        uint32_t techFaultHepaFilterMissing : 1; /* Bit 25. */
        uint32_t techFaultReplaceHepaFilter : 1; /* Bit 26. */
        uint32_t techFaultAtmosphericCommErr : 1; /* Bit 27. */
        uint32_t techFaultHepaFilterPressureSensor : 1; /* Bit 28. */
        uint32_t techFaultMemoryError : 1; /* Bit 29. */
        uint32_t techFaultO2SourceLow : 1; /* Bit 30. */
        uint32_t reserved : 1;
    } bits;
} stMcmTechAlarmStatus;

/* 0xAB / SubId 2. */
typedef enum {
    POWER_FAULT_PCM_3V3 = 0,
    POWER_FAULT_VDD_24V = 1,
    POWER_FAULT_AVDD_5V = 2,
} eMcmPowerAlarmBit;

typedef union {
    uint8_t value;
    uint8_t bytes[1];
    struct {
        uint8_t powerFaultPcm3V3 : 1; /* Bit 0. */
        uint8_t powerFaultVdd24V : 1; /* Bit 1. */
        uint8_t powerFaultAvdd5V : 1; /* Bit 2. */
        uint8_t reserved : 5;
    } bits;
} stMcmPowerAlarmStatus;

/* 0xAB / SubId 3. */
typedef enum {
    COMM_FAULT_MOTOR_DISCONNECT = 0,
    COMM_FAULT_PCM_DISCONNECT = 1,
} eMcmCommAlarmBit;

typedef union {
    uint8_t value;
    uint8_t bytes[1];
    struct {
        uint8_t commFaultMotorDisconnect : 1; /* Bit 0. */
        uint8_t commFaultPcmDisconnect : 1; /* Bit 1. */
        uint8_t reserved : 6;
    } bits;
} stMcmCommAlarmStatus;

/* 0xAB / SubId 4. */
typedef enum {
    TECH_ALARM_CAL_PRESSURE_SENSOR = 0,
    TECH_ALARM_CAL_OXYGEN_SENSOR = 1,
    TECH_ALARM_CAL_AIR_OXYGEN_RATIO = 2,
    TECH_ALARM_CAL_OXYGEN_RATIO_VALVE = 3,
    TECH_ALARM_CAL_EXHALATION_VALVE = 4,
    TECH_ALARM_CAL_PROXIMAL_FLOW_SENSOR = 5,
    TECH_ALARM_CAL_GAS_SOURCE_PRESSURE_SENSOR = 6,
} eMcmCalAlarmBit;

typedef union {
    uint8_t value;
    uint8_t bytes[1];
    struct {
        uint8_t techAlarmCalPressureSensor : 1; /* Bit 0. */
        uint8_t techAlarmCalOxygenSensor : 1; /* Bit 1. */
        uint8_t techAlarmCalAirOxygenRatio : 1; /* Bit 2. */
        uint8_t techAlarmCalOxygenRatioValve : 1; /* Bit 3. */
        uint8_t techAlarmCalExhalationValve : 1; /* Bit 4. */
        uint8_t techAlarmCalProximalFlowSensor : 1; /* Bit 5. */
        uint8_t techAlarmCalGasSourcePressureSensor : 1; /* Bit 6. */
        uint8_t reserved : 1;
    } bits;
} stMcmCalAlarmStatus;

typedef struct {
    stMcmTechPhysAlarmStatus phys;
    stMcmTechAlarmStatus tech;
    stMcmPowerAlarmStatus power;
    stMcmCommAlarmStatus comm;
    stMcmCalAlarmStatus cal;
} stMcmTechAlarmStatusSnapshot;

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_PHYSALARM_ALARMBITS_H */
/*************************************** End of file ********************************/
