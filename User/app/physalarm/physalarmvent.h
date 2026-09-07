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
#define PHYS_ALARM_CPAP_HIGH_OFFSET_CMH2O           15.0F
#define PHYS_ALARM_CPAP_RECOVERY_OFFSET_CMH2O       14.5F
#define PHYS_ALARM_CPAP_CONFIRM_MS                 15000U
#define PHYS_ALARM_CPAP_RECOVERY_MS                3000U

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

/** Placeholder: Patient circuit blockage - H. */
bool physAlarmVentPipelineBlockageDetect(uint32_t nowMs);

/** Placeholder: Inspiratory branch blockage - M. */
bool physAlarmVentInspBranchBlockageDetect(uint32_t nowMs);

/** Detect sustained high circuit pressures with timed hysteretic recovery. */
bool physAlarmVentCpapTooHighDetect(uint32_t nowMs);

/** Placeholder: Circuit leak - L. */
bool physAlarmVentPipelineLeakDetect(uint32_t nowMs);

/** Placeholder: Patient circuit disconnection - H. */
bool physAlarmVentPipelineDisconnectDetect(uint32_t nowMs);

/** Placeholder: Pressure limitation - L. */
bool physAlarmVentPressureLimitDetect(uint32_t nowMs);

/** Placeholder: Volume limitation - L. */
bool physAlarmVentVolumeLimitDetect(uint32_t nowMs);

/** Placeholder: Inspiratory pressure not reached - L. */
bool physAlarmVentInspPressNotReachedDetect(uint32_t nowMs);

/** Placeholder: Tidal volume not reached - L. */
bool physAlarmVentTidalVolNotReachedDetect(uint32_t nowMs);

/** Placeholder: Sigh cycle pressure limitation - L. */
bool physAlarmVentSighCyclePressLimitDetect(uint32_t nowMs);

/** Placeholder: Insufficient oxygen supply - H. */
bool physAlarmVentO2SupplyInsufficientDetect(uint32_t nowMs);

/** Placeholder: Inspiratory time too long - L. */
bool physAlarmVentInspTimeTooLongDetect(uint32_t nowMs);

/** Placeholder: Inhaled gas temperature too high - H. */
bool physAlarmVentInhaledGasTempHighDetect(uint32_t nowMs);

/** Placeholder: AMV target not reached - L. */
bool physAlarmVentAmvTargetNotReachedDetect(uint32_t nowMs);

/** Placeholder: Oxygen therapy flow not reached - H. */
bool physAlarmVentO2FlowNotReachedDetect(uint32_t nowMs);

/** Placeholder: Patient flow sensor fault - H. */
bool physAlarmVentPatFlowSensorFaultDetect(uint32_t nowMs);

/** Placeholder: Patient pressure sensor fault - H. */
bool physAlarmVentPatPressSensorFaultDetect(uint32_t nowMs);

/** Placeholder: Machine circuit disconnection - H. */
bool physAlarmVentMechPipelineDisconnectDetect(uint32_t nowMs);

/** Placeholder: Expiratory branch blockage - M. */
bool physAlarmVentExpBranchBlockageDetect(uint32_t nowMs);

/** Placeholder: Maximum inspiratory negative pressure - H. */
bool physAlarmVentMaxInspNegPressureDetect(uint32_t nowMs);

/** Placeholder: Inspiratory pressure not released - H. */
bool physAlarmVentInspPressureNotReleasedDetect(uint32_t nowMs);

/** Placeholder: Oxygen source failure - H. */
bool physAlarmVentO2SourceFailureDetect(uint32_t nowMs);

/** Placeholder: Proximal pressure sampling tube disconnection - H. */
bool physAlarmVentProximalPressTubeDisconnectDetect(uint32_t nowMs);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_PHYSALARM_PHYSALARMVENT_H */
/*************************************** End of file ********************************/
