/************************************************************************************
* @file     : physalarmmanager.h
* @brief    : Physiological alarm manager interface.
* @details  : Declares registered alarm types, runtime state, and processing APIs.
* @author   :
* @date     : 2026-09-07
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_PHYSALARM_PHYSALARMMANAGER_H
#define USER_APP_PHYSALARM_PHYSALARMMANAGER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PHYS_ALARM_AIRWAY_PRESSURE_HIGH = 0,
    PHYS_ALARM_AIRWAY_PRESSURE_LOW,
    PHYS_ALARM_EXHALED_VOLUME_HIGH,
    PHYS_ALARM_EXHALED_VOLUME_LOW,

    PHYS_ALARM_PEEP_HIGH,   
    PHYS_ALARM_PEEP_LOW,    
    PHYS_ALARM_PIPELINE_BLOCKAGE, // Patient circuit blockage - H
    PHYS_ALARM_INSP_BRANCH_BLOCKAGE, // Inspiratory branch blockage - M
    PHYS_ALARM_CPAP_TOO_HIGH, // Continuous airway pressure too high - H
    PHYS_ALARM_PIPELINE_LEAK, // Circuit leak - L
    PHYS_ALARM_PIPELINE_DISCONNECT, // Patient circuit disconnection - H
    PHYS_ALARM_PRESSURE_LIMIT, // Pressure limitation - L
    PHYS_ALARM_VOLUME_LIMIT, // Volume limitation - L
    PHYS_ALARM_INSP_PRESS_NOT_REACHED, // Inspiratory pressure not reached - L
    PHYS_ALARM_TIDAL_VOL_NOT_REACHED, // Tidal volume not reached - L
    PHYS_ALARM_SIGH_CYCLE_PRESS_LIMIT, // Sigh cycle pressure limitation - L
    PHYS_ALARM_O2_SUPPLY_INSUFFICIENT, // Insufficient oxygen supply - H
    PHYS_ALARM_INSP_TIME_TOO_LONG, // Inspiratory time too long - L
    PHYS_ALARM_INHALED_GAS_TEMP_HIGH, // Inhaled gas temperature too high - H
    PHYS_ALARM_AMV_TARGET_NOT_REACHED, // AMV target not reached - L
    PHYS_ALARM_O2_FLOW_NOT_REACHED, // Oxygen therapy flow not reached - H
    PHYS_ALARM_PAT_FLOW_SENSOR_FAULT, // Patient flow sensor fault - H
    PHYS_ALARM_PAT_PRESS_SENSOR_FAULT, // Patient pressure sensor fault - H
    PHYS_ALARM_MECH_PIPELINE_DISCONNECT, // Machine circuit disconnection - H
    PHYS_ALARM_EXP_BRANCH_BLOCKAGE, // Expiratory branch blockage - M
    PHYS_ALARM_MAX_INSP_NEG_PRESSURE, // Maximum inspiratory negative pressure - H
    PHYS_ALARM_INSP_PRESSURE_NOT_RELEASED, // Inspiratory pressure not released - H
    PHYS_ALARM_O2_SOURCE_FAILURE, // Oxygen source failure - H
    PHYS_ALARM_PROXIMAL_PRESS_TUBE_DISCONNECT, // Proximal pressure sampling tube disconnection - H

    
    PHYS_ALARM_COUNT,
} ePhysAlarmType;

typedef bool (*pfPhysAlarmDetector)(uint32_t nowMs);

typedef struct stPhysAlarmRegistration {
    ePhysAlarmType type;
    bool enabled;
    pfPhysAlarmDetector detector;
    bool active;
} stPhysAlarmRegistration;

/** Initialize all registered physiological alarms. */
void physAlarmManagerInit(void);

/** Run every enabled physiological alarm detector. */
void physAlarmManagerProcess(uint32_t nowMs);

/** Return whether the selected physiological alarm is active. */
bool physAlarmManagerStateGet(ePhysAlarmType type);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_PHYSALARM_PHYSALARMMANAGER_H */
/*************************************** End of file ********************************/
