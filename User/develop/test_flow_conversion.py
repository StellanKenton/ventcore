"""Run production flow conversion against nonlinear bidirectional calibration data."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HARNESS = r'''
/************************************************************************************
* @file     : flow_conversion_test.c
* @brief    : Host regression for calibrated proximal flow and gas correction.
***********************************************************************************/
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include "calibration.h"
#include "calibtrans.h"
#include "controldata.h"
#include "databus.h"
#include "settingdata.h"
#include "adc.h"
#include "sfm3119.h"
#include "blower_vcm.h"

volatile uint16_t adc_value[ADC_CH_COUNT];
const SFM3119_Result *sfm3119GetResult(eSfm3119SensorIndex index) { (void)index; return NULL; }
int8_t blowerVcmGetFeedback(stBlowerVcmFeedback *feedback) { *feedback = (stBlowerVcmFeedback){0}; return 1; }

static stCalibrationProxFlow gTable;
static stCalibrationZero gZero = {.proxPressureAd = 2048, .peepPressureAd = 2048};
static stCalibrationPressure gPressure;
static stVentPatientSettings gSettings;
static int gAvailable = 1;
const stCalibrationProxFlow *calibrationGetProxFlow(void) { return gAvailable ? &gTable : NULL; }
const stCalibrationZero *calibrationGetZero(void) { return &gZero; }
const stCalibrationPressure *calibrationGetPressure(void) { return &gPressure; }
const stCalibrationOxygenValve *calibrationGetOxygenValve(void) { return NULL; }
stVentPatientSettings *GetVentPatientSettings(void) { return &gSettings; }

/** Use different nonlinear curves for inspiration and expiration. */
static float adc(float flow) {
    return 2048.0F + flow * fabsf(flow) * ((flow >= 0) ? 0.04F : 0.06F);
}

/** Feed pressurized differential ADC and matched pressure through production filters. */
static float sampleAtPressure(float first, float second, float drift, float pressure) {
    float lDensityRatio = 1 + pressure / CALIBTRANS_AMBIENT_PRESSURE_CMH2O;
    controlDataSet(RAW_MDIFF_AD_PRE, 2048 + (adc(first) - 2048) / lDensityRatio + drift);
    controlDataSet(RAW_MDIFF_AD, 2048 + (adc(second) - 2048) / lDensityRatio + drift);
    controlDataSet(RAW_PEEP_AD, 2048 + pressure * 10);
    controlDataSet(RAW_PEEP_AD_PRE, 2048 + pressure * 10);
    controlDataFilterProcess();
    controlDataCalibrationProcess();
    return controlDataGet(PAT_REAL_FLOW);
}

/** Feed ambient-pressure samples for the zero-offset regressions. */
static float sample(float first, float second, float drift) {
    return sampleAtPressure(first, second, drift, 0);
}

int main(void) {
    float lFlow;
    float lOffset;
    float lCoefficient;
    unsigned int lIndex;
    gSettings.Gas = VENT_GAS_ATPS;
    for (lIndex = 0; lIndex < 8; ++lIndex) {
        gPressure.peepAdcValues[lIndex] = 2048 + lIndex * 100;
        gPressure.pressureValues[lIndex] = lIndex * 10;
    }
    for (lIndex = 0; lIndex < 32; ++lIndex) {
        lFlow = (lIndex < 16) ? ((float)lIndex - 15.0F) * 10.0F :
                              ((float)lIndex - 16.0F) * 10.0F;
        gTable.adultFlow[lIndex] = lFlow;
        gTable.adultFlowAd[lIndex] = adc(lFlow);
    }
    /* All knots and an interior segment must survive forward conversion. */
    for (lIndex = 0; lIndex < 32; ++lIndex) {
        assert(calibtransAdultProxFlow(gTable.adultFlowAd[lIndex], &lFlow) == 1);
        assert(fabsf(lFlow - gTable.adultFlow[lIndex]) < 0.001F);
    }
    assert(calibtransAdultProxFlow((adc(30) + adc(40)) * 0.5F, &lFlow) == 1);
    assert(fabsf(lFlow - 35) < 0.001F);
    /* Fixed mass flow creates less differential pressure in a pressurized circuit. */
    for (lIndex = 0; lIndex < 32; ++lIndex) {
        float lDensityRatio = 1 + 30 / CALIBTRANS_AMBIENT_PRESSURE_CMH2O;
        float lAd = 2048 + (gTable.adultFlowAd[lIndex] - 2048) / lDensityRatio;
        assert(calibtransAdultProxFlowAtPressure(lAd, 30, &lFlow) == 1);
        assert(fabsf(lFlow - gTable.adultFlow[lIndex]) < 0.001F);
    }
    assert(calibtransAdultProxFlowAtPressure(2048, NAN, &lFlow) < 0);
    assert(calibtransAdultProxFlowAtPressure(2048, -CALIBTRANS_AMBIENT_PRESSURE_CMH2O, &lFlow) < 0);
    /* Remove positive sensor drift in ADC space; both flow directions stay exact. */
    for (lIndex = 0; lIndex < 100; ++lIndex) sample(0, 0, 3);
    assert(controlDataMdiffFlowZeroOffsetSet(controlDataGet(PAT_REAL_FLOW)) == DATABUS_STATUS_OK);
    assert(fabsf(controlDataGet(PAT_REAL_FLOW)) < 0.01F);
    for (lIndex = 0; lIndex < 100; ++lIndex) sample(30, 30, 3);
    assert(fabsf(controlDataGet(PAT_REAL_FLOW) - 30) < 0.01F);
    for (lIndex = 0; lIndex < 100; ++lIndex) sample(-60, -60, 3);
    assert(fabsf(controlDataGet(PAT_REAL_FLOW) + 60) < 0.01F);
    /* Re-zero after a drift reversal; cumulative API must apply only the residual. */
    for (lIndex = 0; lIndex < 100; ++lIndex) sample(0, 0, -2);
    assert(controlDataMdiffFlowZeroOffsetSet(controlDataMdiffFlowZeroOffsetGet() +
        controlDataGet(PAT_REAL_FLOW)) == DATABUS_STATUS_OK);
    for (lIndex = 0; lIndex < 100; ++lIndex) sample(30, 30, -2);
    assert(fabsf(controlDataGet(PAT_REAL_FLOW) - 30) < 0.01F);
    lOffset = controlDataMdiffFlowZeroOffsetGet();
    gSettings.Gas = VENT_GAS_BTPS;
    sample(30, 30, -2);
    lCoefficient = controlDataGet(BTPS_COEFFICIENT);
    assert(fabsf(controlDataGet(PAT_REAL_FLOW) - 30 * lCoefficient) < 0.01F);
    assert(fabsf(controlDataMdiffFlowZeroOffsetGet() - lOffset * lCoefficient) < 0.001F);
    assert(fabsf(lCoefficient - (1013.0F / 950.34F) * (310.15F / 293.15F)) < 0.00001F);
    for (lIndex = 0; lIndex < 100; ++lIndex) sampleAtPressure(30, 30, -2, 30);
    assert(fabsf(controlDataGet(PAT_REAL_FLOW) - 30 * lCoefficient) < 0.02F);
    for (lIndex = 0; lIndex < 100; ++lIndex) sampleAtPressure(-60, -60, -2, 5);
    assert(fabsf(controlDataGet(PAT_REAL_FLOW) + 60 * lCoefficient) < 0.02F);
    for (lIndex = 0; lIndex < 100; ++lIndex) sample(0, 0, -2);
    assert(fabsf(controlDataGet(PAT_REAL_FLOW)) < 0.01F);
    gAvailable = 0;
    sample(0, 0, -2);
    assert(isnan(controlDataGet(PAT_REAL_FLOW)));
    gAvailable = 1;
    for (lIndex = 0; lIndex < 100; ++lIndex) sample(0, 0, -2);
    assert(fabsf(controlDataGet(PAT_REAL_FLOW)) < 0.01F);
    return 0;
}
/**************************End of file********************************/
'''


def main():
    compiler = os.environ.get('CC') or shutil.which('gcc') or shutil.which('clang')
    if not compiler:
        compiler = next((str(p) for p in (Path('C:/msys64/mingw64/bin/gcc.exe'),
            Path('C:/Qt/Tools/mingw1310_64/bin/gcc.exe')) if p.is_file()), None)
    if not compiler:
        raise SystemExit('Set CC to a native GCC or Clang compiler')
    paths = ['User/app/databus', 'User/app/calibration', 'User/module/rtos', 'User/bsp/adc',
             'User/bsp/blower_vcm', 'User/bsp/sf06sdk',
             'User/tools/filter/butterworth', 'User/tools/filter/iir1']
    sources = ['User/app/databus/databus.c', 'User/app/databus/controldata.c',
               'User/app/calibration/calibtrans.c',
               'User/tools/filter/butterworth/butterworthfilter.c',
               'User/tools/filter/iir1/iir1.c']
    environment = os.environ.copy()
    environment['PATH'] = str(Path(compiler).parent) + os.pathsep + environment['PATH']
    with tempfile.TemporaryDirectory(prefix='vent-flow-conversion-') as directory:
        harness = Path(directory) / 'flow_conversion_test.c'
        harness.write_text(HARNESS, encoding='utf-8', newline='\n')
        executable = Path(directory) / 'test.exe'
        subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
            '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
            *[f'-I{ROOT / p}' for p in paths], str(harness),
            *[str(ROOT / p) for p in sources], '-o', str(executable)], check=True, env=environment)
        subprocess.run([str(executable)], check=True, env=environment)
    print('PASS: nonlinear knots, ADC zero drift/re-zero, BTPS, pressure density, invalid table')


if __name__ == '__main__':
    main()
