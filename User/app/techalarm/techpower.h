/************************************************************************************
* @file     : techpower.h
* @brief    : Technical alarm interface; task context only.
* @details  : Current alarm states using the legacy MCM wire mapping.
* @author   :
* @date     : 2026-09-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_TECHALARM_TECHPOWER_H
#define USER_APP_TECHALARM_TECHPOWER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Placeholder: POWER_FAULT_PCM_3V3. */
bool techPowerPcm3V3Detect(uint32_t nowMs);

/** Placeholder: POWER_FAULT_VDD_24V. */
bool techPowerVdd24VDetect(uint32_t nowMs);

/** Placeholder: POWER_FAULT_AVDD_5V. */
bool techPowerAvdd5VDetect(uint32_t nowMs);


#ifdef __cplusplus
}
#endif

#endif /* USER_APP_TECHALARM_TECHPOWER_H */
/*************************************** End of file ********************************/
