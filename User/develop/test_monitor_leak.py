"""Exercise production leak monitoring and VAC pause control with host inputs."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HARNESS = r'''
/************************************************************************************
* @file     : monitor_leak_test.c
* @brief    : Deterministic leak estimator and pause integration regression.
***********************************************************************************/
#include <assert.h>
#include <math.h>
#include "monitorengine.h"
#include "flowcontroller.h"
#include "controldata.h"
#include "databus.h"
#include "phasecontroller.h"
#include "calibtrans.h"
#include "rtos.h"

static float gData[CONTROL_DATA_COUNT];
static float gOffset;
static uint32_t gNow;
static ePhaseControllerState gPhase;
static uint8_t gPause;
static stVentLimitSettings gLimits = {.pressureLow = 1.0F, .pressureHigh = 60.0F};
static stBreathPlan gPlan;

float controlDataGet(ControlData_Index_EnumDef index) { return gData[index]; }
float controlDataMdiffFlowZeroOffsetGet(void) { return gOffset; }
ePhaseControllerState phaseControllerStateGet(void) { return gPhase; }
uint8_t phaseControllerVolumePauseActiveGet(void) { return gPause; }
int8_t phaseControllerActivePlanGet(stBreathPlan *plan) {
    *plan = gPlan;
    return PHASE_CONTROL_SUCCESS;
}
eBreathCycleReason phaseControllerCycleReasonGet(void) { return BREATH_CYCLE_REASON_TIME; }
float phaseControlGet(ePhaseControlType type) { (void)type; return 30.0F; }
int8_t calibtransPrsSpeed(float pressure, float *speed) {
    *speed = pressure * 10.0F;
    return CALIBTRANS_STATUS_OK;
}
void repRtosEnterCritical(void) {}
void repRtosExitCritical(void) {}
void breathSchedulerVolumeReset(void) {}
void breathSchedulerVolumeFeedback(const stBreathPlan *plan, float vtiMl, uint8_t valid) {
    (void)plan;
    (void)vtiMl;
    (void)valid;
}

/** Advance one nominal 6 ms measurement, independently of estimated flow. */
static void sample(ePhaseControllerState phase, float flow, float pressure) {
    gPhase = phase;
    gData[MDIFF_REAL_FLOW] = flow;
    gData[PAT_REAL_PRS] = pressure;
    gNow += 6U;
    monitorEngineProcess(gNow);
}

/** Restore independent test state. */
static void reset(void) {
    gNow = 0U;
    gOffset = 0.0F;
    gPause = 0U;
    gPlan = (stBreathPlan){.sequence = 1U, .mode = VENT_MD_VAC,
        .breathType = BREATH_TYPE_MANDATORY_VOLUME, .targetTidalVolumeMl = 500.0F, .peepCmh2o = 5.0F, .inspiratoryFlowLpm = 30.0F,
        .maximumInspiratoryTimeMs = 1000U, .limitSettings = &gLimits};
    monitorEngineInit();
    flowControllerInit();
}

/** Complete a balanced lung cycle with analytically known downstream K=2. */
static void knownLeak(void) {
    reset();
    sample(PHASE_INSP, 30.0F, 25.0F); /* Lung +20, leak +10. */
    sample(PHASE_EXP, -14.0F, 9.0F); /* Lung -20, leak +6. */
    assert(monitorEngineGet(MONITOR_LEAK_VALID) == 0.0F);
    gPlan.sequence++;
    sample(PHASE_INSP, 10.0F, 25.0F);
    assert(fabsf(monitorEngineGet(MONITOR_LEAK_COEFFICIENT) - 2.0F) < 0.001F);
    assert(fabsf(monitorEngineGet(MONITOR_LEAK_FLOW) - 10.0F) < 0.001F);
}

int main(void) {
    stBreathResult lResult;
    stActuatorRequest lRequest;
    uint16_t lTarget;
    unsigned int lIndex;
    knownLeak();
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert(lResult.sequence == 1U);
    /* Real estimator output is shared by pause control and plateau detection. */
    gPause = 1U;
    gData[INSP_FLOW_FILTERED] = 80.0F;
    for (lIndex = 0U; lIndex < 100U; lIndex++) {
        sample(PHASE_INSP, 10.0F, 25.0F);
        assert(flowControllerProcess(&gPlan, &lRequest) == ACTUATOR_REQUEST_SUCCESS);
    }
    assert(monitorEngineGet(MONITOR_PLATEAU_PRS) == 25.0F);
    lTarget = lRequest.blowerTarget;
    gData[INSP_FLOW_FILTERED] = 0.0F;
    assert(flowControllerProcess(&gPlan, &lRequest) == ACTUATOR_REQUEST_SUCCESS);
    assert(lTarget == lRequest.blowerTarget);

    /* Leak masks reverse lung flow but must not mask a breath boundary. */
    reset();
    sample(PHASE_INSP, 12.0F, 25.0F);
    sample(PHASE_EXP, 8.0F, 25.0F);
    gPlan.sequence++;
    sample(PHASE_INSP, 12.0F, 25.0F);
    assert(fabsf(monitorEngineGet(MONITOR_LEAK_COEFFICIENT) - 2.0F) < 0.001F);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert(lResult.sequence == 1U && lResult.inspiratoryTimeMs == 6U);

    /* Negative balance stays diagnostic; both consumers use zero compensation. */
    reset();
    sample(PHASE_INSP, 10.0F, 25.0F);
    sample(PHASE_EXP, -20.0F, 25.0F);
    gPlan.sequence++;
    gPause = 1U;
    sample(PHASE_INSP, 0.0F, 25.0F);
    assert(fabsf(monitorEngineGet(MONITOR_LEAK_BALANCE_COEFFICIENT) + 1.0F) < 0.001F);
    assert(monitorEngineGet(MONITOR_LEAK_COEFFICIENT) == 0.0F);
    assert(monitorEngineGet(MONITOR_LEAK_FLOW) == 0.0F);
    assert(monitorEngineGet(MONITOR_PLATEAU_PRS) == 25.0F);

    /* Bad samples reject the whole cycle, including any previously valid K. */
    for (lIndex = 0U; lIndex < 4U; lIndex++) {
        knownLeak();
        sample(PHASE_EXP, (lIndex == 3U) ? NAN : -10.0F,
               (lIndex == 0U) ? 0.0F : (lIndex == 1U) ? -1.0F :
               (lIndex == 2U) ? NAN : 25.0F);
        gPlan.sequence++;
        sample(PHASE_INSP, 10.0F, 25.0F);
        assert(monitorEngineGet(MONITOR_LEAK_VALID) == 0.0F);
        assert(monitorEngineGet(MONITOR_LEAK_FLOW) == 0.0F);
    }

    /* Stop mid-inspiration; initial expiration must not join the old cycle. */
    knownLeak();
    sample(PHASE_IDLE, 0.0F, 0.0F);
    assert(monitorEngineGet(MONITOR_LEAK_FLOW) == 0.0F);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_ERROR_STATE);
    sample(PHASE_COMPEN, 0.0F, 0.0F);
    sample(PHASE_EXP, -100.0F, 25.0F);
    gPlan.sequence++;
    sample(PHASE_INSP, 10.0F, 25.0F);
    assert(monitorEngineGet(MONITOR_LEAK_VALID) == 0.0F);
    sample(PHASE_EXP, -10.0F, 25.0F);
    gPlan.sequence++;
    sample(PHASE_INSP, 0.0F, 25.0F);
    assert(monitorEngineGet(MONITOR_LEAK_VALID) == 1.0F);
    assert(monitorEngineGet(MONITOR_LEAK_FLOW) == 0.0F);

    /* Re-zeroing discards both previous data and the remaining partial cycle. */
    knownLeak();
    gOffset = 1.0F;
    sample(PHASE_INSP, 50.0F, 25.0F);
    sample(PHASE_INSP, 50.0F, 25.0F);
    sample(PHASE_EXP, -10.0F, 25.0F);
    gPlan.sequence++;
    sample(PHASE_INSP, 10.0F, 25.0F);
    assert(monitorEngineGet(MONITOR_LEAK_VALID) == 0.0F);
    sample(PHASE_EXP, -10.0F, 25.0F);
    gPlan.sequence++;
    sample(PHASE_INSP, 0.0F, 25.0F);
    assert(monitorEngineGet(MONITOR_LEAK_VALID) == 1.0F);
    assert(monitorEngineGet(MONITOR_LEAK_FLOW) == 0.0F);

    /* Sequence changes without expiration cannot publish a complete cycle. */
    reset();
    sample(PHASE_INSP, 100.0F, 25.0F);
    gPlan.sequence++;
    sample(PHASE_INSP, 100.0F, 25.0F);
    assert(monitorEngineGet(MONITOR_LEAK_VALID) == 0.0F);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_ERROR_STATE);

    reset();
    sample(PHASE_INSP, 300.0F, 25.0F);
    sample(PHASE_EXP, 300.0F, 25.0F);
    gPlan.sequence++;
    sample(PHASE_INSP, 0.0F, 25.0F);
    assert(monitorEngineGet(MONITOR_LEAK_COEFFICIENT) == MONITOR_LEAK_COEFFICIENT_MAX);
    assert(monitorEngineGet(MONITOR_LEAK_FLOW) == MONITOR_PATIENT_LEAK_FLOW_MAX_LPM);
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
    with tempfile.TemporaryDirectory(prefix="ventcore-monitor-leak-") as directory:
        harness = Path(directory) / "monitor_leak_test.c"
        harness.write_text(HARNESS, encoding="utf-8", newline="\n")
        executable = Path(directory) / "monitor_leak_test.exe"
        includes = ["user/app/ventlogic", "user/app/ventalgo", "user/app/databus",
                    "user/app/calibration", "user/module/rtos", "user/tools/controller"]
        command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                   *[f"-I{ROOT / path}" for path in includes], str(harness),
                   str(ROOT / "user/app/ventlogic/monitorengine.c"),
                   str(ROOT / "user/app/ventalgo/flowcontroller.c"),
                   str(ROOT / "user/tools/controller/pid.c"), "-o", str(executable)]
        environment = os.environ.copy()
        environment["PATH"] = str(Path(compiler).parent) + os.pathsep + environment["PATH"]
        subprocess.run(command, check=True, env=environment)
        subprocess.run([str(executable)], check=True, env=environment)
    print("PASS: leak estimate, pause integration, boundaries, invalid data, restart, re-zero, limits")


if __name__ == "__main__":
    main()
