/************************************************************************************
* @file     : physalarmvent.h
* @brief    : Ventilation physiological alarm detector interface.
* @details  : Declares ventilation detector functions registered by the manager.
* @author   :
* @date     : 2026-09-07
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_PHYSALARM_PHYSALARMVENT_H
#define USER_APP_PHYSALARM_PHYSALARMVENT_H

#include <stdbool.h>
#include <stdint.h>

#include "physalarmmanager.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PHYS_ALARM_PRESSURE_HIGH_CONFIRM_MS       20U
#define PHYS_ALARM_PRESSURE_LOW_CONFIRM_BREATHS    3U
#define PHYS_ALARM_VTE_CONFIRM_BREATHS              4U
#define PHYS_ALARM_VTE_HIGH_IMMEDIATE_RATIO          1.5F
#define PHYS_ALARM_PEEP_HIGH_OFFSET_CMH2O            5.0F
#define PHYS_ALARM_PEEP_LOW_OFFSET_CMH2O             3.0F
#define PHYS_ALARM_PEEP_RECOVERY_MS                200U

typedef struct stPhysAlarmVentRuntime {
    uint32_t referenceMs;
    uint32_t processedSequence;
    uint8_t consecutiveBreaths;
    bool sequenceInitialized;
    bool timing;
    bool active;
} stPhysAlarmVentRuntime;

/** Initialize all ventilation alarm detectors. */
void physAlarmVentInit(void);

/** Detect sustained patient pressure above Pmax. */
bool physAlarmVentAirwayPressureHighDetect(uint32_t nowMs);

/** Detect completed breaths whose Ppeak or Pplat is below Pmin. */
bool physAlarmVentAirwayPressureLowDetect(uint32_t nowMs);

/** Detect completed breaths whose Vte is above VteMax. */
bool physAlarmVentExhaledVolumeHighDetect(uint32_t nowMs);

/** Detect completed breaths whose Vte is below VteMin. */
bool physAlarmVentExhaledVolumeLowDetect(uint32_t nowMs);

/** Detect high completed-cycle PEEP at inspiration, with timed recovery. */
bool physAlarmVentPeepHighDetect(uint32_t nowMs);

/** Detect low completed-cycle PEEP at inspiration, with timed recovery. */
bool physAlarmVentPeepLowDetect(uint32_t nowMs);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_PHYSALARM_PHYSALARMVENT_H */
/*************************************** End of file ********************************/
