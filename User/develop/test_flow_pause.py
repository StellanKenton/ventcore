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
#include "controldata.h"
#include "monitorengine.h"
#include "phasecontroller.h"
#include "calibtrans.h"

static float gData[CONTROL_DATA_COUNT];
static float gRefs[PHASE_COUNT];
static float gLeak;
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
uint8_t phaseControllerVolumePauseActiveGet(void) { return gPause; }
ePhaseControllerState phaseControllerStateGet(void) { return gPhase; }
int8_t calibtransPrsSpeed(float pressureValue, float *speedRps) {
    *speedRps = pressureValue * 10.0F;
    return gCalibrationStatus;
}

/** Run a successful cycle and return its blower request. */
static uint16_t testStep(const stBreathPlan *plan) {
    stActuatorRequest lRequest;
    assert(flowControllerProcess(plan, &lRequest) == ACTUATOR_REQUEST_SUCCESS);
    return lRequest.blowerTarget;
}

/** Verify zero-flow control, transition limits, delayed feedback and failure handling. */
int main(void) {
    stVentLimitSettings lLimits = {.pressureLow = 1.0F, .pressureHigh = 60.0F};
    stBreathPlan lPlan = {.sequence = 1U, .mode = VENT_MD_VAC,
        .breathType = BREATH_TYPE_MANDATORY_VOLUME,
        .inspiratoryFlowLpm = 30.0F, .limitSettings = &lLimits};
    stActuatorRequest lRequest;
    uint16_t lPrevious, lTarget, lZeroFlowTarget;
    unsigned int lIndex;

    flowControllerInit();
    assert(flowControllerPauseSettledGet() == 0U);
    gRefs[PHASE_REF_FLOW] = 30.0F;
    gData[INSP_FLOW_FILTERED] = 30.0F;
    gData[MDIFF_REAL_FLOW] = 30.0F;
    gData[PAT_REAL_PRS] = 28.0F;
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
    gData[MDIFF_REAL_FLOW] = 0.0F;
    gData[INSP_FLOW_FILTERED] = 0.0F;
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
    gData[MDIFF_REAL_FLOW] = -2.0F;
    for (lIndex = 0U; lIndex < 50U; lIndex++) { lTarget = testStep(&lPlan); }
    assert(lTarget > lZeroFlowTarget);
    gData[MDIFF_REAL_FLOW] = 2.0F;
    for (lIndex = 0U; lIndex < 100U; lIndex++) { lTarget = testStep(&lPlan); }
    assert(lTarget < lZeroFlowTarget);
    /* Supply flow is not patient flow and must not become a pause flow target. */
    gData[MDIFF_REAL_FLOW] = 0.0F;
    for (lIndex = 0U; lIndex < 10U; lIndex++) { lTarget = testStep(&lPlan); }
    lPrevious = lTarget;
    gData[INSP_FLOW_FILTERED] = 10.0F;
    for (lIndex = 0U; lIndex < 10U; lIndex++) { lTarget = testStep(&lPlan); }
    assert(abs((int)lTarget - (int)lPrevious) <= 1);
    /* Downstream leakage can still be tracked as proximal through-flow. */
    gLeak = 2.0F;
    gData[MDIFF_REAL_FLOW] = 2.0F;
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
    gData[INSP_FLOW_FILTERED] = 30.0F;
    gData[PAT_REAL_PRS] = 15.0F;
    assert(testStep(&lPlan) < 300U);
    /* High-pressure pause must also track leakage without chasing supply flow. */
    lPlan.sequence++;
    gLeak = 2.0F;
    gData[PAT_REAL_PRS] = 45.0F;
    (void)testStep(&lPlan);
    gPause = 1U;
    gData[MDIFF_REAL_FLOW] = gLeak;
    for (lIndex = 0U; lIndex < 100U; lIndex++) { lTarget = testStep(&lPlan); }
    assert(flowControllerPauseSettledGet() != 0U);
    lZeroFlowTarget = lTarget;
    gData[MDIFF_REAL_FLOW] = 0.0F;
    for (lIndex = 0U; lIndex < 50U; lIndex++) { lTarget = testStep(&lPlan); }
    assert(lTarget > lZeroFlowTarget);
    gData[MDIFF_REAL_FLOW] = gLeak;
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
                   str(ROOT / "User/tools/controller/pid.c"), "-o", str(executable)]
        environment = os.environ.copy()
        environment["PATH"] = str(Path(compiler).parent) + os.pathsep + environment["PATH"]
        subprocess.run(command, check=True, env=environment)
        subprocess.run([str(executable)], check=True, env=environment)
    print("PASS: entry, delayed-tail integral gating, reverse flow, flow-source distinction, limits, faults, reset")


if __name__ == "__main__":
    main()
