/************************************************************************************
* @file     : actuatorcontroller.h
* @brief    : Breath actuator controller interface.
* @details  : Declares actuator controller initialization and periodic processing.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTLOGIC_ACTUATORCONTROLLER_H
#define USER_APP_VENTLOGIC_ACTUATORCONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "blower_vcm.h"

#define ACTUATOR_CONTROLLER_RELIEF_OPEN_DUTY     0U
#define ACTUATOR_CONTROLLER_RELIEF_CLOSED_DUTY   100U

/** Initialize the actuator controller. */
void actuatorControllerInit(void);

/** Run one actuator controller processing cycle. */
void actuatorControllerProcess(void);

/** Override the automatic blower request until manual control is cleared. */
void actuatorControllerBlowerManualSet(eBlowerVcmControlMode mode,
                                       uint16_t targetValue,
                                       uint8_t vcmSaturation);

/** Return blower ownership to the automatic pressure controller. */
void actuatorControllerBlowerManualClear(void);

/** Return true while the blower is under manual debug control. */
bool actuatorControllerBlowerManualIsActive(void);

/** Override the automatic expiratory valve request until manual control is cleared. */
void actuatorControllerExpValveManualSet(uint8_t dutyPercent);

/** Return expiratory valve ownership to the automatic pressure controller. */
void actuatorControllerExpValveManualClear(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTLOGIC_ACTUATORCONTROLLER_H */
/**************************End of file********************************/
