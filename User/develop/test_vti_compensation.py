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
#include <stdlib.h>
#include "breathscheduler.h"
#include "phasecontroller.h"
#include "triggerengine.h"
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
int8_t controlDataMdiffFlowZeroOffsetSet(float offset) {
    gOffset = offset;
    return DATABUS_STATUS_OK;
}
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
    #if BREATH_VOLUME_FLOW_COMPENSATION_ENABLE
    near(lPlan.inspiratoryFlowLpm, 530.0F * 60.0F / (670.67F - 41.45F));
#else
    near(lPlan.inspiratoryFlowLpm, 500.0F * 60.0F / 670.67F);
#endif
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
#if BREATH_VOLUME_FLOW_COMPENSATION_ENABLE
    near(lPlan.volumeCorrectionMl, 80.0F);
#else
    near(lPlan.volumeCorrectionMl, 500.0F * 20.0F / 600.0F);
#endif
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


/** Exercise fixed flow, both adaptation directions and all requested targets. */
static void testTimeCompensation(void) {
    const float lTargets[] = {300.0F, 500.0F, 800.0F};
    for (unsigned int lTarget = 0U; lTarget < 3U; lTarget++) {
        for (int lDirection = -1; lDirection <= 1; lDirection += 2) {
            reset();
            GetVentVacSettings()->tidalVolume = lTargets[lTarget];
            breathSchedulerProcess();
            stBreathPlan lPlan = next();
            float lFlow = lPlan.inspiratoryFlowLpm;
            float lMeasured = 0.0F;
            for (unsigned int lIndex = 0U; lIndex < 50U; lIndex++) {
                uint32_t lPrevious = lPlan.riseTimeMs;
                /* Independent constant-flow plant with a signed 100 ms area error. */
                lMeasured = lFlow * ((float)lPlan.riseTimeMs - lDirection * 100.0F) / 60.0F;
                breathSchedulerVolumeFeedback(&lPlan, lMeasured, 1U);
                lPlan = next();
                near(lPlan.inspiratoryFlowLpm, lFlow);
                assert(abs((int)lPlan.riseTimeMs - (int)lPrevious) <= 20);
                assert(lPlan.holdTimeMs == 1000U);
                assert(lPlan.maximumInspiratoryTimeMs == lPlan.riseTimeMs + lPlan.holdTimeMs);
                assert(lPlan.maximumInspiratoryTimeMs + lPlan.expiratoryTimeMs == 4000U);
                assert(lPlan.expiratoryTimeMs >= lPlan.minimumExpiratoryTimeMs);
                assert(lPlan.riseTimeMs >= 700U && lPlan.riseTimeMs <= 1300U);
            }
            assert(fabsf(lMeasured - lTargets[lTarget]) <= lTargets[lTarget] * 0.005F + 1.0F);
            float lCorrection = lPlan.volumeCorrectionMl;
            breathSchedulerVolumeFeedback(&lPlan, NAN, 1U);
            lPlan = next();
            near(lPlan.volumeCorrectionMl, lCorrection);
            breathSchedulerVolumeFeedback(&lPlan, 1.0F, 0U);
            lPlan = next();
            near(lPlan.volumeCorrectionMl, lCorrection);
            GetVentVacSettings()->peep += 1.0F;
            breathSchedulerProcess();
            lPlan = next();
            near(lPlan.volumeCorrectionMl, 0.0F);
            for (unsigned int lIndex = 0U; lIndex < 100U; lIndex++) {
                breathSchedulerVolumeFeedback(&lPlan, 1.0F, 1U);
                lPlan = next();
            }
            assert(lPlan.riseTimeMs == 1300U);
        }
    }
    /* No positive adaptation may consume the mandatory expiration reserve. */
    reset();
    GetVentVacSettings()->freq = 60.0F;
    GetVentVacSettings()->inspTimeMs = 800U;
    GetVentVacSettings()->inspPausePct = 0.0F;
    breathSchedulerProcess();
    stBreathPlan lPlan = next();
    for (unsigned int lIndex = 0U; lIndex < 30U; lIndex++) {
        breathSchedulerVolumeFeedback(&lPlan, 1.0F, 1U);
        lPlan = next();
        assert(lPlan.expiratoryTimeMs >= BREATH_PEEP_LOCK_TIME_MS);
        assert(lPlan.maximumInspiratoryTimeMs + lPlan.expiratoryTimeMs == 1000U);
    }
    assert(lPlan.maximumInspiratoryTimeMs == 808U);
}

/** Verify VAC efforts start volume breaths and restart the mandatory timer. */
static void testVacTrigger(void) {
    stBreathPlan lPlan;
    uint32_t lExpirationStart;
    uint32_t lInspirationStart;
    for (unsigned int lCase = 0U; lCase < 4U; lCase++) {
        unsigned int lType = lCase == 3U ? VENT_TRIGGER_FLOW : lCase;
        reset();
        triggerEngineInit();
        GetVentVacSettings()->triggerType = (eVentTriggerType)lType;
        GetVentVacSettings()->flowTriggerLpm = lCase == 3U ? 10.0F : 1.0F;
        GetVentVacSettings()->pressureTriggerCmh2o = -1.0F;
        breathSchedulerProcess();
        gData[INSP_REAL_FLOW] = 0.0F;
        gData[PAT_REAL_FLOW] = 0.0F;
        gData[PAT_REAL_PRS] = 0.0F;
        for (gNow = 0U; gNow < 2000U; gNow += 6U) {
            phaseControllerProcess(gNow);
            if (phaseControllerStateGet() == PHASE_EXP) { break; }
        }
        assert(phaseControllerStateGet() == PHASE_EXP);
        lExpirationStart = gNow;
        assert(phaseControllerExpirationCaptureNotify() == PHASE_CONTROL_SUCCESS);
        gData[PAT_REAL_PRS] = 5.0F;
        gData[PAT_REAL_FLOW] = lCase == 3U ? -25.0F : 0.0F;
        for (unsigned int lIndex = 0U; lIndex < 10U; lIndex++, gNow += 6U) {
            phaseControllerProcess(gNow);
            triggerEngineProcess(gNow);
        }
        gData[PAT_REAL_FLOW] = lCase == 3U ? -14.0F : 1.1F;
        gData[PAT_REAL_PRS] = 3.9F;
        for (; gNow < lExpirationStart + BREATH_PEEP_LOCK_TIME_MS; gNow += 6U) {
            phaseControllerProcess(gNow);
            triggerEngineProcess(gNow);
            assert(phaseControllerStateGet() == PHASE_EXP);
        }
        assert(phaseControllerActivePlanGet(&lPlan) == PHASE_CONTROL_SUCCESS);
        for (; gNow <= lExpirationStart + lPlan.expiratoryTimeMs + 6U; gNow += 6U) {
            phaseControllerProcess(gNow);
            triggerEngineProcess(gNow);
            if (phaseControllerStateGet() == PHASE_INSP) { break; }
        }
        assert(phaseControllerStateGet() == PHASE_INSP);
        assert(phaseControllerActivePlanGet(&lPlan) == PHASE_CONTROL_SUCCESS);
        assert(lPlan.mode == VENT_MD_VAC);
        assert(lPlan.breathType == BREATH_TYPE_MANDATORY_VOLUME);
        assert(lPlan.triggerReason == (lType == VENT_TRIGGER_OFF ? BREATH_TRIGGER_REASON_TIME :
               lType == VENT_TRIGGER_FLOW ? BREATH_TRIGGER_REASON_FLOW : BREATH_TRIGGER_REASON_PRESSURE));
        near(lPlan.targetTidalVolumeMl, 500.0F);
        assert(lPlan.timeTriggerEnabled == 1U && lPlan.cycleType == BREATH_CYCLE_TYPE_TIME);
        lInspirationStart = gNow;
        gData[PAT_REAL_FLOW] = 0.0F;
        gData[PAT_REAL_PRS] = 5.0F;
        phaseControllerProcess(gNow + 100U);
        assert(phaseControlGet(PHASE_REF_FLOW) > 0.0F);
        gNow = lInspirationStart + lPlan.maximumInspiratoryTimeMs;
        phaseControllerProcess(gNow);
        assert(phaseControllerStateGet() == PHASE_EXP);
        lExpirationStart = gNow;
        phaseControllerProcess(lExpirationStart + lPlan.expiratoryTimeMs - 1U);
        assert(phaseControllerStateGet() == PHASE_EXP);
        phaseControllerProcess(lExpirationStart + lPlan.expiratoryTimeMs);
        assert(phaseControllerStateGet() == PHASE_INSP);
        assert(phaseControllerActivePlanGet(&lPlan) == PHASE_CONTROL_SUCCESS);
        assert(lPlan.triggerReason == BREATH_TRIGGER_REASON_TIME);
        assert(lPlan.breathType == BREATH_TYPE_MANDATORY_VOLUME);
    }
}

int main(void) {
    testVacTrigger();
    (void)testTimeCompensation;
#if BREATH_VOLUME_FLOW_COMPENSATION_ENABLE
    testEma();
    testReset();
    testConvergence();
    testIntegration();
    testShortVolumeReference();
#else
    (void)testEma; (void)testReset; (void)testConvergence;
    (void)testIntegration; (void)testShortVolumeReference;
    testTimeCompensation();
    testReset();
    testIntegration();
    testShortVolumeReference();
#endif
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
                   "user/app/ventlogic/triggerengine.c",
                   "user/app/ventlogic/monitorengine.c", "user/app/ventalgo/flowcontroller.c",
                   "user/app/databus/settingdata.c", "user/tools/controller/pid.c"]
        command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                   *[f"-I{ROOT / path}" for path in includes], str(harness),
                   *[str(ROOT / path) for path in sources], "-o", str(executable)]
        environment = os.environ.copy()
        environment["PATH"] = str(Path(compiler).parent) + os.pathsep + environment["PATH"]
        for legacy in (0, 1):
            subprocess.run(command + [f"-DBREATH_VOLUME_FLOW_COMPENSATION_ENABLE={legacy}"], check=True, env=environment)
            subprocess.run([str(executable)], check=True, env=environment)
            print(f"PASS: {'legacy flow' if legacy else 'time'} compensation")
    print("PASS: EMA, next-breath timing, once-only feedback, conversion, bounds, convergence, faults, reset")


if __name__ == "__main__":
    main()
