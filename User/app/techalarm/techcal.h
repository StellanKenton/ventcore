/************************************************************************************
* @file     : techcal.h
* @brief    : Technical alarm interface; task context only.
* @details  : Current alarm states using the legacy MCM wire mapping.
* @author   :
* @date     : 2026-09-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_TECHALARM_TECHCAL_H
#define USER_APP_TECHALARM_TECHCAL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Placeholder: TECH_ALARM_CAL_PRESSURE_SENSOR. */
bool techCalPressureSensorDetect(uint32_t nowMs);

/** Placeholder: TECH_ALARM_CAL_OXYGEN_SENSOR. */
bool techCalOxygenSensorDetect(uint32_t nowMs);

/** Placeholder: TECH_ALARM_CAL_AIR_OXYGEN_RATIO. */
bool techCalAirOxygenRatioDetect(uint32_t nowMs);

/** Placeholder: TECH_ALARM_CAL_OXYGEN_RATIO_VALVE. */
bool techCalOxygenRatioValveDetect(uint32_t nowMs);

/** Placeholder: TECH_ALARM_CAL_EXHALATION_VALVE. */
bool techCalExhalationValveDetect(uint32_t nowMs);

/** Placeholder: TECH_ALARM_CAL_PROXIMAL_FLOW_SENSOR. */
bool techCalProximalFlowSensorDetect(uint32_t nowMs);

/** Placeholder: TECH_ALARM_CAL_GAS_SOURCE_PRESSURE_SENSOR. */
bool techCalGasSourcePressureSensorDetect(uint32_t nowMs);


#ifdef __cplusplus
}
#endif

#endif /* USER_APP_TECHALARM_TECHCAL_H */
/*************************************** End of file ********************************/
