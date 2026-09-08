"""Test the production flow controller and PID with scripted host sensor inputs."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HARNESS = r'''
/************************************************************************************
* @file     : flow_pause_test.c
* @brief    : Host regression with deterministic sensor and calibration stubs.
***********************************************************************************/
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include "flowcontroller.h"
#include "pressurecontroller.h"
#include "controldata.h"
#include "monitorengine.h"
#include "phasecontroller.h"
#include "calibtrans.h"

static float gData[CONTROL_DATA_COUNT];
static float gRefs[PHASE_COUNT];
static float gLeak;
static float gCapturedFeedforward;
static uint8_t gCaptureFeedforward;
static uint8_t gPause;
static ePhaseControllerState gPhase = PHASE_INSP;
static int8_t gCalibrationStatus = CALIBTRANS_STATUS_OK;

float controlDataGet(ControlData_Index_EnumDef index) { return gData[index]; }
float phaseControlGet(ePhaseControlType type) { return gRefs[type]; }
int8_t phaseControlSet(ePhaseControlType type, float value) {
    gRefs[type] = value;
    return PHASE_CONTROL_SUCCESS;
}
float monitorEngineGet(eMonitorDataType type) { (void)type; return gLeak; }
void monitorEngineVolumeLimitedNotify(void) {}
uint8_t phaseControllerVolumePauseActiveGet(void) { return gPause; }
ePhaseControllerState phaseControllerStateGet(void) { return gPhase; }
int8_t calibtransPrsSpeed(float pressureValue, float *speedRps) {
    if (gCaptureFeedforward != 0U) {
        gCapturedFeedforward = pressureValue;
        gCaptureFeedforward = 0U;
    }
    *speedRps = pressureValue * 10.0F;
    return gCalibrationStatus;
}

/** PAC ignores alarm-high changes; PSV modes retain both pressure caps. */
static void testPressureAlarmLimit(void) {
    const eVentMode lModes[] = {VENT_MD_PAC, VENT_MD_CPAP_PSV, VENT_MD_PSV_ST};
    stVentLimitSettings lLimits = {.pressureLow = 1.0F, .pressureHigh = 20.0F};
    stBreathPlan lPlan = {.sequence = 1U, .mode = VENT_MD_PAC,
        .breathType = BREATH_TYPE_MANDATORY_PRESSURE, .peepCmh2o = 5.0F,
        .inspiratoryPressureCmh2o = 30.0F, .maximumInspiratoryTimeMs = 1000U,
        .pressureLimitCmh2o = 15.0F, .limitSettings = &lLimits};
    stActuatorRequest lRequest;
    stPressureControllerDiagnostic lDiagnostic;
    gData[PAT_REAL_PRS] = 30.0F;
    gData[INSP_REAL_PRS] = 30.0F;
    for (unsigned lMode = 0U; lMode < 3U; lMode++) {
        lPlan.mode = lModes[lMode];
        gData[PAT_REAL_PRS] = lMode == 0U ? 30.0F : 0.0F;
        lLimits.pressureHigh = 20.0F;
        pressureControllerInit();
        for (unsigned lStep = 0U; lStep < 200U; lStep++) {
            assert(pressureControllerProcess(&lPlan, &lRequest) == ACTUATOR_REQUEST_SUCCESS);
        }
        pressureControllerDiagnosticGet(&lDiagnostic);
        assert(fabsf(lDiagnostic.inspTarget - (lMode == 0U ? 30.0F : 15.0F)) < 0.001F);
        lLimits.pressureHigh = 10.0F;
        assert(pressureControllerProcess(&lPlan, &lRequest) == ACTUATOR_REQUEST_SUCCESS);
        pressureControllerDiagnosticGet(&lDiagnostic);
        assert(fabsf(lDiagnostic.inspTarget - (lMode == 0U ? 30.0F : 10.0F)) < 0.001F);
    }
    gData[PAT_REAL_PRS] = 0.0F;
    gData[INSP_REAL_PRS] = 0.0F;
    gRefs[PHASE_REF_PRESSURE] = 0.0F;
}

/** Verify PAC terminal handoff, regulation, mode isolation and breath reset. */
static void testPacSettledHold(void) {
    const eVentMode lModes[] = {VENT_MD_PAC, VENT_MD_CPAP_PSV, VENT_MD_PSV_ST};
    stVentLimitSettings lLimits = {.pressureLow = 1.0F, .pressureHigh = 60.0F};
    stBreathPlan lPlan = {.sequence = 1U, .mode = VENT_MD_PAC,
        .breathType = BREATH_TYPE_MANDATORY_PRESSURE, .peepCmh2o = 5.0F,
        .inspiratoryPressureCmh2o = 30.0F, .riseTimeMs = 200U,
        .maximumInspiratoryTimeMs = 1350U, .pressureLimitCmh2o = 60.0F,
        .limitSettings = &lLimits};
    stActuatorRequest lRequest;
    stPressureControllerDiagnostic lDiagnostic;
    uint16_t lTrackedSpeed;
    uint16_t lInitialTarget;

    for (unsigned lMode = 0U; lMode < 3U; lMode++) {
        lPlan.mode = lModes[lMode];
        gData[PAT_REAL_PRS] = 31.0F;
        gData[INSP_REAL_PRS] = 31.0F;
        gData[PAT_REAL_FLOW] = 20.0F;
        gData[INSP_REAL_FLOW] = 20.0F;
        pressureControllerInit();
        for (unsigned lStep = 0U; lStep < 100U; lStep++) {
            assert(pressureControllerProcess(&lPlan, &lRequest) == ACTUATOR_REQUEST_SUCCESS);
        }
        pressureControllerDiagnosticGet(&lDiagnostic);
        assert(lDiagnostic.flowCompensation > 1.0F);
        lTrackedSpeed = lRequest.blowerTarget + 12U;
        gData[RAW_BLOWER_SPEED] = (float)lTrackedSpeed;
        gData[PAT_REAL_FLOW] = 2.0F;
        assert(pressureControllerProcess(&lPlan, &lRequest) == ACTUATOR_REQUEST_SUCCESS);
        pressureControllerDiagnosticGet(&lDiagnostic);
        if (lMode != 0U) {
            assert(lDiagnostic.flowCompensation > 1.0F);
            continue;
        }
        assert(abs((int)lRequest.blowerTarget - (int)lTrackedSpeed) <= 1);
        assert(lDiagnostic.flowCompensation == 0.0F);
        /* Residual supply flow must not reintroduce withdrawn feedforward. */
        gData[INSP_REAL_FLOW] = 40.0F;
        gData[PAT_REAL_FLOW] = 11.0F;
        assert(pressureControllerProcess(&lPlan, &lRequest) == ACTUATOR_REQUEST_SUCCESS);
        pressureControllerDiagnosticGet(&lDiagnostic);
        assert(lDiagnostic.flowCompensation == 0.0F);
        gData[PAT_REAL_PRS] = 29.0F;
        gData[INSP_REAL_PRS] = 29.0F;
        assert(pressureControllerProcess(&lPlan, &lRequest) == ACTUATOR_REQUEST_SUCCESS);
        lInitialTarget = lRequest.blowerTarget;
        for (unsigned lStep = 0U; lStep < 40U; lStep++) {
            assert(pressureControllerProcess(&lPlan, &lRequest) == ACTUATOR_REQUEST_SUCCESS);
        }
        assert(lRequest.blowerTarget > lInitialTarget);
        gData[PAT_REAL_PRS] = 32.0F;
        gData[INSP_REAL_PRS] = 32.0F;
        for (unsigned lStep = 0U; lStep < 1000U; lStep++) {
            assert(pressureControllerProcess(&lPlan, &lRequest) == ACTUATOR_REQUEST_SUCCESS);
            assert(lRequest.blowerTarget <= 800U);
        }
        assert(lRequest.blowerTarget < lInitialTarget);
        assert(lRequest.expiratoryValveDuty < 100U);
        /* Feedforward saturation must not accumulate a hidden integral backlog. */
        gData[PAT_REAL_PRS] = 0.0F;
        gData[INSP_REAL_PRS] = 0.0F;
        for (unsigned lStep = 0U; lStep < 2000U; lStep++) {
            assert(pressureControllerProcess(&lPlan, &lRequest) == ACTUATOR_REQUEST_SUCCESS);
            assert(lRequest.blowerTarget <= 800U);
        }
        assert(lRequest.blowerTarget == 800U);
        gData[PAT_REAL_PRS] = 40.0F;
        gData[INSP_REAL_PRS] = 40.0F;
        assert(pressureControllerProcess(&lPlan, &lRequest) == ACTUATOR_REQUEST_SUCCESS);
        assert(lRequest.blowerTarget < 750U);
        lPlan.sequence++;
        gData[PAT_REAL_FLOW] = 20.0F;
        gData[PAT_REAL_PRS] = 5.0F;
        gData[INSP_REAL_PRS] = 5.0F;
        assert(pressureControllerProcess(&lPlan, &lRequest) == ACTUATOR_REQUEST_SUCCESS);
        assert(pressureControllerStateGet() == PRESSURE_CONTROLLER_INSP_RISE);
        pressureControllerDiagnosticGet(&lDiagnostic);
        assert(lDiagnostic.flowCompensation > 0.0F);
        gCalibrationStatus = CALIBTRANS_ERROR_NOT_READY;
        assert(pressureControllerProcess(&lPlan, &lRequest) == ACTUATOR_REQUEST_ERROR_STATE);
        assert(lRequest.validMask == 0U);
        gCalibrationStatus = CALIBTRANS_STATUS_OK;
    }
    /* Reject unavailable/nonfinite speed; bound even a plausible large mismatch. */
    lPlan.mode = VENT_MD_PAC;
    for (unsigned lCase = 0U; lCase < 3U; lCase++) {
        pressureControllerInit();
        gData[PAT_REAL_PRS] = 30.0F;
        gData[INSP_REAL_PRS] = 30.0F;
        gData[PAT_REAL_FLOW] = 20.0F;
        gData[INSP_REAL_FLOW] = 20.0F;
        for (unsigned lStep = 0U; lStep < 100U; lStep++) {
            assert(pressureControllerProcess(&lPlan, &lRequest) == ACTUATOR_REQUEST_SUCCESS);
        }
        lInitialTarget = lRequest.blowerTarget;
        gData[RAW_BLOWER_SPEED] = lCase == 0U ? 0.0F : lCase == 1U ? NAN : 800.0F;
        gData[PAT_REAL_FLOW] = 0.0F;
        assert(pressureControllerProcess(&lPlan, &lRequest) == ACTUATOR_REQUEST_SUCCESS);
        assert(abs((int)lRequest.blowerTarget - (int)lInitialTarget) <= (lCase == 2U ? 40 : 1));
    }
    for (unsigned lIndex = 0U; lIndex < CONTROL_DATA_COUNT; lIndex++) {
        gData[lIndex] = 0.0F;
    }
    gRefs[PHASE_REF_PRESSURE] = 0.0F;
}

/** Run a successful cycle and return its blower request. */
static uint16_t testStep(const stBreathPlan *plan) {
    stActuatorRequest lRequest;
    assert(flowControllerProcess(plan, &lRequest) == ACTUATOR_REQUEST_SUCCESS);
    return lRequest.blowerTarget;
}

/** Check the exact VAC pressure model independently of PID output. */
static void testVacFeedforward(void) {
    const float lVolumes[] = {300.0F, 420.0F, 500.0F, 1400.0F, 1500.0F};
    const float lCompliance[] = {30.0F, 30.0F, 30.0F, 30.0F, 30.0F};
    stVentLimitSettings lLimits = {.pressureLow = 1.0F, .pressureHigh = 60.0F};
    stBreathPlan lPlan = {.sequence = 1U, .mode = VENT_MD_VAC,
        .breathType = BREATH_TYPE_MANDATORY_VOLUME, .peepCmh2o = 5.0F,
        .inspiratoryFlowLpm = 30.0F, .limitSettings = &lLimits};
    stActuatorRequest lRequest;
    unsigned int lIndex;
    float lExpected;

    gRefs[PHASE_REF_FLOW] = 30.0F;
    gData[INSP_REAL_FLOW] = 30.0F;
    gData[PAT_REAL_FLOW] = 30.0F;
    gData[PAT_REAL_PRS] = 40.0F;
    for (lIndex = 0U; lIndex < 5U; lIndex++) {
        flowControllerInit();
        lPlan.targetTidalVolumeMl = lVolumes[lIndex];
        lPlan.deliveryTargetMl = lVolumes[lIndex] + 100.0F;
        gRefs[PHASE_REF_VOLUME] = 0.5F * lVolumes[lIndex];
        gCaptureFeedforward = 1U;
        (void)testStep(&lPlan);
        lExpected = 5.0F + 0.5F * (lVolumes[lIndex] + 100.0F) /
                    lCompliance[lIndex] + 8.3277F;
        assert(fabsf(gCapturedFeedforward - lExpected) < 0.001F);
        /* Actual pressure is not added on top of the elastic model. */
        gData[PAT_REAL_PRS] = 10.0F;
        gCaptureFeedforward = 1U;
        (void)testStep(&lPlan);
        assert(fabsf(gCapturedFeedforward - lExpected) < 0.001F);
    }
    gRefs[PHASE_REF_VOLUME] = 0.0F;
    gCaptureFeedforward = 1U;
    (void)testStep(&lPlan);
    assert(fabsf(gCapturedFeedforward - 13.3277F) < 0.001F);
    gRefs[PHASE_REF_VOLUME] = 3000.0F;
    gCaptureFeedforward = 1U;
    (void)testStep(&lPlan);
    assert(fabsf(gCapturedFeedforward - 60.0F) < 0.001F);
    lLimits.pressureHigh = 8.0F;
    gCaptureFeedforward = 1U;
    (void)testStep(&lPlan);
    assert(gCapturedFeedforward == 8.0F);
    gRefs[PHASE_REF_VOLUME] = NAN;
    assert(flowControllerProcess(&lPlan, &lRequest) == ACTUATOR_REQUEST_ERROR_STATE);
    gRefs[PHASE_REF_VOLUME] = 0.0F;
}

/** Verify zero-flow control, transition limits, delayed feedback and failure handling. */
int main(void) {
    testPressureAlarmLimit();
    testPacSettledHold();
    stVentLimitSettings lLimits = {.pressureLow = 1.0F, .pressureHigh = 60.0F};
    stBreathPlan lPlan = {.sequence = 1U, .mode = VENT_MD_VAC,
        .breathType = BREATH_TYPE_MANDATORY_VOLUME, .targetTidalVolumeMl = 500.0F,
        .deliveryTargetMl = 500.0F, .peepCmh2o = 5.0F,
        .inspiratoryFlowLpm = 30.0F, .limitSettings = &lLimits};
    stActuatorRequest lRequest;
    uint16_t lPrevious, lTarget, lZeroFlowTarget;
    unsigned int lIndex;

    testVacFeedforward();
    flowControllerInit();
    assert(flowControllerPauseSettledGet() == 0U);
    gRefs[PHASE_REF_FLOW] = 30.0F;
    gData[INSP_REAL_FLOW] = 30.0F;
    gData[PAT_REAL_FLOW] = 30.0F;
    gData[PAT_REAL_FLOW] = 30.0F;
    gData[PAT_REAL_PRS] = 28.0F;
    lTarget = testStep(&lPlan);
    gData[INSP_REAL_FLOW] = 0.0F;
    assert(testStep(&lPlan) == lTarget);
    gData[PAT_REAL_FLOW] = 20.0F;
    assert(testStep(&lPlan) > lTarget);
    flowControllerInit();
    gData[INSP_REAL_FLOW] = 30.0F;
    gData[PAT_REAL_FLOW] = 30.0F;
    lTarget = testStep(&lPlan);
    gPause = 1U;
    gLeak = 0.0F;
    /* Delayed delivery flow must not cause an unbounded command discontinuity. */
    for (lIndex = 0U; lIndex < FLOW_CONTROLLER_PAUSE_SETTLE_SAMPLES; lIndex++) {
        lPrevious = lTarget;
        lTarget = testStep(&lPlan);
        assert(abs((int)lPrevious - (int)lTarget) <= FLOW_CONTROLLER_PAUSE_SPEED_STEP_MAX);
    }
    /* A tail longer than 120 ms must still not accumulate a negative integral. */
    lPrevious = lTarget;
    for (lIndex = 0U; lIndex < 100U; lIndex++) { lTarget = testStep(&lPlan); }
    assert(abs((int)lTarget - (int)lPrevious) <= 1);
    assert(flowControllerPauseSettledGet() == 0U);
    gData[PAT_REAL_PRS] = 20.0F;
    gData[PAT_REAL_FLOW] = 0.0F;
    gData[INSP_REAL_FLOW] = 0.0F;
    for (lIndex = 0U; lIndex < 400U; lIndex++) {
        lPrevious = lTarget;
        lTarget = testStep(&lPlan);
        assert(abs((int)lPrevious - (int)lTarget) <= FLOW_CONTROLLER_PAUSE_SPEED_STEP_MAX);
    }
    /* The controller must not install a pressure-hold reference. */
    assert(flowControllerPauseSettledGet() != 0U);
    gData[PAT_REAL_PRS] = 19.0F;
    assert(gRefs[PHASE_REF_PRESSURE] == 0.0F);
    /* Compare flow corrections at the same settled pressure baseline. */
    for (lIndex = 0U; lIndex < 400U; lIndex++) { lTarget = testStep(&lPlan); }
    lZeroFlowTarget = lTarget;
    /* Real reverse flow must raise output, including sustained integral correction. */
    gData[PAT_REAL_FLOW] = -2.0F;
    for (lIndex = 0U; lIndex < 50U; lIndex++) { lTarget = testStep(&lPlan); }
    assert(lTarget > lZeroFlowTarget);
    gData[PAT_REAL_FLOW] = 2.0F;
    for (lIndex = 0U; lIndex < 100U; lIndex++) { lTarget = testStep(&lPlan); }
    assert(lTarget < lZeroFlowTarget);
    /* Supply flow is not patient flow and must not become a pause flow target. */
    gData[PAT_REAL_FLOW] = 0.0F;
    for (lIndex = 0U; lIndex < 10U; lIndex++) { lTarget = testStep(&lPlan); }
    lPrevious = lTarget;
    gData[INSP_REAL_FLOW] = 10.0F;
    for (lIndex = 0U; lIndex < 10U; lIndex++) { lTarget = testStep(&lPlan); }
    assert(abs((int)lTarget - (int)lPrevious) <= 1);
    /* Downstream leakage can still be tracked as proximal through-flow. */
    gLeak = 2.0F;
    gData[PAT_REAL_FLOW] = 2.0F;
    for (lIndex = 0U; lIndex < 10U; lIndex++) { lTarget = testStep(&lPlan); }
    lPrevious = lTarget;
    for (lIndex = 0U; lIndex < 40U; lIndex++) { lTarget = testStep(&lPlan); }
    assert(abs((int)lTarget - (int)lPrevious) <= 1);
    /* A live absolute pressure limit takes priority over slew limiting. */
    lLimits.pressureHigh = 5.0F;
    assert(testStep(&lPlan) <= 50U);
    gData[PAT_REAL_PRS] = NAN;
    assert(flowControllerProcess(&lPlan, &lRequest) == ACTUATOR_REQUEST_ERROR_STATE);
    assert(lRequest.validMask == 0U);
    gData[PAT_REAL_PRS] = 20.0F;
    gCalibrationStatus = CALIBTRANS_ERROR_NOT_READY;
    assert(flowControllerProcess(&lPlan, &lRequest) == ACTUATOR_REQUEST_ERROR_STATE);
    assert(lRequest.validMask == 0U);
    gCalibrationStatus = CALIBTRANS_STATUS_OK;
    gPhase = PHASE_EXP;
    assert(flowControllerProcess(&lPlan, &lRequest) == ACTUATOR_REQUEST_ERROR_STATE);
    /* A new breath must discard the previous pressure baseline and pause integral. */
    lPlan.sequence++;
    lLimits.pressureHigh = 60.0F;
    gPhase = PHASE_INSP;
    gPause = 0U;
    gData[INSP_REAL_FLOW] = 30.0F;
    gData[PAT_REAL_PRS] = 15.0F;
    assert(testStep(&lPlan) < 300U);
    /* High-pressure pause must also track leakage without chasing supply flow. */
    lPlan.sequence++;
    gLeak = 2.0F;
    gData[PAT_REAL_PRS] = 45.0F;
    (void)testStep(&lPlan);
    gPause = 1U;
    gData[PAT_REAL_FLOW] = gLeak;
    for (lIndex = 0U; lIndex < 100U; lIndex++) { lTarget = testStep(&lPlan); }
    assert(flowControllerPauseSettledGet() != 0U);
    lZeroFlowTarget = lTarget;
    gData[PAT_REAL_FLOW] = 0.0F;
    for (lIndex = 0U; lIndex < 50U; lIndex++) { lTarget = testStep(&lPlan); }
    assert(lTarget > lZeroFlowTarget);
    gData[PAT_REAL_FLOW] = gLeak;
    for (lIndex = 0U; lIndex < 10U; lIndex++) { lTarget = testStep(&lPlan); }
    lPrevious = lTarget;
    for (lIndex = 0U; lIndex < 50U; lIndex++) { lTarget = testStep(&lPlan); }
    assert(abs((int)lTarget - (int)lPrevious) <= 1);
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
    with tempfile.TemporaryDirectory(prefix="ventcore-flow-pause-") as directory:
        harness = Path(directory) / "flow_pause_test.c"
        harness.write_text(HARNESS, encoding="utf-8", newline="\n")
        executable = Path(directory) / "flow_pause_test.exe"
        includes = ["User/app/ventalgo", "User/app/ventlogic", "User/app/databus",
                    "User/app/calibration", "User/tools/controller"]
        command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                   *[f"-I{ROOT / path}" for path in includes], str(harness),
                   str(ROOT / "User/app/ventalgo/flowcontroller.c"),
                   str(ROOT / "User/app/ventalgo/pressurecontroller.c"),
                   str(ROOT / "User/tools/controller/pid.c"), "-o", str(executable)]
        environment = os.environ.copy()
        environment["PATH"] = str(Path(compiler).parent) + os.pathsep + environment["PATH"]
        subprocess.run(command, check=True, env=environment)
        subprocess.run([str(executable)], check=True, env=environment)
    print("PASS: PAC terminal handoff/regulation/reset, alarm-high independence, PSV/ST isolation, VAC entry, delayed-tail integral gating, reverse flow, flow-source distinction, limits, faults")


if __name__ == "__main__":
    main()
