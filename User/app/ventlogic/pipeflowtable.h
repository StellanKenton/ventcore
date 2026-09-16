/************************************************************************************
* @file     : pipeflowtable.h
* @brief    : Fixed patient-circuit pressure-to-flow reference.
* @details  : Linear interpolation with clamped endpoints; task context only.
* @author   :
* @date     : 2026-09-15
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef USER_APP_VENTLOGIC_PIPEFLOWTABLE_H
#define USER_APP_VENTLOGIC_PIPEFLOWTABLE_H

#include <stdint.h>
#include "settingdata.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PIPE_FLOW_TABLE_SUCCESS          1
#define PIPE_FLOW_TABLE_ERROR_PARAM     (-1)
#define PIPE_FLOW_TABLE_POINT_COUNT      6U

/** Convert cmH2O to L/min; adult and pediatric patients share one table. */
int8_t pipeFlowTableGet(eVentPatientType patientType, float pressureCmh2o, float *flowLpm);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_VENTLOGIC_PIPEFLOWTABLE_H */
/*************************************** End of file ********************************/
