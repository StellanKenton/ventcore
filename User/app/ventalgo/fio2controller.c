/************************************************************************************
* @file     : fio2controller.c
* @brief    : Ventilation FiO2 controller.
* @details  : Keeps oxygen closed until the calibrated FiO2 algorithm is enabled.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "fio2controller.h"

#include <stddef.h>

void fio2ControllerInit(void)
{
}

int8_t fio2ControllerProcess(const stBreathPlan *plan, stActuatorRequest *request)
{
    if ((plan == NULL) || (request == NULL)) {
        return ACTUATOR_REQUEST_ERROR_PARAM;
    }
    if ((request->validMask & ACTUATOR_REQUEST_VALID_BREATH_OUTPUTS) !=
        ACTUATOR_REQUEST_VALID_BREATH_OUTPUTS) {
        return ACTUATOR_REQUEST_ERROR_STATE;
    }

    /* FiO2 control remains disabled until a calibrated oxygen strategy is available. */
    request->oxygenValveDuty = 0U;
    return ACTUATOR_REQUEST_SUCCESS;
}

/**************************End of file********************************/
