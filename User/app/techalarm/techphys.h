/************************************************************************************
* @file     : techphys.h
* @brief    : Technical alarm interface; task context only.
* @details  : Current alarm states using the legacy MCM wire mapping.
* @author   :
* @date     : 2026-09-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_TECHALARM_TECHPHYS_H
#define USER_APP_TECHALARM_TECHPHYS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TECH_PHYS_PEEP_HIGH_OFFSET_CMH2O            5.0F
#define TECH_PHYS_PEEP_LOW_OFFSET_CMH2O             3.0F
#define TECH_PHYS_PEEP_RECOVERY_MS                200U
#define TECH_PHYS_CPAP_HIGH_OFFSET_CMH2O           15.0F
#define TECH_PHYS_CPAP_RECOVERY_OFFSET_CMH2O       14.5F
#define TECH_PHYS_CPAP_CONFIRM_MS                 15000U
#define TECH_PHYS_CPAP_RECOVERY_MS                3000U

typedef struct {
    uint32_t referenceMs;
    uint32_t processedSequence;
    bool sequenceInitialized;
    bool timing;
    bool active;
} stTechPhysRuntime;

/** Reset detector history before AlarmTask processing. */
void techPhysInit(void);

/** Evaluate the current alarm in AlarmTask context. */
bool techPhysPeepHighDetect(uint32_t nowMs);

/** Evaluate the current alarm in AlarmTask context. */
bool techPhysPeepLowDetect(uint32_t nowMs);

/** Evaluate the current alarm in AlarmTask context. */
bool techPhysPipelineBlockageDetect(uint32_t nowMs);

/** Evaluate the current alarm in AlarmTask context. */
bool techPhysInspBranchBlockageDetect(uint32_t nowMs);

/** Evaluate the current alarm in AlarmTask context. */
bool techPhysCpapTooHighDetect(uint32_t nowMs);

/** Evaluate the current alarm in AlarmTask context. */
bool techPhysPipelineLeakDetect(uint32_t nowMs);

/** Evaluate the current alarm in AlarmTask context. */
bool techPhysPipelineDisconnectDetect(uint32_t nowMs);

/** Evaluate the current alarm in AlarmTask context. */
bool techPhysPressureLimitDetect(uint32_t nowMs);

/** Evaluate the current alarm in AlarmTask context. */
bool techPhysVolumeLimitDetect(uint32_t nowMs);

/** Evaluate the current alarm in AlarmTask context. */
bool techPhysInspPressNotReachedDetect(uint32_t nowMs);

/** Evaluate the current alarm in AlarmTask context. */
bool techPhysTidalVolNotReachedDetect(uint32_t nowMs);

/** Evaluate the current alarm in AlarmTask context. */
bool techPhysSighCyclePressLimitDetect(uint32_t nowMs);

/** Evaluate the current alarm in AlarmTask context. */
bool techPhysInspTimeTooLongDetect(uint32_t nowMs);

/** Evaluate the current alarm in AlarmTask context. */
bool techPhysInhaledGasTempHighDetect(uint32_t nowMs);

/** Evaluate the current alarm in AlarmTask context. */
bool techPhysAmvTargetNotReachedDetect(uint32_t nowMs);

/** Evaluate the current alarm in AlarmTask context. */
bool techPhysO2FlowNotReachedDetect(uint32_t nowMs);

/** Evaluate the current alarm in AlarmTask context. */
bool techPhysPatFlowSensorFaultDetect(uint32_t nowMs);

/** Evaluate the current alarm in AlarmTask context. */
bool techPhysPatPressSensorFaultDetect(uint32_t nowMs);

/** Evaluate the current alarm in AlarmTask context. */
bool techPhysMechPipelineDisconnectDetect(uint32_t nowMs);

/** Evaluate the current alarm in AlarmTask context. */
bool techPhysExpBranchBlockageDetect(uint32_t nowMs);

/** Evaluate the current alarm in AlarmTask context. */
bool techPhysMaxInspNegPressureDetect(uint32_t nowMs);

/** Evaluate the current alarm in AlarmTask context. */
bool techPhysInspPressureNotReleasedDetect(uint32_t nowMs);

/** Evaluate the current alarm in AlarmTask context. */
bool techPhysO2SourceFailureDetect(uint32_t nowMs);

/** Evaluate the current alarm in AlarmTask context. */
bool techPhysProximalPressTubeDisconnectDetect(uint32_t nowMs);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_TECHALARM_TECHPHYS_H */
/*************************************** End of file ********************************/
