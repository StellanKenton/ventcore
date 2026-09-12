/************************************************************************************
* @file     : techcomm.c
* @brief    : Technical alarm detectors; unimplemented entries remain inactive.
* @details  : Current alarm states using the legacy MCM wire mapping.
* @author   :
* @date     : 2026-09-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "techcomm.h"

/** Placeholder: COMM_FAULT_MOTOR_DISCONNECT. */
bool techCommMotorDisconnectDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: COMM_FAULT_PCM_DISCONNECT. */
bool techCommPcmDisconnectDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/*************************************** End of file ********************************/
