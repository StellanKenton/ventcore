/************************************************************************************
* @file     : techcomm.h
* @brief    : Technical alarm interface; task context only.
* @details  : Current alarm states using the legacy MCM wire mapping.
* @author   :
* @date     : 2026-09-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_TECHALARM_TECHCOMM_H
#define USER_APP_TECHALARM_TECHCOMM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Placeholder: COMM_FAULT_MOTOR_DISCONNECT. */
bool techCommMotorDisconnectDetect(uint32_t nowMs);

/** Placeholder: COMM_FAULT_PCM_DISCONNECT. */
bool techCommPcmDisconnectDetect(uint32_t nowMs);


#ifdef __cplusplus
}
#endif

#endif /* USER_APP_TECHALARM_TECHCOMM_H */
/*************************************** End of file ********************************/
