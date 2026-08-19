/************************************************************************************
* @file     : databus.c
* @brief    : Ventilation data acquisition and 10 Hz filtering.
***********************************************************************************/
#include "databus.h"

#include <stddef.h>

#include "adc.h"
#include "butterworthfilter.h"
#include "controldata.h"
#include "iir1.h"
#include "numfilter.h"
#include "sfm3119.h"

static MovAvgFilterObj gcontrolDataFilters[VENT_DATA_CHANNEL_COUNT];
static float gcontrolDataFilterBuffers[VENT_DATA_CHANNEL_COUNT][VENT_DATA_10HZ_WINDOW_SIZE];
static ButterworthFilterObj gButterworthFilters[VENT_DATA_CHANNEL_COUNT];
/* Second-order Butterworth low-pass coefficients for Fs = 166.667 Hz and Fc = 14 Hz. */
static const float gButterworth14HzNum[3U] = {0.05017146F, 0.10034292F, 0.05017146F};
static const float gButterworth14HzDen[3U] = {1.0F, -1.27411492F, 0.47480076F};
static stLpf1 gPressureFilters[4U][2U];
static stLpf1 gFlowFilters[2U][2U];
static stLpf1 gInspFlowTriggerFilter;
static uint8_t gcontrolDataFiltersInitialized = 0U;

/** Average two 3 ms samples and run two cascaded low-pass filters. */
static float controlDataLpf2Run(stLpf1 filters[2U], float sample1, float sample2) {
    float lInput = 0.5F * (sample1 + sample2);
    float lOutput = lpf1Run(&filters[0U], lInput);

    return lpf1Run(&filters[1U], lOutput);
}

static void controlDataFiltersInit(void) {
    uint8_t lIndex;

    for (lIndex = 0U; lIndex < VENT_DATA_CHANNEL_COUNT; lIndex++) {
        UnitAlgoMovAvgFilterInit(&gcontrolDataFilters[lIndex],
                                  gcontrolDataFilterBuffers[lIndex],
                                  VENT_DATA_10HZ_WINDOW_SIZE);
        UnitAlgoButterworthFilterInit(&gButterworthFilters[lIndex],
                                      gButterworth14HzNum,
                                      gButterworth14HzDen);
    }

    for (lIndex = 0U; lIndex < 4U; lIndex++) {
        lpf1Init(&gPressureFilters[lIndex][0U], 0.625F);
        lpf1Init(&gPressureFilters[lIndex][1U], 0.625F);
        // 0.6~0.65 is a good range for the second filter gain, 
        // which is the most important one for the final output. 
        // The first filter is just to reduce the noise of the input signal.
    }

    for (lIndex = 0U; lIndex < 2U; lIndex++) {
        lpf1Init(&gFlowFilters[lIndex][0U], 0.625F);
        lpf1Init(&gFlowFilters[lIndex][1U], 0.625F);
    }
    lpf1Init(&gInspFlowTriggerFilter, 0.7F);

    gcontrolDataFiltersInitialized = 1U;
}

void controlDataRawProcess(void) {
    const SFM3119_Result *lAirResult = sfm3119GetResult(SFM3119_AIR_INDEX);
    const SFM3119_Result *lO2Result = sfm3119GetResult(SFM3119_O2_INDEX);
    float lInspPrs = (float)adc_value[ADC_IDX_INSP_PRS];
    float lMdiffPrs = (float)adc_value[ADC_IDX_MDIFF_PRS];
    float lPeepPrs = (float)adc_value[ADC_IDX_PEEP_PRS];
    float lExpPrs = (float)adc_value[ADC_IDX_EXP_PRS];
    float lInspFlow = (lAirResult != NULL) ? lAirResult->flow_slm : 0.0F;
    float lO2Flow = (lO2Result != NULL) ? lO2Result->flow_slm : 0.0F;

    controlDataSet(RAW_INSP_AD_PRE, controlDataGet(RAW_INSP_AD));
    controlDataSet(RAW_INSP_AD, lInspPrs);
    controlDataSet(RAW_MDIFF_AD_PRE, controlDataGet(RAW_MDIFF_AD));
    controlDataSet(RAW_MDIFF_AD, lMdiffPrs);
    controlDataSet(RAW_PEEP_AD_PRE, controlDataGet(RAW_PEEP_AD));
    controlDataSet(RAW_PEEP_AD, lPeepPrs);
    controlDataSet(RAW_EXP_AD_PRE, controlDataGet(RAW_EXP_AD));
    controlDataSet(RAW_EXP_AD, lExpPrs);
    controlDataSet(RAW_INSP_FLOW_PRE, controlDataGet(RAW_INSP_FLOW));
    controlDataSet(RAW_INSP_FLOW, lInspFlow);
    controlDataSet(RAW_O2_FLOW_PRE, controlDataGet(RAW_O2_FLOW));
    controlDataSet(RAW_O2_FLOW, lO2Flow);
}

void controlDataFilterProcess(void) {
    float lFiltered;

    if (gcontrolDataFiltersInitialized == 0U) {
        controlDataFiltersInit();
    }

    lFiltered = UnitAlgoMovAvgFilterUpdata(&gcontrolDataFilters[0U], controlDataGet(RAW_INSP_AD));
    controlDataSet(INSP_PRS_10HZ, lFiltered);
    lFiltered = UnitAlgoMovAvgFilterUpdata(&gcontrolDataFilters[1U], controlDataGet(RAW_MDIFF_AD));
    controlDataSet(MDIFF_PRS_10HZ, lFiltered);
    lFiltered = UnitAlgoMovAvgFilterUpdata(&gcontrolDataFilters[2U], controlDataGet(RAW_PEEP_AD));
    controlDataSet(PEEP_PRS_10HZ, lFiltered);
    lFiltered = UnitAlgoMovAvgFilterUpdata(&gcontrolDataFilters[3U], controlDataGet(RAW_EXP_AD));
    controlDataSet(EXP_PRS_10HZ, lFiltered);
    lFiltered = UnitAlgoMovAvgFilterUpdata(&gcontrolDataFilters[4U], controlDataGet(RAW_INSP_FLOW));
    controlDataSet(INSP_FLOW_10HZ, lFiltered);
    lFiltered = UnitAlgoMovAvgFilterUpdata(&gcontrolDataFilters[5U], controlDataGet(RAW_O2_FLOW));
    controlDataSet(O2_FLOW_10HZ, lFiltered);

    lFiltered = UnitAlgoButterworthFilterUpdate(controlDataGet(RAW_INSP_AD), &gButterworthFilters[0U]);
    controlDataSet(INSP_PRS_BWF, lFiltered);
    lFiltered = UnitAlgoButterworthFilterUpdate(controlDataGet(RAW_MDIFF_AD), &gButterworthFilters[1U]);
    controlDataSet(MDIFF_PRS_BWF, lFiltered);
    lFiltered = UnitAlgoButterworthFilterUpdate(controlDataGet(RAW_PEEP_AD), &gButterworthFilters[2U]);
    controlDataSet(PEEP_PRS_BWF, lFiltered);
    lFiltered = UnitAlgoButterworthFilterUpdate(controlDataGet(RAW_EXP_AD), &gButterworthFilters[3U]);
    controlDataSet(EXP_PRS_BWF, lFiltered);
    lFiltered = UnitAlgoButterworthFilterUpdate(controlDataGet(RAW_INSP_FLOW), &gButterworthFilters[4U]);
    controlDataSet(INSP_FLOW_BWF, lFiltered);
    lFiltered = UnitAlgoButterworthFilterUpdate(controlDataGet(RAW_O2_FLOW), &gButterworthFilters[5U]);
    controlDataSet(O2_FLOW_BWF, lFiltered);

    lFiltered = controlDataLpf2Run(gPressureFilters[0U], controlDataGet(RAW_INSP_AD), controlDataGet(RAW_INSP_AD_PRE));
    controlDataSet(INSP_PRS_FILTERED, lFiltered);
    lFiltered = controlDataLpf2Run(gPressureFilters[1U], controlDataGet(RAW_MDIFF_AD), controlDataGet(RAW_MDIFF_AD_PRE));
    controlDataSet(MDIFF_PRS_FILTERED, lFiltered);
    lFiltered = controlDataLpf2Run(gPressureFilters[2U], controlDataGet(RAW_PEEP_AD), controlDataGet(RAW_PEEP_AD_PRE));
    controlDataSet(PEEP_PRS_FILTERED, lFiltered);
    lFiltered = controlDataLpf2Run(gPressureFilters[3U], controlDataGet(RAW_EXP_AD), controlDataGet(RAW_EXP_AD_PRE));
    controlDataSet(EXP_PRS_FILTERED, lFiltered);

    lFiltered = controlDataLpf2Run(gFlowFilters[0U], controlDataGet(RAW_INSP_FLOW), controlDataGet(RAW_INSP_FLOW_PRE));
    controlDataSet(INSP_FLOW_FILTERED, lFiltered);
    lFiltered = lpf1Run(&gInspFlowTriggerFilter,
                        0.5F * (controlDataGet(RAW_INSP_FLOW) + controlDataGet(RAW_INSP_FLOW_PRE)));
    controlDataSet(INSP_FLOW_TRIGER_FILTERED, lFiltered);
    lFiltered = controlDataLpf2Run(gFlowFilters[1U], controlDataGet(RAW_O2_FLOW), controlDataGet(RAW_O2_FLOW_PRE));
    controlDataSet(O2_FLOW_FILTERED, lFiltered);
}
