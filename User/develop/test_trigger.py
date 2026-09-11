"""Host regression for expiration readiness, false triggers and patient efforts."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HARNESS = r'''
/************************************************************************************
* @file     : trigger_test.c
* @brief    : Production trigger engine regression with sampled sensor inputs.
***********************************************************************************/
#include <assert.h>
#include <math.h>
#include <string.h>
#include "triggerengine.h"
#include "controldata.h"
#include "log.h"
#include "expirationcontroller.h"
#include "calibtrans.h"

static stBreathPlan gPlan;
static ePhaseControllerState gPhase;
static uint8_t gReady;
static unsigned int gTriggers;
static uint32_t gNow;
static float gPressure;
static float gFlow;
static eBreathTriggerReason gReason;

float controlDataGet(ControlData_Index_EnumDef index) {
    return index == PAT_REAL_FLOW ? gFlow : gPressure;
}
float phaseControlGet(ePhaseControlType type) {
    (void)type;
    return gPlan.peepCmh2o;
}
int8_t phaseControllerExpirationCaptureNotify(void) {
    gReady = 1U;
    return PHASE_CONTROL_SUCCESS;
}
int8_t calibtransPrsSpeed(float pressure, float *speed) {
    *speed = pressure * 10.0F;
    return CALIBTRANS_STATUS_OK;
}
ePhaseControllerState phaseControllerStateGet(void) { return gPhase; }
uint8_t phaseControllerExpirationReadyGet(void) { return gReady; }
int8_t phaseControllerActivePlanGet(stBreathPlan *plan) {
    *plan = gPlan;
    return PHASE_CONTROL_SUCCESS;
}
int8_t phaseControllerTrigger(eBreathTriggerReason reason, uint32_t nowMs) {
    if (nowMs < gPlan.minimumExpiratoryTimeMs) { return PHASE_CONTROL_ERROR_STATE; }
    gTriggers++;
    gReason = reason;
    gPhase = PHASE_INSP;
    return PHASE_CONTROL_SUCCESS;
}
void logWrite(eLogLevel level, const char *tag, const char *format, ...) {
    (void)level; (void)tag; (void)format;
}

/** Begin one independent expiration with the screenshot thresholds. */
static void setup(eVentTriggerType type) {
    memset(&gPlan, 0, sizeof(gPlan));
    gPlan.sequence = 1U;
    gPlan.mode = VENT_MD_PAC;
    gPlan.peepCmh2o = 5.0F;
    gPlan.allowedTriggerType = type;
    gPlan.pressureTriggerCmh2o = 3.0F;
    gPlan.flowTriggerLpm = 3.0F;
    gPlan.minimumExpiratoryTimeMs = 192U;
    gNow = 192U;
    gTriggers = 0U;
    gPhase = PHASE_EXP;
    gReady = 1U;
    triggerEngineInit();
}

/** Feed uniformly spaced samples without bypassing the production state machine. */
static void samples(float pressure, float flow, unsigned int count) {
    gPressure = pressure;
    gFlow = flow;
    for (unsigned int i = 0; i < count; i++) {
        triggerEngineProcess(gNow);
        gNow += 6U;
    }
}

int main(void) {
    /* Run the real release/capture controller before trigger detection. */
    setup(VENT_TRIGGER_FLOW);
    gPlan.flowTriggerLpm = 1.0F;
    gReady = 0U;
    expirationControllerInit();
    stActuatorRequest lRequest = {0};
    for (unsigned int i = 0U; i < 300U; i++) {
        samples(8.0F, -10.0F, 1U);
        assert(expirationControllerProcess(&gPlan, NULL, &lRequest) ==
               ACTUATOR_REQUEST_SUCCESS);
    }
    assert(gReady == 0U && gTriggers == 0U);
    assert(expirationControllerStateGet() == EXPIRATION_CONTROLLER_RELEASE);
    for (unsigned int i = 0U; i < 200U; i++) {
        samples(2.0F, 0.0F, 1U);
        assert(expirationControllerProcess(&gPlan, NULL, &lRequest) ==
               ACTUATOR_REQUEST_SUCCESS);
    }
    assert(expirationControllerStateGet() == EXPIRATION_CONTROLLER_PEEP);
    assert(gReady == 1U && gTriggers == 0U);
    samples(1.0F, 1.1F, 3U);
    assert(gTriggers == 1U);

    /* Release dwell is consecutive, including after a new breath plan. */
    gPlan.sequence++;
    gPhase = PHASE_EXP;
    gReady = 0U;
    for (unsigned int i = 0U; i < 9U; i++) {
        samples(2.0F, 0.0F, 1U);
        assert(expirationControllerProcess(&gPlan, NULL, &lRequest) ==
               ACTUATOR_REQUEST_SUCCESS);
    }
    assert(expirationControllerStateGet() == EXPIRATION_CONTROLLER_RELEASE);
    samples(8.0F, 0.0F, 1U);
    assert(expirationControllerProcess(&gPlan, NULL, &lRequest) ==
           ACTUATOR_REQUEST_SUCCESS);
    for (unsigned int i = 0U; i < 9U; i++) {
        samples(5.0F, 0.0F, 1U);
        assert(expirationControllerProcess(&gPlan, NULL, &lRequest) ==
               ACTUATOR_REQUEST_SUCCESS);
    }
    assert(expirationControllerStateGet() == EXPIRATION_CONTROLLER_RELEASE);
    samples(5.0F, 0.0F, 1U);
    assert(expirationControllerProcess(&gPlan, NULL, &lRequest) ==
           ACTUATOR_REQUEST_SUCCESS);
    assert(expirationControllerStateGet() == EXPIRATION_CONTROLLER_CAPTURE);
    assert(gReady == 0U);
    for (unsigned int i = 0U; i < 30U; i++) {
        samples(5.0F, 0.0F, 1U);
        assert(expirationControllerProcess(&gPlan, NULL, &lRequest) ==
               ACTUATOR_REQUEST_SUCCESS);
    }
    assert(expirationControllerStateGet() == EXPIRATION_CONTROLLER_PEEP);
    assert(gReady == 1U);

    /* Low PEEP must not permanently block arming after capture completes. */
    for (int i = 0; i < 2; i++) {
        setup(i == 0 ? VENT_TRIGGER_FLOW : VENT_TRIGGER_PRESSURE);
        gPlan.flowTriggerLpm = 1.0F;
        gPlan.pressureTriggerCmh2o = 1.0F;
        samples(2.0F, 0.0F, 2000U);
        assert(gTriggers == 0U);
        samples(0.9F, 1.1F, 2U);
        assert(gTriggers == 0U);
        samples(0.9F, 1.1F, 1U);
        assert(gTriggers == 1U);
        assert(gReason == (i == 0 ? BREATH_TRIGGER_REASON_FLOW :
                                  BREATH_TRIGGER_REASON_PRESSURE));

        /* The next expiration must arm again without returning to set PEEP. */
        gPhase = PHASE_EXP;
        gPlan.sequence++;
        samples(2.0F, -10.0F, 10U);
        for (unsigned int j = 1U; j <= 100U; j++) {
            samples(2.0F, -10.0F + 0.1F * (float)j, 1U);
        }
        samples(2.0F, 0.0F, 20U);
        assert(gTriggers == 1U);
        samples(0.9F, 1.1F, 3U);
        assert(gTriggers == 2U);
    }

    /* Reproduce negative expiratory baseline followed by passive emptying. */
    setup(VENT_TRIGGER_FLOW);
    samples(5.0F, -30.0F, 20U);
    for (int i = 0; i <= 60; i++) { samples(5.0F, -30.0F + i * 0.5F, 1U); }
    samples(5.0F, 0.0F, 40U);
    assert(gTriggers == 0U);
    samples(5.0F, 3.1F, 2U);
    assert(gTriggers == 0U);
    samples(5.0F, 0.0F, 1U);
    samples(5.0F, 3.1F, 2U);
    assert(gTriggers == 0U);
    samples(5.0F, 3.1F, 1U);
    assert(gTriggers == 1U && gReason == BREATH_TRIGGER_REASON_FLOW);
    samples(5.0F, 10.0F, 30U);
    assert(gTriggers == 1U);

    /* A moving negative baseline detects effort before expiratory flow reaches zero. */
    setup(VENT_TRIGGER_FLOW);
    gPlan.mode = VENT_MD_VAC;
    gPlan.flowTriggerLpm = 10.0F;
    gReady = 0U;
    samples(8.0F, -40.0F, 40U);
    assert(gTriggers == 0U);
    gReady = 1U;
    for (unsigned int i = 1U; i <= 100U; i++) {
        samples(5.0F, -40.0F + 0.2F * (float)i, 1U);
    }
    assert(gTriggers == 0U);
    samples(5.0F, -9.0F, 2U);
    assert(gTriggers == 0U);
    samples(5.0F, -20.0F, 1U);
    samples(5.0F, -9.0F, 2U);
    assert(gTriggers == 0U);
    samples(5.0F, -9.0F, 1U);
    assert(gTriggers == 1U && gFlow < 0.0F);

    /* Passive exponential emptying must not trigger at the tested time constants. */
    for (unsigned int i = 0U; i < 3U; i++) {
        setup(VENT_TRIGGER_FLOW);
        samples(5.0F, -30.0F, 30U);
        for (unsigned int j = 1U; j <= 500U; j++) {
            samples(5.0F, -30.0F * expf(-0.006F * (float)j /
                    (0.2F + 0.2F * (float)i)), 1U);
        }
        samples(5.0F, 0.0F, 30U);
        assert(gTriggers == 0U);
    }

    /* A gradual VAC effort must not be learned as expiratory bias flow. */
    setup(VENT_TRIGGER_FLOW);
    gPlan.mode = VENT_MD_VAC;
    gPlan.flowTriggerLpm = 10.0F;
    samples(4.5F, -30.0F, 20U);
    for (unsigned int i = 1U; i <= 60U; i++) {
        samples(4.5F, -30.0F + 0.5F * (float)i, 1U);
    }
    samples(4.5F, 0.0F, 100U);
    assert(gTriggers == 0U);
    for (unsigned int i = 1U; i <= 50U; i++) {
        samples(4.5F - 0.84F * (float)i / 50.0F,
                23.2F * (float)i / 50.0F, 1U);
    }
    assert(gTriggers == 1U && gReason == BREATH_TRIGGER_REASON_FLOW);

    /* Retain established positive bias, threshold and consecutive confirmation. */
    setup(VENT_TRIGGER_FLOW);
    gPlan.mode = VENT_MD_VAC;
    gPlan.flowTriggerLpm = 10.0F;
    samples(4.5F, 3.0F, 30U);
    for (unsigned int i = 1U; i <= 50U; i++) {
        samples(4.5F, 3.0F + 9.9F * (float)i / 50.0F, 1U);
    }
    samples(4.5F, 12.9F, 100U);
    assert(gTriggers == 0U);
    samples(4.5F, 13.0F, 2U);
    assert(gTriggers == 0U);
    samples(4.5F, 12.9F, 1U);
    samples(4.5F, 13.0F, 2U);
    assert(gTriggers == 0U);
    samples(4.5F, 13.0F, 1U);
    assert(gTriggers == 1U);

    /* The next expiration learns its own bias and follows declining flow. */
    gPlan.sequence++;
    gPhase = PHASE_EXP;
    samples(4.5F, 20.0F, 30U);
    assert(gTriggers == 1U);
    samples(4.5F, 0.0F, 100U);
    samples(4.5F, 10.1F, 3U);
    assert(gTriggers == 2U);

    /* Preserve positive bias-flow subtraction and real inspiratory triggering. */
    setup(VENT_TRIGGER_FLOW);
    samples(5.0F, 2.0F, 30U);
    samples(5.0F, 2.5F, 30U);
    assert(gTriggers == 0U);
    samples(5.0F, 6.0F, 3U);
    assert(gTriggers == 1U);

    /* Capture timeout and normal pressure release must not arm a high baseline. */
    setup(VENT_TRIGGER_PRESSURE);
    samples(12.0F, 0.0F, 30U);
    samples(8.0F, 0.0F, 30U);
    samples(5.0F, 0.0F, 30U);
    assert(gTriggers == 0U);
    samples(9.0F, 0.0F, 50U);
    samples(5.0F, 0.0F, 30U);
    assert(gTriggers == 0U);
    samples(1.9F, 0.0F, 3U);
    assert(gTriggers == 1U && gReason == BREATH_TRIGGER_REASON_PRESSURE);
    setup(VENT_TRIGGER_PRESSURE);
    gPlan.pressureTriggerCmh2o = -3.0F;
    samples(5.0F, 0.0F, 10U);
    samples(1.9F, 0.0F, 3U);
    assert(gTriggers == 1U);

    /* Slow pressure efforts below actual PEEP must survive baseline tracking. */
    setup(VENT_TRIGGER_PRESSURE);
    gPlan.mode = VENT_MD_VAC;
    gPlan.pressureTriggerCmh2o = 2.0F;
    samples(2.5F, 0.0F, 100U);
    assert(gTriggers == 0U);
    for (unsigned int i = 1U; i <= 50U; i++) {
        samples(2.5F - 2.5F * (float)i / 50.0F, 0.0F, 1U);
    }
    assert(gTriggers == 1U && gReason == BREATH_TRIGGER_REASON_PRESSURE);

    /* A brief pressure rebound is not a new stable pressure reference. */
    setup(VENT_TRIGGER_PRESSURE);
    gPlan.mode = VENT_MD_VAC;
    gPlan.pressureTriggerCmh2o = 2.0F;
    samples(2.5F, 0.0F, 100U);
    samples(4.8F, 0.0F, 4U);
    samples(2.5F, 0.0F, 100U);
    samples(0.6F, 0.0F, 4U);
    assert(gTriggers == 0U);
    samples(0.5F, 0.0F, 2U);
    assert(gTriggers == 0U);
    samples(0.6F, 0.0F, 1U);
    samples(0.5F, 0.0F, 2U);
    assert(gTriggers == 0U);
    samples(0.5F, 0.0F, 1U);
    assert(gTriggers == 1U);
    gPlan.sequence++;
    gPhase = PHASE_EXP;
    samples(1.0F, 0.0F, 100U);
    assert(gTriggers == 1U);
    samples(-1.0F, 0.0F, 3U);
    assert(gTriggers == 2U);

    /* Stable pressure levels continue rebuilding the reference in expiration. */
    setup(VENT_TRIGGER_PRESSURE);
    gPlan.pressureTriggerCmh2o = 2.0F;
    samples(2.5F, 0.0F, 30U);
    samples(3.5F, 0.0F, 200U);
    assert(gTriggers == 0U);
    samples(1.4F, 0.0F, 3U);
    assert(gTriggers == 1U);
    setup(VENT_TRIGGER_PRESSURE);
    gPlan.pressureTriggerCmh2o = 2.0F;
    samples(3.5F, 0.0F, 30U);
    samples(2.5F, 0.0F, 200U);
    samples(1.4F, 0.0F, 3U);
    assert(gTriggers == 0U);
    samples(0.4F, 0.0F, 3U);
    assert(gTriggers == 1U);

    /* A discontinuous settling interval must not reuse the old high baseline. */
    setup(VENT_TRIGGER_PRESSURE);
    samples(5.0F, 0.0F, 3U);
    samples(20.0F, 0.0F, 1U);
    samples(5.0F, 0.0F, 10U);
    assert(gTriggers == 0U);

    setup(VENT_TRIGGER_FLOW);
    gReady = 0U;
    samples(5.0F, -30.0F, 10U);
    samples(5.0F, 10.0F, 10U);
    assert(gTriggers == 0U);
    gReady = 1U;
    samples(5.0F, 0.0F, 10U);
    samples(5.0F, 4.0F, 2U);
    samples(NAN, 4.0F, 1U);
    samples(5.0F, 0.0F, 10U);
    samples(5.0F, 4.0F, 2U);
    assert(gTriggers == 0U);
    samples(5.0F, 4.0F, 1U);
    assert(gTriggers == 1U);

    /* Switching trigger type discards the old candidate even at the same sequence. */
    setup(VENT_TRIGGER_FLOW);
    samples(5.0F, -20.0F, 20U);
    samples(5.0F, -10.0F, 2U);
    gPlan.allowedTriggerType = VENT_TRIGGER_PRESSURE;
    samples(2.0F, -10.0F, 20U);
    assert(gTriggers == 0U);
    gPlan.allowedTriggerType = VENT_TRIGGER_FLOW;
    samples(2.0F, -10.0F, 20U);
    assert(gTriggers == 0U);
    samples(2.0F, -6.9F, 3U);
    assert(gTriggers == 1U);

    /* Minimum expiration, plan changes and each supported spontaneous mode. */
    setup(VENT_TRIGGER_FLOW);
    gNow = 0U;
    samples(5.0F, 0.0F, 10U);
    samples(5.0F, 4.0F, 10U);
    assert(gTriggers == 0U);
    samples(5.0F, 4.0F, 20U);
    assert(gTriggers == 1U);
    setup(VENT_TRIGGER_FLOW);
    samples(5.0F, 0.0F, 10U);
    samples(5.0F, 4.0F, 2U);
    gPlan.sequence++;
    samples(5.0F, 4.0F, 10U);
    assert(gTriggers == 0U);
    for (int i = 0; i < 2; i++) {
        setup(VENT_TRIGGER_FLOW);
        gPlan.mode = i == 0 ? VENT_MD_CPAP_PSV : VENT_MD_PSV_ST;
        samples(5.0F, 0.0F, 10U);
        samples(5.0F, 4.0F, 3U);
        assert(gTriggers == 1U);
    }

    for (int i = 0; i < 4; i++) {
        setup(VENT_TRIGGER_FLOW);
        gPlan.flowTriggerLpm = i == 0 ? 0.0F : i == 1 ? -1.0F : i == 2 ? NAN : INFINITY;
        samples(5.0F, 0.0F, 20U);
        samples(5.0F, 10.0F, 20U);
        assert(gTriggers == 0U);
    }
    setup(VENT_TRIGGER_OFF);
    samples(5.0F, 0.0F, 20U);
    samples(1.0F, 10.0F, 20U);
    assert(gTriggers == 0U);
    for (int i = 0; i < 3; i++) {
        setup(i == 0 ? VENT_TRIGGER_FLOW : i == 1 ? VENT_TRIGGER_PRESSURE : VENT_TRIGGER_OFF);
        gPlan.mode = VENT_MD_VAC;
        samples(5.0F, 0.0F, 20U);
        samples(1.0F, 10.0F, 2U);
        assert(gTriggers == 0U);
        samples(1.0F, 10.0F, 1U);
        assert(gTriggers == (i == 2 ? 0U : 1U));
        if (i != 2) {
            assert(gReason == (i == 0 ? BREATH_TRIGGER_REASON_FLOW : BREATH_TRIGGER_REASON_PRESSURE));
        }
    }
    setup(VENT_TRIGGER_FLOW);
    gPlan.mode = VENT_MD_V_SIMV;
    samples(5.0F, 0.0F, 20U);
    samples(1.0F, 10.0F, 20U);
    assert(gTriggers == 0U);
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
    with tempfile.TemporaryDirectory(prefix="ventcore-trigger-") as directory:
        harness = Path(directory) / "trigger_test.c"
        harness.write_text(HARNESS, encoding="utf-8", newline="\n")
        executable = Path(directory) / "trigger_test.exe"
        includes = ["user/app/ventlogic", "user/app/databus", "user/app/ventalgo",
                    "user/app/calibration", "user/tools/controller",
                    "user/module/log", "user/tools/ringbuffer"]
        sources = ["user/app/ventlogic/triggerengine.c",
                   "user/app/ventalgo/expirationcontroller.c", "user/tools/controller/pid.c"]
        command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                   *[f"-I{ROOT / path}" for path in includes], str(harness),
                   *[str(ROOT / path) for path in sources], "-o", str(executable)]
        environment = os.environ.copy()
        environment["PATH"] = str(Path(compiler).parent) + os.pathsep + environment["PATH"]
        subprocess.run(command, check=True, env=environment)
        subprocess.run([str(executable)], check=True, env=environment)
    print("PASS: release/capture recovery, low PEEP rearming, expiratory rebound, efforts, debounce, faults, modes")



if __name__ == "__main__":
    main()
