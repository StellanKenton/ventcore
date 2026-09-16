/************************************************************************************
* @file     : pipeflowtable.c
* @brief    : Fixed patient-circuit pressure-to-flow reference.
* @details  : Adult/pediatric and neonatal calibration points in cmH2O and L/min.
* @author   :
* @date     : 2026-09-15
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "pipeflowtable.h"

#include <float.h>
#include <stddef.h>

static const float gPipeFlowAdultTable[PIPE_FLOW_TABLE_POINT_COUNT][2] = {
    {1.07F, 10.4F}, {5.14F, 39.5F}, {11.39F, 60.6F},
    {24.56F, 90.5F}, {38.98F, 117.3F}, {49.34F, 133.8F}
};
static const float gPipeFlowNeonatalTable[PIPE_FLOW_TABLE_POINT_COUNT][2] = {
    {3.42F, 1.1F}, {5.61F, 6.4F}, {10.08F, 14.1F},
    {21.26F, 26.2F}, {51.34F, 46.6F}, {76.75F, 56.9F}
};

/** Interpolate adjacent points and clamp pressures outside the measured range. */
int8_t pipeFlowTableGet(eVentPatientType patientType, float pressureCmh2o, float *flowLpm) {
    const float (*lTable)[2];
    uint8_t lIndex;

    if ((flowLpm == NULL) || ((unsigned int)patientType >= VENT_PATIENT_TYPE_COUNT) ||
        !(pressureCmh2o >= -FLT_MAX && pressureCmh2o <= FLT_MAX)) {
        return PIPE_FLOW_TABLE_ERROR_PARAM;
    }
    lTable = (patientType == VENT_PATIENT_NEONATAL) ? gPipeFlowNeonatalTable : gPipeFlowAdultTable;
    if (pressureCmh2o <= lTable[0][0]) {
        *flowLpm = lTable[0][1];
        return PIPE_FLOW_TABLE_SUCCESS;
    }
    for (lIndex = 1U; lIndex < PIPE_FLOW_TABLE_POINT_COUNT; lIndex++) {
        if (pressureCmh2o <= lTable[lIndex][0]) {
            *flowLpm = lTable[lIndex - 1U][1] +
                (pressureCmh2o - lTable[lIndex - 1U][0]) /
                (lTable[lIndex][0] - lTable[lIndex - 1U][0]) *
                (lTable[lIndex][1] - lTable[lIndex - 1U][1]);
            return PIPE_FLOW_TABLE_SUCCESS;
        }
    }
    *flowLpm = lTable[PIPE_FLOW_TABLE_POINT_COUNT - 1U][1];
    return PIPE_FLOW_TABLE_SUCCESS;
}

/*************************************** End of file ********************************/
