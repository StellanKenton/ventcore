"""Host regression for VAC EMA feedback, plan timing and bounded adaptation."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HARNESS = r'''
/************************************************************************************
* @file     : vti_compensation_test.c
* @brief    : Real scheduler, phase, monitor and flow-controller feedback regression.
***********************************************************************************/
#include <assert.h>
#include <math.h>
#include "breathscheduler.h"
#include "phasecontroller.h"
#include "monitorengine.h"
#include "flowcontroller.h"
#include "controldata.h"
#include "databus.h"
#include "calibration.h"
#include "calibtrans.h"
#include "log.h"
#include "rtos.h"

static float gData[CONTROL_DATA_COUNT];
static float gOffset;
static float gInspiratoryFlow = 40.0F;
static uint32_t gNow;
static float gQuietFlow;

float controlDataGet(ControlData_Index_EnumDef index) { return gData[index]; }
float controlDataMdiffFlowZeroOffsetGet(void) { return gOffset; }
void controlDataMdiffFlowZeroOffsetSet(float offset) { gOffset = offset; }
uint8_t calibrationIsValid(eCalibrationType type) { (void)type; return 1U; }
int8_t calibtransPrsSpeed(float pressure, float *speed) {
    *speed = pressure * 10.0F;
    return CALIBTRANS_STATUS_OK;
}
void logWrite(eLogLevel level, const char *tag, const char *format, ...) {
    (void)level; (void)tag; (void)format;
}
void repRtosEnterCritical(void) {}
void repRtosExitCritical(void) {}

/** Compare physical values with tolerance for single-precision integration. */
static void near(float actual, float expected) { assert(fabsf(actual - expected) < 0.02F); }

/** Fetch a real next plan in scheduler-only tests. */
static stBreathPlan next(void) {
    stBreathPlan lPlan;
    assert(breathSchedulerNextPlanGet(BREATH_TRIGGER_REASON_TIME, &lPlan) == BREATH_CONTROL_SUCCESS);
    return lPlan;
}

/** Restore user settings and all production modules. */
static void reset(void) {
    *GetVentVacSettings() = (stVentVacSettings){.oxygen = 21.0F, .peep = 5.0F,
        .freq = 15.0F, .inspTimeMs = 2000U, .tidalVolume = 500.0F,
        .triggerType = VENT_TRIGGER_OFF, .inspPausePct = 50.0F};
    GetVentLimitSettings()->pressureHigh = 50.0F;
    gOffset = 0.0F;
    gQuietFlow = 0.0F;
    gNow = 0U;
    gInspiratoryFlow = 40.0F;
    assert(breathSchedulerInit() == BREATH_CONTROL_SUCCESS);
    phaseControllerInit();
    monitorEngineInit();
    flowControllerInit();
    assert(breathSchedulerStart(VENT_MD_VAC) == BREATH_CONTROL_SUCCESS);
}

/** Verify EMA initialization, exact update, once-only consumption and conversion. */
static void testEma(void) {
    stBreathPlan lPlan;
    stBreathPlan lNext;
    reset();
    lPlan = next();
    near(lPlan.deliveryTargetMl, 500.0F);
    near(lPlan.filteredVtiMl, 0.0F);
    breathSchedulerVolumeFeedback(&lPlan, 400.0F, 1U);
    breathSchedulerVolumeFeedback(&lPlan, 1.0F, 1U); /* Same sequence is ignored. */
    lNext = next();
    near(lNext.filteredVtiMl, 400.0F);
    near(lNext.volumeCorrectionMl, 80.0F);
    near(lNext.deliveryTargetMl, 580.0F);
    near(lNext.targetTidalVolumeMl, 500.0F);
    near(lNext.inspiratoryFlowLpm, 610.0F * 60.0F / 958.55F);
    assert(lNext.riseTimeMs == 1000U && lNext.holdTimeMs == 1000U);
    breathSchedulerVolumeFeedback(&lPlan, 1.0F, 1U); /* Older plan is ignored. */
    breathSchedulerVolumeFeedback(&lNext, 600.0F, 1U);
    lPlan = next();
    near(lPlan.filteredVtiMl, 500.0F);
    near(lPlan.volumeCorrectionMl, 40.0F);
    breathSchedulerVolumeFeedback(&lPlan, NAN, 1U);
    lPlan = next();
    near(lPlan.filteredVtiMl, 500.0F);
    near(lPlan.volumeCorrectionMl, 40.0F);
    breathSchedulerVolumeFeedback(&lPlan, 100.0F, 0U);
    lPlan = next();
    near(lPlan.filteredVtiMl, 500.0F);
    near(lPlan.volumeCorrectionMl, 40.0F);
    breathSchedulerVolumeFeedback(&lPlan, 0.0F, 1U);
    lPlan = next();
    near(lPlan.volumeCorrectionMl, 40.0F);
}

/** Settings epochs, live limits, modes and restarts must discard old feedback. */
static void testReset(void) {
    stBreathPlan lPlan;
    stBreathPlan lOld;
    unsigned int lIndex;
    for (lIndex = 0U; lIndex < 5U; lIndex++) {
        reset();
        lOld = next();
        breathSchedulerVolumeFeedback(&lOld, 400.0F, 1U);
        if (lIndex == 0U) { GetVentVacSettings()->tidalVolume = 600.0F; }
        if (lIndex == 1U) { GetVentVacSettings()->inspTimeMs = 1500U; }
        if (lIndex == 2U) { GetVentVacSettings()->inspPausePct = 25.0F; }
        if (lIndex == 3U) { GetVentVacSettings()->peep = 10.0F; }
        if (lIndex == 4U) { GetVentLimitSettings()->pressureHigh = 40.0F; }
        breathSchedulerProcess();
        breathSchedulerVolumeFeedback(&lOld, 10.0F, 1U);
        lPlan = next();
        near(lPlan.volumeCorrectionMl, 0.0F);
        near(lPlan.filteredVtiMl, 0.0F);
    }
    reset();
    lOld = next();
    breathSchedulerVolumeFeedback(&lOld, 400.0F, 1U);
    assert(breathSchedulerStop() == BREATH_CONTROL_SUCCESS);
    assert(breathSchedulerStart(VENT_MD_VAC) == BREATH_CONTROL_SUCCESS);
    breathSchedulerVolumeFeedback(&lOld, 10.0F, 1U);
    lPlan = next();
    near(lPlan.volumeCorrectionMl, 0.0F);
    breathSchedulerVolumeFeedback(&lPlan, 400.0F, 1U);
    assert(breathSchedulerSettingsUpdate(VENT_MD_PAC) == BREATH_CONTROL_SUCCESS);
    lPlan = next();
    near(lPlan.volumeCorrectionMl, 0.0F);
    assert(breathSchedulerSettingsUpdate(VENT_MD_VAC) == BREATH_CONTROL_SUCCESS);
    lPlan = next();
    near(lPlan.filteredVtiMl, 0.0F);
    /* A fractional delivery time retains the original effective-time formula. */
    GetVentVacSettings()->inspTimeMs = 1001U;
    GetVentVacSettings()->inspPausePct = 33.0F;
    breathSchedulerProcess();
    lPlan = next();
    near(lPlan.inspiratoryFlowLpm, 530.0F * 60.0F / (670.67F - 41.45F));
}

/** Exercise a simple repeatable volume loss, bounds and the no-chasing deadband. */
static void testConvergence(void) {
    stBreathPlan lPlan;
    float lMeasured;
    float lPrevious;
    unsigned int lIndex;
    reset();
    lPlan = next();
    for (lIndex = 0U; lIndex < 80U; lIndex++) {
        lMeasured = lPlan.deliveryTargetMl - 80.0F;
        lPrevious = lPlan.volumeCorrectionMl;
        breathSchedulerVolumeFeedback(&lPlan, lMeasured, 1U);
        lPlan = next();
        assert(fabsf(lPlan.volumeCorrectionMl - lPrevious) <= 125.01F);
        assert(fabsf(lPlan.volumeCorrectionMl) <= 150.01F);
        if (lIndex == 0U) { near(lPlan.volumeCorrectionMl, 64.0F); }
        if (lIndex == 1U) { near(lMeasured, 484.0F); }
        if (lIndex > 1U) { near(lMeasured, 500.0F); }
    }
    assert(fabsf(lMeasured - 500.0F) <= 5.1F);
    /* Huge persistent mismatch stays bounded in both directions. */
    reset();
    lPlan = next();
    for (lIndex = 0U; lIndex < 30U; lIndex++) {
        breathSchedulerVolumeFeedback(&lPlan, 1.0F, 1U);
        lPlan = next();
    }
    near(lPlan.volumeCorrectionMl, 150.0F);
    reset();
    lPlan = next();
    for (lIndex = 0U; lIndex < 30U; lIndex++) {
        breathSchedulerVolumeFeedback(&lPlan, 1000.0F, 1U);
        lPlan = next();
    }
    near(lPlan.volumeCorrectionMl, -150.0F);
    reset();
    lPlan = next();
    breathSchedulerVolumeFeedback(&lPlan, 498.0F, 1U);
    lPlan = next();
    near(lPlan.filteredVtiMl, 498.0F);
    near(lPlan.volumeCorrectionMl, 0.0F);
}

/** Match production scheduling order with prescribed proximal measurements. */
static void step(void) {
    ePhaseControllerState lPhase;
    gNow += 6U;
    breathSchedulerProcess();
    phaseControllerProcess(gNow);
    lPhase = phaseControllerStateGet();
    gData[PAT_REAL_FLOW] = (lPhase == PHASE_INSP) ? gInspiratoryFlow :
                           (lPhase == PHASE_EXP) ? -20.0F : gQuietFlow;
    gData[PAT_REAL_PRS] = (lPhase == PHASE_INSP) ? 25.0F :
                         (lPhase == PHASE_EXP) ? 5.0F : 0.0F;
    monitorEngineProcess(gNow);
}

/** Wait for a new actual inspiration, not merely the initial expiration plan. */
static stBreathPlan advance(uint32_t previousSequence) {
    stBreathPlan lPlan;
    unsigned int lIndex;
    for (lIndex = 0U; lIndex < 3000U; lIndex++) {
        step();
        if ((phaseControllerStateGet() == PHASE_INSP) &&
            (phaseControllerActivePlanGet(&lPlan) == PHASE_CONTROL_SUCCESS) &&
            (lPlan.sequence != previousSequence)) {
            return lPlan;
        }
    }
    assert(0);
    return (stBreathPlan){0};
}

/** Verify same-boundary feedback, saturation rejection and re-zero recovery. */
static void testIntegration(void) {
    stBreathPlan lPlan;
    stBreathResult lResult;
    stActuatorRequest lRequest;
    float lFiltered;
    float lCorrection;
    reset();
    GetVentVacSettings()->inspTimeMs = 600U;
    GetVentVacSettings()->inspPausePct = 0.0F;
    GetVentVacSettings()->freq = 60.0F;
    gQuietFlow = 1.5F; /* Real startup zero correction after initial plan load. */
    gData[INSP_REAL_FLOW] = 0.0F;
    gData[PAT_REAL_FLOW] = 0.0F;
    gData[PAT_REAL_PRS] = 0.0F;
    lPlan = advance(0U);
    near(lPlan.filteredVtiMl, 0.0F);
    near(phaseControlGet(PHASE_REF_VOLUME), 0.0F);
    for (unsigned int lIndex = 0U; lIndex < 50U; lIndex++) { step(); }
    near(phaseControlGet(PHASE_REF_VOLUME), 500.0F * 258.55F / 558.55F);
    lPlan = advance(lPlan.sequence);
    near(phaseControlGet(PHASE_REF_VOLUME), 0.0F);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert(lResult.sequence + 1U == lPlan.sequence);
    near(lResult.vtiMl, 400.0F);
    near(lPlan.filteredVtiMl, lResult.vtiMl);
    near(lPlan.volumeCorrectionMl, 80.0F);
    gInspiratoryFlow = 60.0F;
    lFiltered = lPlan.filteredVtiMl;
    lPlan = advance(lPlan.sequence);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    near(lPlan.filteredVtiMl, lFiltered + 0.5F * (lResult.vtiMl - lFiltered));
    lFiltered = lPlan.filteredVtiMl;
    lCorrection = lPlan.volumeCorrectionMl;
    /* Drive the real flow controller into its feedforward pressure limit. */
    gData[PAT_REAL_PRS] = 60.0F;
    assert(flowControllerProcess(&lPlan, &lRequest) == ACTUATOR_REQUEST_SUCCESS);
    lPlan = advance(lPlan.sequence);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert((lResult.validMask & BREATH_RESULT_VOLUME_LIMITED) != 0U);
    near(lPlan.filteredVtiMl, lFiltered);
    near(lPlan.volumeCorrectionMl, lCorrection);
    gData[PAT_REAL_FLOW] = NAN;
    monitorEngineProcess(gNow);
    lPlan = advance(lPlan.sequence);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert((lResult.validMask & BREATH_RESULT_VALID_VTI) == 0U);
    near(lPlan.volumeCorrectionMl, lCorrection);
    gOffset += 1.0F;
    step();
    lPlan = advance(lPlan.sequence);
    near(lPlan.filteredVtiMl, 0.0F);
    near(lPlan.volumeCorrectionMl, 0.0F);
}

/** A delivery shorter than the ramp still reaches user VT without dividing by zero. */
static void testShortVolumeReference(void) {
    reset();
    GetVentVacSettings()->inspTimeMs = 1000U;
    GetVentVacSettings()->inspPausePct = 99.0F;
    gData[PAT_REAL_FLOW] = 0.0F;
    gData[INSP_REAL_FLOW] = 0.0F;
    gData[PAT_REAL_PRS] = 0.0F;
    (void)advance(0U);
    near(phaseControlGet(PHASE_REF_VOLUME), 0.0F);
    step();
    near(phaseControlGet(PHASE_REF_VOLUME), 180.0F);
    step();
    assert(phaseControllerVolumePauseActiveGet() != 0U);
    near(phaseControlGet(PHASE_REF_VOLUME), 500.0F);
    assert(breathSchedulerStop() == BREATH_CONTROL_SUCCESS);
    step();
    near(phaseControlGet(PHASE_REF_VOLUME), 0.0F);
}

int main(void) {
    testEma();
    testReset();
    testConvergence();
    testIntegration();
    testShortVolumeReference();
    return 0;
}
/**************************End of file********************************/
'''


def main():
    compiler = os.environ.get("CC") or shutil.which("gcc") or shutil.which("clang")
    if not compiler:
        compiler = next((str(path) for path in (
            Path("C:/msys64/mingw64/bin/gcc.exe"),
            Path("C:/Qt/Tools/mingw1310_64/bin/gcc.exe"),
        ) if path.is_file()), None)
    if not compiler:
        raise SystemExit("Set CC to a native GCC or Clang compiler for this host test.")
    with tempfile.TemporaryDirectory(prefix="ventcore-vti-") as directory:
        harness = Path(directory) / "vti_compensation_test.c"
        harness.write_text(HARNESS, encoding="utf-8", newline="\n")
        executable = Path(directory) / "vti_compensation_test.exe"
        includes = ["user/app/ventlogic", "user/app/ventalgo", "user/app/databus",
                    "user/app/calibration", "user/module/rtos", "user/tools/controller",
                    "user/tools/filter/numfilter", "user/module/log", "user/tools/ringbuffer"]
        sources = ["user/app/ventlogic/breathscheduler.c", "user/app/ventlogic/phasecontroller.c",
                   "user/app/ventlogic/monitorengine.c", "user/app/ventalgo/flowcontroller.c",
                   "user/app/databus/settingdata.c", "user/tools/controller/pid.c"]
        command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                   *[f"-I{ROOT / path}" for path in includes], str(harness),
                   *[str(ROOT / path) for path in sources], "-o", str(executable)]
        environment = os.environ.copy()
        environment["PATH"] = str(Path(compiler).parent) + os.pathsep + environment["PATH"]
        subprocess.run(command, check=True, env=environment)
        subprocess.run([str(executable)], check=True, env=environment)
    print("PASS: EMA, next-breath timing, once-only feedback, conversion, bounds, convergence, faults, reset")


if __name__ == "__main__":
    main()
