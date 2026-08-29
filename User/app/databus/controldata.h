/************************************************************************************
* @file     : controldata.h
* @brief    : Control data bus interface.
* @details  : Provides indexed access to shared float control data.
***********************************************************************************/
#ifndef USER_APP_DATABUS_CONTROLDATA_H
#define USER_APP_DATABUS_CONTROLDATA_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RAW_INSP_AD = 0,
    RAW_INSP_AD_PRE,
    RAW_PEEP_AD,
    RAW_PEEP_AD_PRE,
    RAW_EXP_AD,
    RAW_EXP_AD_PRE,
    RAW_MDIFF_AD,
    RAW_MDIFF_AD_PRE,
    RAW_INSP_FLOW,
    RAW_INSP_FLOW_PRE,
    RAW_O2_FLOW,
    RAW_O2_FLOW_PRE,
    RAW_BLOWER_SPEED,

    INSP_PRS_BWF,
    PEEP_PRS_BWF,
    EXP_PRS_BWF,
    MDIFF_PRS_BWF,
    INSP_FLOW_BWF,
    O2_FLOW_BWF,

    INSP_PRS_FILTERED,
    PEEP_PRS_FILTERED,
    EXP_PRS_FILTERED,
    MDIFF_PRS_FILTERED,
    INSP_FLOW_FILTERED,
    INSP_FLOW_TRIGER_FILTERED,
    O2_FLOW_FILTERED,

    INSP_REAL_PRS,
    PEEP_REAL_PRS,
    EXP_REAL_PRS,
    PAT_REAL_PRS,
    MDIFF_REAL_FLOW,

    PREDICT_PAT_PRS,
    
    CONTROL_DATA_COUNT
} ControlData_Index_EnumDef;

float controlDataGet(ControlData_Index_EnumDef index);
void controlDataSet(ControlData_Index_EnumDef index, float data);

#ifdef __cplusplus
}
#endif

#endif /* USER_APP_DATABUS_CONTROLDATA_H */
