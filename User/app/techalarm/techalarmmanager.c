/************************************************************************************
* @file     : techalarmmanager.c
* @brief    : Technical alarm dispatch, state publication and MCM wire mapping.
* @details  : Current alarm states using the legacy MCM wire mapping.
* @author   :
* @date     : 2026-09-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "techalarmmanager.h"

#include <stddef.h>
#include <string.h>
#include "rtos.h"
#include "techphys.h"
#include "techdevice.h"
#include "techpower.h"
#include "techcomm.h"
#include "techcal.h"

/* Enable only implemented detectors; wire mapping is independent of enum order. */
static stTechAlarmRegistration gTechAlarmRegistrations[TECH_ALARM_COUNT] = {
    [TECH_ALARM_PEEP_HIGH] = {
        .enabled = true,
        .detector = techPhysPeepHighDetect,
        .module = TECH_ALARM_PHYS_MODULE_ID,
        .bit = PHYSIO_FAULT_PEEP_TOO_HIGH,
        .active = false,
    },
    [TECH_ALARM_PEEP_LOW] = {
        .enabled = true,
        .detector = techPhysPeepLowDetect,
        .module = TECH_ALARM_PHYS_MODULE_ID,
        .bit = PHYSIO_FAULT_PEEP_TOO_LOW,
        .active = false,
    },
    [TECH_ALARM_PIPELINE_BLOCKAGE] = {
        .enabled = false,
        .detector = techPhysPipelineBlockageDetect,
        .module = TECH_ALARM_PHYS_MODULE_ID,
        .bit = PHYSIO_FAULT_PIPELINE_BLOCKAGE,
        .active = false,
    },
    [TECH_ALARM_INSP_BRANCH_BLOCKAGE] = {
        .enabled = false,
        .detector = techPhysInspBranchBlockageDetect,
        .module = TECH_ALARM_PHYS_MODULE_ID,
        .bit = PHYSIO_FAULT_INSP_BRANCH_BLOCKAGE,
        .active = false,
    },
    [TECH_ALARM_CPAP_TOO_HIGH] = {
        .enabled = true,
        .detector = techPhysCpapTooHighDetect,
        .module = TECH_ALARM_PHYS_MODULE_ID,
        .bit = PHYSIO_FAULT_CPAP_TOO_HIGH,
        .active = false,
    },
    [TECH_ALARM_PIPELINE_LEAK] = {
        .enabled = false,
        .detector = techPhysPipelineLeakDetect,
        .module = TECH_ALARM_PHYS_MODULE_ID,
        .bit = PHYSIO_FAULT_PIPELINE_LEAK,
        .active = false,
    },
    [TECH_ALARM_PIPELINE_DISCONNECT] = {
        .enabled = false,
        .detector = techPhysPipelineDisconnectDetect,
        .module = TECH_ALARM_PHYS_MODULE_ID,
        .bit = PHYSIO_FAULT_PIPELINE_DISCONNECT,
        .active = false,
    },
    [TECH_ALARM_PRESSURE_LIMIT] = {
        .enabled = false,
        .detector = techPhysPressureLimitDetect,
        .module = TECH_ALARM_PHYS_MODULE_ID,
        .bit = PHYSIO_FAULT_PRESSURE_LIMIT,
        .active = false,
    },
    [TECH_ALARM_VOLUME_LIMIT] = {
        .enabled = false,
        .detector = techPhysVolumeLimitDetect,
        .module = TECH_ALARM_PHYS_MODULE_ID,
        .bit = PHYSIO_FAULT_VOLUME_LIMIT,
        .active = false,
    },
    [TECH_ALARM_INSP_PRESS_NOT_REACHED] = {
        .enabled = false,
        .detector = techPhysInspPressNotReachedDetect,
        .module = TECH_ALARM_PHYS_MODULE_ID,
        .bit = PHYSIO_FAULT_INSP_PRESS_NOT_REACHED,
        .active = false,
    },
    [TECH_ALARM_TIDAL_VOL_NOT_REACHED] = {
        .enabled = false,
        .detector = techPhysTidalVolNotReachedDetect,
        .module = TECH_ALARM_PHYS_MODULE_ID,
        .bit = PHYSIO_FAULT_TIDAL_VOL_NOT_REACHED,
        .active = false,
    },
    [TECH_ALARM_SIGH_CYCLE_PRESS_LIMIT] = {
        .enabled = false,
        .detector = techPhysSighCyclePressLimitDetect,
        .module = TECH_ALARM_PHYS_MODULE_ID,
        .bit = PHYSIO_FAULT_SIGH_CYCLE_PRESS_LIMIT,
        .active = false,
    },
    [TECH_ALARM_O2_SUPPLY_INSUFFICIENT] = {
        .enabled = false,
        .detector = techDeviceO2SupplyInsufficientDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_O2_SOURCE_LOW,
        .active = false,
    },
    [TECH_ALARM_INSP_TIME_TOO_LONG] = {
        .enabled = false,
        .detector = techPhysInspTimeTooLongDetect,
        .module = TECH_ALARM_PHYS_MODULE_ID,
        .bit = PHYSIO_FAULT_INSP_TIME_TOO_LONG,
        .active = false,
    },
    [TECH_ALARM_INHALED_GAS_TEMP_HIGH] = {
        .enabled = false,
        .detector = techPhysInhaledGasTempHighDetect,
        .module = TECH_ALARM_PHYS_MODULE_ID,
        .bit = PHYSIO_FAULT_INHALED_GAS_TEMP_HIGH,
        .active = false,
    },
    [TECH_ALARM_AMV_TARGET_NOT_REACHED] = {
        .enabled = false,
        .detector = techPhysAmvTargetNotReachedDetect,
        .module = TECH_ALARM_PHYS_MODULE_ID,
        .bit = PHYSIO_FAULT_AMV_TARGET_NOT_REACHED,
        .active = false,
    },
    [TECH_ALARM_O2_FLOW_NOT_REACHED] = {
        .enabled = false,
        .detector = techPhysO2FlowNotReachedDetect,
        .module = TECH_ALARM_PHYS_MODULE_ID,
        .bit = PHYSIO_FAULT_O2_FLOW_NOT_REACHED,
        .active = false,
    },
    [TECH_ALARM_PAT_FLOW_SENSOR_FAULT] = {
        .enabled = false,
        .detector = techPhysPatFlowSensorFaultDetect,
        .module = TECH_ALARM_PHYS_MODULE_ID,
        .bit = PHYSIO_FAULT_PAT_FLOW_SENSOR_FAULT,
        .active = false,
    },
    [TECH_ALARM_PAT_PRESS_SENSOR_FAULT] = {
        .enabled = false,
        .detector = techPhysPatPressSensorFaultDetect,
        .module = TECH_ALARM_PHYS_MODULE_ID,
        .bit = PHYSIO_FAULT_PAT_PRESS_SENSOR_FAULT,
        .active = false,
    },
    [TECH_ALARM_MECH_PIPELINE_DISCONNECT] = {
        .enabled = false,
        .detector = techPhysMechPipelineDisconnectDetect,
        .module = TECH_ALARM_PHYS_MODULE_ID,
        .bit = PHYSIO_FAULT_MECH_PIPELINE_DISCONNECT,
        .active = false,
    },
    [TECH_ALARM_EXP_BRANCH_BLOCKAGE] = {
        .enabled = false,
        .detector = techPhysExpBranchBlockageDetect,
        .module = TECH_ALARM_PHYS_MODULE_ID,
        .bit = PHYSIO_FAULT_EXP_BRANCH_BLOCKAGE,
        .active = false,
    },
    [TECH_ALARM_MAX_INSP_NEG_PRESSURE] = {
        .enabled = false,
        .detector = techPhysMaxInspNegPressureDetect,
        .module = TECH_ALARM_PHYS_MODULE_ID,
        .bit = PHYSIO_FAULT_MAX_INSP_NEG_PRESSURE,
        .active = false,
    },
    [TECH_ALARM_INSP_PRESSURE_NOT_RELEASED] = {
        .enabled = false,
        .detector = techPhysInspPressureNotReleasedDetect,
        .module = TECH_ALARM_PHYS_MODULE_ID,
        .bit = PHYSIO_FAULT_INSP_PRESSURE_NOT_RELEASED,
        .active = false,
    },
    [TECH_ALARM_O2_SOURCE_FAILURE] = {
        .enabled = false,
        .detector = techPhysO2SourceFailureDetect,
        .module = TECH_ALARM_PHYS_MODULE_ID,
        .bit = PHYSIO_FAULT_O2_SOURCE_FAILURE,
        .active = false,
    },
    [TECH_ALARM_PROXIMAL_PRESS_TUBE_DISCONNECT] = {
        .enabled = false,
        .detector = techPhysProximalPressTubeDisconnectDetect,
        .module = TECH_ALARM_PHYS_MODULE_ID,
        .bit = PHYSIO_FAULT_PROXIMAL_PRESS_TUBE_DISCONNECT,
        .active = false,
    },
    [TECH_ALARM_DEVICE_INSP_PRESS_SENSOR] = {
        .enabled = false,
        .detector = techDeviceInspPressSensorDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_INSP_PRESS_SENSOR,
        .active = false,
    },
    [TECH_ALARM_DEVICE_EXP_PRESS_SENSOR] = {
        .enabled = false,
        .detector = techDeviceExpPressSensorDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_EXP_PRESS_SENSOR,
        .active = false,
    },
    [TECH_ALARM_DEVICE_PROXIMAL_PRESS_SENSOR] = {
        .enabled = false,
        .detector = techDeviceProximalPressSensorDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_PROXIMAL_PRESS_SENSOR,
        .active = false,
    },
    [TECH_ALARM_DEVICE_INSP_FLOW_SENSOR] = {
        .enabled = false,
        .detector = techDeviceInspFlowSensorDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_INSP_FLOW_SENSOR,
        .active = false,
    },
    [TECH_ALARM_DEVICE_O2_FLOW_SENSOR] = {
        .enabled = false,
        .detector = techDeviceO2FlowSensorDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_O2_FLOW_SENSOR,
        .active = false,
    },
    [TECH_ALARM_DEVICE_AIR_FLOW_SENSOR_TYPE_ERR] = {
        .enabled = false,
        .detector = techDeviceAirFlowSensorTypeErrDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_AIR_FLOW_SENSOR_TYPE_ERR,
        .active = false,
    },
    [TECH_ALARM_DEVICE_TRI_O2_FLOW_SENSOR_TYPE_ERR] = {
        .enabled = false,
        .detector = techDeviceTriO2FlowSensorTypeErrDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_TRI_O2_FLOW_SENSOR_TYPE_ERR,
        .active = false,
    },
    [TECH_ALARM_DEVICE_PROXIMAL_FLOW_SENSOR_DISCONNECT] = {
        .enabled = false,
        .detector = techDeviceProximalFlowSensorDisconnectDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_PROXIMAL_FLOW_SENSOR_DISCONNECT,
        .active = false,
    },
    [TECH_ALARM_DEVICE_PROXIMAL_FLOW_SENSOR_TYPE_ERR] = {
        .enabled = false,
        .detector = techDeviceProximalFlowSensorTypeErrDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_PROXIMAL_FLOW_SENSOR_TYPE_ERR,
        .active = false,
    },
    [TECH_ALARM_DEVICE_PROXIMAL_FLOW_SENSOR_REVERSED] = {
        .enabled = false,
        .detector = techDeviceProximalFlowSensorReversedDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_PROXIMAL_FLOW_SENSOR_REVERSED,
        .active = false,
    },
    [TECH_ALARM_DEVICE_TURBINE_TEMP_SENSOR] = {
        .enabled = false,
        .detector = techDeviceTurbineTempSensorDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_TURBINE_TEMP_SENSOR,
        .active = false,
    },
    [TECH_ALARM_DEVICE_TURBINE_HALL_SIGNAL_ERR] = {
        .enabled = false,
        .detector = techDeviceTurbineHallSignalErrDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_TURBINE_HALL_SIGNAL_ERR,
        .active = false,
    },
    [TECH_ALARM_DEVICE_NEGATIVE_PRESS_SENSOR] = {
        .enabled = false,
        .detector = techDeviceNegativePressSensorDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_NEGATIVE_PRESS_SENSOR,
        .active = false,
    },
    [TECH_ALARM_DEVICE_O2_SENSOR] = {
        .enabled = false,
        .detector = techDeviceO2SensorDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_O2_SENSOR,
        .active = false,
    },
    [TECH_ALARM_DEVICE_ATMOSPHERIC_PRESS_SENSOR] = {
        .enabled = false,
        .detector = techDeviceAtmosphericPressSensorDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_ATMOSPHERIC_PRESS_SENSOR,
        .active = false,
    },
    [TECH_ALARM_DEVICE_PRESS_SENSOR_ZERO_ERROR] = {
        .enabled = false,
        .detector = techDevicePressSensorZeroErrorDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_PRESS_SENSOR_ZERO_ERROR,
        .active = false,
    },
    [TECH_ALARM_DEVICE_SAFETY_VALVE] = {
        .enabled = false,
        .detector = techDeviceSafetyValveDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_SAFETY_VALVE,
        .active = false,
    },
    [TECH_ALARM_DEVICE_THREE_WAY_VALVE] = {
        .enabled = false,
        .detector = techDeviceThreeWayValveDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_THREE_WAY_VALVE,
        .active = false,
    },
    [TECH_ALARM_DEVICE_TOTAL_INSP_MANIFOLD] = {
        .enabled = false,
        .detector = techDeviceTotalInspManifoldDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_TOTAL_INSP_MANIFOLD,
        .active = false,
    },
    [TECH_ALARM_DEVICE_O2_BRANCH_DISCONNECT] = {
        .enabled = false,
        .detector = techDeviceO2BranchDisconnectDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_O2_BRANCH_DISCONNECT,
        .active = false,
    },
    [TECH_ALARM_DEVICE_POWER_CAP_DISCONNECT] = {
        .enabled = false,
        .detector = techDevicePowerCapDisconnectDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_POWER_CAP_DISCONNECT,
        .active = false,
    },
    [TECH_ALARM_DEVICE_PEEP_VALVE] = {
        .enabled = false,
        .detector = techDevicePeepValveDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_PEEP_VALVE,
        .active = false,
    },
    [TECH_ALARM_DEVICE_TURBINE_SHAFT] = {
        .enabled = false,
        .detector = techDeviceTurbineShaftDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_TURBINE_SHAFT,
        .active = false,
    },
    [TECH_ALARM_DEVICE_TURBINE_TEMP_HIGH] = {
        .enabled = false,
        .detector = techDeviceTurbineTempHighDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_TURBINE_TEMP_HIGH,
        .active = false,
    },
    [TECH_ALARM_DEVICE_TURBINE_TEMP_OVERHIGH] = {
        .enabled = false,
        .detector = techDeviceTurbineTempOverhighDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_TURBINE_TEMP_OVERHIGH,
        .active = false,
    },
    [TECH_ALARM_DEVICE_HEPA_FILTER_MISSING] = {
        .enabled = false,
        .detector = techDeviceHepaFilterMissingDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_HEPA_FILTER_MISSING,
        .active = false,
    },
    [TECH_ALARM_DEVICE_REPLACE_HEPA_FILTER] = {
        .enabled = false,
        .detector = techDeviceReplaceHepaFilterDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_REPLACE_HEPA_FILTER,
        .active = false,
    },
    [TECH_ALARM_DEVICE_ATMOSPHERIC_COMM_ERR] = {
        .enabled = false,
        .detector = techDeviceAtmosphericCommErrDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_ATMOSPHERIC_COMM_ERR,
        .active = false,
    },
    [TECH_ALARM_DEVICE_HEPA_FILTER_PRESSURE_SENSOR] = {
        .enabled = false,
        .detector = techDeviceHepaFilterPressureSensorDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_HEPA_FILTER_PRESSURE_SENSOR,
        .active = false,
    },
    [TECH_ALARM_DEVICE_MEMORY_ERROR] = {
        .enabled = false,
        .detector = techDeviceMemoryErrorDetect,
        .module = TECH_ALARM_TECH_MODULE_ID,
        .bit = TECH_FAULT_MEMORY_ERROR,
        .active = false,
    },
    [TECH_ALARM_POWER_PCM_3V3] = {
        .enabled = false,
        .detector = techPowerPcm3V3Detect,
        .module = TECH_ALARM_POWER_MODULE_ID,
        .bit = POWER_FAULT_PCM_3V3,
        .active = false,
    },
    [TECH_ALARM_POWER_VDD_24V] = {
        .enabled = false,
        .detector = techPowerVdd24VDetect,
        .module = TECH_ALARM_POWER_MODULE_ID,
        .bit = POWER_FAULT_VDD_24V,
        .active = false,
    },
    [TECH_ALARM_POWER_AVDD_5V] = {
        .enabled = false,
        .detector = techPowerAvdd5VDetect,
        .module = TECH_ALARM_POWER_MODULE_ID,
        .bit = POWER_FAULT_AVDD_5V,
        .active = false,
    },
    [TECH_ALARM_COMM_MOTOR_DISCONNECT] = {
        .enabled = false,
        .detector = techCommMotorDisconnectDetect,
        .module = TECH_ALARM_COMM_MODULE_ID,
        .bit = COMM_FAULT_MOTOR_DISCONNECT,
        .active = false,
    },
    [TECH_ALARM_COMM_PCM_DISCONNECT] = {
        .enabled = false,
        .detector = techCommPcmDisconnectDetect,
        .module = TECH_ALARM_COMM_MODULE_ID,
        .bit = COMM_FAULT_PCM_DISCONNECT,
        .active = false,
    },
    [TECH_ALARM_CALIBRATION_PRESSURE_SENSOR] = {
        .enabled = false,
        .detector = techCalPressureSensorDetect,
        .module = TECH_ALARM_CAL_MODULE_ID,
        .bit = TECH_ALARM_CAL_PRESSURE_SENSOR,
        .active = false,
    },
    [TECH_ALARM_CALIBRATION_OXYGEN_SENSOR] = {
        .enabled = false,
        .detector = techCalOxygenSensorDetect,
        .module = TECH_ALARM_CAL_MODULE_ID,
        .bit = TECH_ALARM_CAL_OXYGEN_SENSOR,
        .active = false,
    },
    [TECH_ALARM_CALIBRATION_AIR_OXYGEN_RATIO] = {
        .enabled = false,
        .detector = techCalAirOxygenRatioDetect,
        .module = TECH_ALARM_CAL_MODULE_ID,
        .bit = TECH_ALARM_CAL_AIR_OXYGEN_RATIO,
        .active = false,
    },
    [TECH_ALARM_CALIBRATION_OXYGEN_RATIO_VALVE] = {
        .enabled = false,
        .detector = techCalOxygenRatioValveDetect,
        .module = TECH_ALARM_CAL_MODULE_ID,
        .bit = TECH_ALARM_CAL_OXYGEN_RATIO_VALVE,
        .active = false,
    },
    [TECH_ALARM_CALIBRATION_EXHALATION_VALVE] = {
        .enabled = false,
        .detector = techCalExhalationValveDetect,
        .module = TECH_ALARM_CAL_MODULE_ID,
        .bit = TECH_ALARM_CAL_EXHALATION_VALVE,
        .active = false,
    },
    [TECH_ALARM_CALIBRATION_PROXIMAL_FLOW_SENSOR] = {
        .enabled = false,
        .detector = techCalProximalFlowSensorDetect,
        .module = TECH_ALARM_CAL_MODULE_ID,
        .bit = TECH_ALARM_CAL_PROXIMAL_FLOW_SENSOR,
        .active = false,
    },
    [TECH_ALARM_CALIBRATION_GAS_SOURCE_PRESSURE_SENSOR] = {
        .enabled = false,
        .detector = techCalGasSourcePressureSensorDetect,
        .module = TECH_ALARM_CAL_MODULE_ID,
        .bit = TECH_ALARM_CAL_GAS_SOURCE_PRESSURE_SENSOR,
        .active = false,
    },
};

/** Clear published states and detector history before AlarmTask processing. */
void techAlarmManagerInit(void) {
    uint32_t lIndex;

    for (lIndex = 0U; lIndex < TECH_ALARM_COUNT; lIndex++) {
        gTechAlarmRegistrations[lIndex].active = false;
    }
    techPhysInit();
}

/** Run detectors outside the critical section, then publish each current state. */
void techAlarmManagerProcess(uint32_t nowMs) {
    uint32_t lIndex;

    for (lIndex = 0U; lIndex < TECH_ALARM_COUNT; lIndex++) {
        stTechAlarmRegistration *lRegistration = &gTechAlarmRegistrations[lIndex];
        bool lActive = lRegistration->enabled && (lRegistration->detector != NULL) &&
                       lRegistration->detector(nowMs);

        if (lRegistration->active == lActive) {
            continue;
        }
        repRtosEnterCritical();
        lRegistration->active = lActive;
        repRtosExitCritical();
    }
}

/** Return false for unknown alarm types. */
bool techAlarmManagerStateGet(eTechAlarmType type) {
    bool lActive;

    if ((uint32_t)type >= TECH_ALARM_COUNT) {
        return false;
    }
    repRtosEnterCritical();
    lActive = gTechAlarmRegistrations[type].active;
    repRtosExitCritical();
    return lActive;
}

/** Collect the five current wire groups without latching short-lived events. */
void techAlarmManagerSnapshotGet(stMcmTechAlarmStatusSnapshot *status) {
    uint32_t lIndex;

    if (status == NULL) {
        return;
    }
    (void)memset(status, 0, sizeof(*status));
    repRtosEnterCritical();
    for (lIndex = 0U; lIndex < TECH_ALARM_COUNT; lIndex++) {
        const stTechAlarmRegistration *lRegistration = &gTechAlarmRegistrations[lIndex];
        uint32_t lMask;

        if (!lRegistration->active) {
            continue;
        }
        lMask = 1UL << lRegistration->bit;
        switch (lRegistration->module) {
            case TECH_ALARM_PHYS_MODULE_ID: status->phys.value |= lMask; break;
            case TECH_ALARM_TECH_MODULE_ID: status->tech.value |= lMask; break;
            case TECH_ALARM_POWER_MODULE_ID: status->power.value |= (uint8_t)lMask; break;
            case TECH_ALARM_COMM_MODULE_ID: status->comm.value |= (uint8_t)lMask; break;
            case TECH_ALARM_CAL_MODULE_ID: status->cal.value |= (uint8_t)lMask; break;
            default: break;
        }
    }
    repRtosExitCritical();
}

/*************************************** End of file ********************************/
