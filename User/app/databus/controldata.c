/************************************************************************************
* @file     : controldata.c
* @brief    : Control data bus implementation.
***********************************************************************************/
#include "controldata.h"

static volatile float gControlData[CONTROL_DATA_COUNT];

float controlDataGet(ControlData_Index_EnumDef index)
{
    if ((unsigned int)index >= (unsigned int)CONTROL_DATA_COUNT) {
        return 0.0F;
    }

    return gControlData[index];
}

void controlDataSet(ControlData_Index_EnumDef index, float data)
{
    if ((unsigned int)index >= (unsigned int)CONTROL_DATA_COUNT) {
        return;
    }

    gControlData[index] = data;
}
