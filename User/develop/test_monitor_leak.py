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
#include "physalarmmanager.h"

static float gData[CONTROL_DATA_COUNT];
static float gOffset;
static uint32_t gNow;
static ePhaseControllerState gPhase;
static uint8_t gPause;
static stVentLimitSettings gLimits = {.pressureLow = 1.0F, .pressureHigh = 60.0F};
static stBreathPlan gPlan;

stVentLimitSettings *GetVentLimitSettings(void) { return &gLimits; }
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

/** Verify PEEP windows, rejected samples, short windows and cycle isolation. */
static void dynamicPeep(void) {
    unsigned int lIndex;

    reset();
    sample(PHASE_INSP, 30.0F, 25.0F);
    sample(PHASE_EXP, 0.0F, 2.0F);
    sample(PHASE_EXP, 0.0F, 5.0F);
    for (lIndex = 1U; lIndex <= 7U; lIndex++) {
        sample(PHASE_EXP, 0.0F, 5.0F + 0.01F * (float)lIndex);
    }
    sample(PHASE_EXP, 0.0F, 9.0F); /* Rejection preserves the window. */
    monitorEngineBreathComplete(gNow);
    assert(fabsf(monitorEngineGet(MONITOR_DYN_PEEP) - 5.05F) < 0.0001F);
    sample(PHASE_EXP, 0.0F, 1.0F); /* Completed cycles are immutable. */
    assert(fabsf(monitorEngineGet(MONITOR_DYN_PEEP) - 5.05F) < 0.0001F);

    /* Fewer than five valid points use this cycle's minimum. */
    for (lIndex = 0U; lIndex < 5U; lIndex++) {
        gPlan.sequence++;
        sample(PHASE_INSP, 30.0F, 25.0F);
        sample(PHASE_EXP, 1.0F, 3.0F);
        sample(PHASE_EXP, 1.0F, 6.0F);
        for (unsigned int lPoint = 0U; lPoint < lIndex; lPoint++) {
            sample(PHASE_EXP, 1.0F, 6.0F);
        }
        monitorEngineBreathComplete(gNow);
        assert(monitorEngineGet(MONITOR_DYN_PEEP) == 3.0F);
    }

    /* Nonconsecutive valid points count; bad pressure breaks adjacency. */
    gPlan.sequence++;
    sample(PHASE_INSP, 30.0F, 25.0F);
    sample(PHASE_EXP, -1.0F, 1.0F);
    for (lIndex = 0U; lIndex < 5U; lIndex++) {
        sample(PHASE_EXP, -1.0F, 5.0F + (float)lIndex);
        sample(PHASE_EXP, -1.0F, 5.0F + (float)lIndex);
        sample(PHASE_EXP, -1.0F, NAN);
    }
    gPlan.sequence++;
    sample(PHASE_INSP, 30.0F, 25.0F); /* Observed completion path. */
    assert(monitorEngineGet(MONITOR_DYN_PEEP) == 7.0F);

    /* The slope threshold is strict in both directions. */
    sample(PHASE_EXP, -1.0F, 0.0F);
    for (lIndex = 0U; lIndex < 8U; lIndex++) {
        sample(PHASE_EXP, -1.0F, (lIndex % 2U == 0U) ? 0.03F : 0.0F);
    }
    monitorEngineBreathComplete(gNow);
    assert(monitorEngineGet(MONITOR_DYN_PEEP) == 0.0F);
    sample(PHASE_IDLE, 0.0F, 0.0F);
    assert(monitorEngineGet(MONITOR_DYN_PEEP) == 0.0F);
}

/** Check inclusive flow limits and strict adjacent flow-rate limits. */
static void dynamicPeepFlow(void) {
    const float lFlows[] = {1.0F, -1.0F, 1.001F, -1.001F,
                           0.0029F, -0.0029F, 0.003F, -0.003F,
                           0.004F, -0.004F, NAN, INFINITY};
    unsigned int lCase;
    unsigned int lIndex;

    for (lCase = 0U; lCase < sizeof(lFlows) / sizeof(lFlows[0]); lCase++) {
        reset();
        sample(PHASE_INSP, 30.0F, 25.0F);
        sample(PHASE_EXP, 0.0F, 2.0F);
        for (lIndex = 0U; lIndex < 12U; lIndex++) {
            float lFlow = lFlows[lCase];
            if ((lCase >= 4U) && ((lIndex % 2U) == 0U)) {
                lFlow = 0.0F;
            }
            sample(PHASE_EXP, lFlow, 5.0F);
        }
        monitorEngineBreathComplete(gNow);
        assert(monitorEngineGet(MONITOR_DYN_PEEP) ==
               ((lCase < 2U || lCase == 4U || lCase == 5U) ? 5.0F : 2.0F));
    }
}

/** Publish a cycle's minimum and enter the next inspiration. */
static void peepAlarmNextBreath(float pressure) {
    sample(PHASE_EXP, 0.0F, pressure);
    monitorEngineBreathComplete(gNow);
    gPlan.sequence++;
    sample(PHASE_INSP, 30.0F, 25.0F);
}

/** Exercise registered PEEP alarms against production completed-cycle values. */
static void peepAlarms(void) {
    unsigned int lCase;
    for (lCase = 0U; lCase < 2U; lCase++) {
        ePhysAlarmType lType = lCase == 0U ? PHYS_ALARM_PEEP_HIGH : PHYS_ALARM_PEEP_LOW;
        float lBad = lCase == 0U ? 11.0F : 1.0F;
        float lEqual = lCase == 0U ? 10.0F : 2.0F;
        uint32_t lStart;
        reset();
        physAlarmManagerInit();
        sample(PHASE_INSP, 30.0F, 25.0F);
        physAlarmManagerProcess(gNow);
        assert(!physAlarmManagerStateGet(lType)); /* No previous cycle. */
        sample(PHASE_EXP, 0.0F, lBad);
        monitorEngineBreathComplete(gNow);
        physAlarmManagerProcess(gNow);
        assert(!physAlarmManagerStateGet(lType)); /* Expiration cannot trigger. */
        gPlan.sequence++;
        sample(PHASE_INSP, 30.0F, 25.0F);
        physAlarmManagerProcess(gNow);
        assert(physAlarmManagerStateGet(lType));
        assert(!physAlarmManagerStateGet(lCase == 0U ? PHYS_ALARM_PEEP_LOW : PHYS_ALARM_PEEP_HIGH));

        peepAlarmNextBreath(5.0F);
        lStart = gNow;
        physAlarmManagerProcess(lStart);
        physAlarmManagerProcess(lStart + 199U);
        assert(physAlarmManagerStateGet(lType));
        gNow += 199U;
        peepAlarmNextBreath(lEqual); /* Equality interrupts recovery. */
        physAlarmManagerProcess(gNow);
        gNow += 250U;
        physAlarmManagerProcess(gNow);
        assert(physAlarmManagerStateGet(lType));

        peepAlarmNextBreath(5.0F);
        lStart = gNow;
        physAlarmManagerProcess(lStart);
        physAlarmManagerProcess(lStart + 199U);
        assert(physAlarmManagerStateGet(lType));
        physAlarmManagerProcess(lStart + 200U);
        assert(!physAlarmManagerStateGet(lType));
        gNow += 200U;
        peepAlarmNextBreath(lEqual);
        physAlarmManagerProcess(gNow);
        assert(!physAlarmManagerStateGet(lType)); /* Equality cannot trigger. */
        peepAlarmNextBreath(lBad);
        physAlarmManagerProcess(gNow);
        assert(physAlarmManagerStateGet(lType));
        peepAlarmNextBreath(5.0F);
        lStart = UINT32_MAX - 100U;
        physAlarmManagerProcess(lStart);
        physAlarmManagerProcess(lStart + 199U);
        assert(physAlarmManagerStateGet(lType));
        physAlarmManagerProcess(lStart + 200U);
        assert(!physAlarmManagerStateGet(lType));
        peepAlarmNextBreath(lBad);
        physAlarmManagerProcess(gNow);
        assert(physAlarmManagerStateGet(lType));
        sample(PHASE_IDLE, 0.0F, 0.0F);
        physAlarmManagerProcess(gNow);
        assert(!physAlarmManagerStateGet(lType));
    }
}

/** Check CPAP timing boundaries, interruptions, phase changes and tick wrap. */
static void cpapCheck(uint32_t nowMs, float insp, float patient, bool active) {
    gData[INSP_REAL_PRS] = insp;
    gData[PAT_REAL_PRS] = patient;
    physAlarmManagerProcess(nowMs);
    assert(physAlarmManagerStateGet(PHYS_ALARM_CPAP_TOO_HIGH) == active);
}

/** Exercise the enabled CPAP detector through the alarm manager. */
static void cpapAlarms(void) {
    reset();
    physAlarmManagerInit();
    gPhase = PHASE_INSP;
    cpapCheck(0U, 21.0F, 19.0F, false);
    cpapCheck(15000U, 21.0F, 19.0F, false);
    cpapCheck(15010U, 19.0F, 21.0F, false);
    cpapCheck(30010U, 19.0F, 21.0F, false);
    cpapCheck(30020U, 21.0F, 21.0F, false);
    cpapCheck(45019U, 21.0F, 21.0F, false);
    cpapCheck(45020U, 20.0F, 21.0F, false);
    cpapCheck(45030U, 21.0F, 21.0F, false);
    gPhase = PHASE_EXP;
    cpapCheck(60029U, 21.0F, 21.0F, false);
    cpapCheck(60030U, 21.0F, 21.0F, true);
    cpapCheck(60040U, 19.0F, 19.0F, true);
    cpapCheck(63039U, 19.0F, 19.0F, true);
    cpapCheck(63040U, 19.0F, 19.5F, true);
    cpapCheck(63050U, 19.0F, 19.0F, true);
    cpapCheck(66050U, 19.0F, 20.0F, true);
    cpapCheck(66060U, 19.0F, 19.0F, true);
    cpapCheck(69059U, 19.0F, 19.0F, true);
    cpapCheck(69060U, 19.0F, 19.0F, false);

    /* A different PEEP shifts both strict thresholds. */
    gPlan.peepCmh2o = 10.0F;
    cpapCheck(70000U, 25.0F, 26.0F, false);
    cpapCheck(85000U, 25.0F, 26.0F, false);
    cpapCheck(85010U, 26.0F, 26.0F, false);
    cpapCheck(100010U, 26.0F, 26.0F, true);
    gPhase = PHASE_IDLE;
    cpapCheck(100020U, 26.0F, 26.0F, false);
    gPhase = PHASE_INSP;
    cpapCheck(100030U, 26.0F, 26.0F, false);
    gPhase = PHASE_COMPEN;
    cpapCheck(115030U, 26.0F, 26.0F, false);
    gPhase = PHASE_INSP;
    cpapCheck(UINT32_MAX - 10000U, 26.0F, 26.0F, false);
    cpapCheck(4998U, 26.0F, 26.0F, false);
    cpapCheck(4999U, 26.0F, 26.0F, true);
    physAlarmManagerInit();
}

int main(void) {
    cpapAlarms();
    stBreathResult lResult;
    stActuatorRequest lRequest;
    uint16_t lTarget;
    unsigned int lIndex;
    peepAlarms();
    dynamicPeep();
    dynamicPeepFlow();
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
        includes = ["user/app/physalarm", "user/app/ventlogic", "user/app/ventalgo", "user/app/databus",
                    "user/app/calibration", "user/module/rtos", "user/tools/controller"]
        command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                   *[f"-I{ROOT / path}" for path in includes], str(harness),
                   str(ROOT / "user/app/ventlogic/monitorengine.c"),
                   str(ROOT / "user/app/physalarm/physalarmvent.c"),
                   str(ROOT / "user/app/physalarm/physalarmmanager.c"),
                   str(ROOT / "user/app/ventalgo/flowcontroller.c"),
                   str(ROOT / "user/tools/controller/pid.c"), "-o", str(executable)]
        environment = os.environ.copy()
        environment["PATH"] = str(Path(compiler).parent) + os.pathsep + environment["PATH"]
        subprocess.run(command, check=True, env=environment)
        subprocess.run([str(executable)], check=True, env=environment)
    print("PASS: PEEP alarms and recovery, dynamic PEEP pressure/flow windows, leak estimate, pause integration, boundaries, invalid data, restart, re-zero, limits")


if __name__ == "__main__":
    main()
