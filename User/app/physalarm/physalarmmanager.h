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
