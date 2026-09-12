/************************************************************************************
* @file     : techalarm.h
* @brief    : Technical alarm detectors and MCM snapshot interface.
* @details  : Current alarm states using the legacy MCM wire mapping.
* @author   :
* @date     : 2026-09-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_PHYSALARM_TECHALARM_H
#define USER_APP_PHYSALARM_TECHALARM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "physalarmvent.h"
#include "alarmbits.h"

#define PHYS_ALARM_PEEP_HIGH_OFFSET_CMH2O            5.0F
#define PHYS_ALARM_PEEP_LOW_OFFSET_CMH2O             3.0F
#define PHYS_ALARM_PEEP_RECOVERY_MS                200U
#define PHYS_ALARM_CPAP_HIGH_OFFSET_CMH2O           15.0F
#define PHYS_ALARM_CPAP_RECOVERY_OFFSET_CMH2O       14.5F
#define PHYS_ALARM_CPAP_CONFIRM_MS                 15000U
#define PHYS_ALARM_CPAP_RECOVERY_MS                3000U

/** Initialize technical detectors before AlarmTask processing. */
void techAlarmInit(void);

/** Read all current technical states; task context only, NULL is ignored. */
void techAlarmSnapshotGet(stMcmTechAlarmStatusSnapshot *status);

/** Detect high completed-cycle PEEP at inspiration, with timed recovery. */
bool techAlarmPeepHighDetect(uint32_t nowMs);

/** Detect low completed-cycle PEEP at inspiration, with timed recovery. */
bool techAlarmPeepLowDetect(uint32_t nowMs);

/** Placeholder: Patient circuit blockage - H. */
bool techAlarmPipelineBlockageDetect(uint32_t nowMs);

/** Placeholder: Inspiratory branch blockage - M. */
bool techAlarmInspBranchBlockageDetect(uint32_t nowMs);

/** Detect sustained high circuit pressures with timed hysteretic recovery. */
bool techAlarmCpapTooHighDetect(uint32_t nowMs);

/** Placeholder: Circuit leak - L. */
bool techAlarmPipelineLeakDetect(uint32_t nowMs);

/** Placeholder: Patient circuit disconnection - H. */
bool techAlarmPipelineDisconnectDetect(uint32_t nowMs);

/** Placeholder: Pressure limitation - L. */
bool techAlarmPressureLimitDetect(uint32_t nowMs);

/** Placeholder: Volume limitation - L. */
bool techAlarmVolumeLimitDetect(uint32_t nowMs);

/** Placeholder: Inspiratory pressure not reached - L. */
bool techAlarmInspPressNotReachedDetect(uint32_t nowMs);

/** Placeholder: Tidal volume not reached - L. */
bool techAlarmTidalVolNotReachedDetect(uint32_t nowMs);

/** Placeholder: Sigh cycle pressure limitation - L. */
bool techAlarmSighCyclePressLimitDetect(uint32_t nowMs);

/** Placeholder: Insufficient oxygen supply - H. */
bool techAlarmO2SupplyInsufficientDetect(uint32_t nowMs);

/** Placeholder: Inspiratory time too long - L. */
bool techAlarmInspTimeTooLongDetect(uint32_t nowMs);

/** Placeholder: Inhaled gas temperature too high - H. */
bool techAlarmInhaledGasTempHighDetect(uint32_t nowMs);

/** Placeholder: AMV target not reached - L. */
bool techAlarmAmvTargetNotReachedDetect(uint32_t nowMs);

/** Placeholder: Oxygen therapy flow not reached - H. */
bool techAlarmO2FlowNotReachedDetect(uint32_t nowMs);

/** Placeholder: Patient flow sensor fault - H. */
bool techAlarmPatFlowSensorFaultDetect(uint32_t nowMs);

/** Placeholder: Patient pressure sensor fault - H. */
bool techAlarmPatPressSensorFaultDetect(uint32_t nowMs);

/** Placeholder: Machine circuit disconnection - H. */
bool techAlarmMechPipelineDisconnectDetect(uint32_t nowMs);

/** Placeholder: Expiratory branch blockage - M. */
bool techAlarmExpBranchBlockageDetect(uint32_t nowMs);

/** Placeholder: Maximum inspiratory negative pressure - H. */
bool techAlarmMaxInspNegPressureDetect(uint32_t nowMs);

/** Placeholder: Inspiratory pressure not released - H. */
bool techAlarmInspPressureNotReleasedDetect(uint32_t nowMs);

/** Placeholder: Oxygen source failure - H. */
bool techAlarmO2SourceFailureDetect(uint32_t nowMs);

/** Placeholder: Proximal pressure sampling tube disconnection - H. */
bool techAlarmProximalPressTubeDisconnectDetect(uint32_t nowMs);


#ifdef __cplusplus
}
#endif

#endif /* USER_APP_PHYSALARM_TECHALARM_H */
/*************************************** End of file ********************************/
