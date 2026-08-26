/************************************************************************************
* @file     : actuatorcontroller.h
* @brief    : Breath actuator arbitration interface.
* @details  : Selects, overrides, and commits unified controller requests.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTLOGIC_ACTUATORCONTROLLER_H
#define USER_APP_VENTLOGIC_ACTUATORCONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

#include "actuatorrequest.h"
#include "blower_vcm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ACTUATOR_CONTROLLER_RELIEF_OPEN_DUTY     0U
#define ACTUATOR_CONTROLLER_RELIEF_CLOSED_DUTY   100U
#define ACTUATOR_CONTROLLER_EXP_SAFE_DUTY        0U
#define ACTUATOR_CONTROLLER_OXYGEN_SAFE_DUTY     0U

/** Initialize controller arbitration and all actuator-producing controllers. */
void actuatorControllerInit(void);

/** Select and commit one unified actuator request. */
void actuatorControllerProcess(void);

/** Copy the request most recently committed after arbitration. */
int8_t actuatorControllerLastRequestGet(stActuatorRequest *request);

/** Override the automatic blower request until manual control is cleared. */
void actuatorControllerBlowerManualSet(eBlowerVcmControlMode mode,
                                       uint16_t targetValue,
                                       uint8_t vcmSaturation);

/** Return blower ownership to automatic control. */
void actuatorControllerBlowerManualClear(void);

/** Return true while the blower is under manual debug control. */
bool actuatorControllerBlowerManualIsActive(void);

/** Override the automatic expiratory-valve request until manual control is cleared. */
void actuatorControllerExpValveManualSet(uint8_t dutyPercent);

/** Return expiratory-valve ownership to automatic control. */
void actuatorControllerExpValveManualClear(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTLOGIC_ACTUATORCONTROLLER_H */
/**************************End of file********************************/
