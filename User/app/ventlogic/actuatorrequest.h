/************************************************************************************
* @file     : actuatorrequest.h
* @brief    : Unified ventilation actuator request contract.
* @details  : Carries one complete bounded output proposal into actuator arbitration.
* @author   :
* @date     : 2026-08-26
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTLOGIC_ACTUATORREQUEST_H
#define USER_APP_VENTLOGIC_ACTUATORREQUEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ACTUATOR_REQUEST_SUCCESS              1
#define ACTUATOR_REQUEST_ERROR_PARAM         (-1)
#define ACTUATOR_REQUEST_ERROR_STATE         (-2)
#define ACTUATOR_REQUEST_VALID_BLOWER          (1U << 0)
#define ACTUATOR_REQUEST_VALID_EXP_VALVE       (1U << 1)
#define ACTUATOR_REQUEST_VALID_OXYGEN_VALVE    (1U << 2)
#define ACTUATOR_REQUEST_VALID_RELIEF_VALVE    (1U << 3)
#define ACTUATOR_REQUEST_VALID_BREATH_OUTPUTS  (ACTUATOR_REQUEST_VALID_BLOWER | \
                                                ACTUATOR_REQUEST_VALID_EXP_VALVE)

typedef struct stActuatorRequest {
    uint16_t blowerTarget;
    uint8_t expiratoryValveDuty;
    uint8_t oxygenValveDuty;
    uint8_t reliefValveDuty;
    uint8_t validMask;
} stActuatorRequest;

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTLOGIC_ACTUATORREQUEST_H */
/**************************End of file********************************/
