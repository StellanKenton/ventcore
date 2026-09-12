/************************************************************************************
* @file     : techpower.c
* @brief    : Technical alarm detectors; unimplemented entries remain inactive.
* @details  : Current alarm states using the legacy MCM wire mapping.
* @author   :
* @date     : 2026-09-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "techpower.h"

/** Placeholder: POWER_FAULT_PCM_3V3. */
bool techPowerPcm3V3Detect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: POWER_FAULT_VDD_24V. */
bool techPowerVdd24VDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/** Placeholder: POWER_FAULT_AVDD_5V. */
bool techPowerAvdd5VDetect(uint32_t nowMs) {
    (void)nowMs;
    return false;
}

/*************************************** End of file ********************************/
