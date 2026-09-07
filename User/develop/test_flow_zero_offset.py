"""Test proximal-flow zero compensation with deterministic host inputs."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HARNESS = r'''
/************************************************************************************
* @file     : flow_zero_offset_test.c
* @brief    : Host regression for proximal-flow zero compensation.
***********************************************************************************/
#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "controldata.h"
#include "databus.h"
#include "log.h"
#include "phasecontroller.h"

static float gData[CONTROL_DATA_COUNT];
static float gOffset = 1.5F;
static uint32_t gRunSequence = 1U;
static unsigned int gOffsetSetCount;

float controlDataGet(ControlData_Index_EnumDef index) { return gData[index]; }
void controlDataMdiffFlowZeroOffsetSet(float offsetLpm) {
    gOffset = offsetLpm;
    gOffsetSetCount++;
}
float controlDataMdiffFlowZeroOffsetGet(void) { return gOffset; }
uint8_t breathSchedulerRunningGet(void) { return 1U; }
uint32_t breathSchedulerRunSequenceGet(void) { return gRunSequence; }
void monitorEngineBreathComplete(uint32_t nowMs) { (void)nowMs; }
int8_t breathSchedulerNextPlanGet(eBreathTriggerReason reason, stBreathPlan *plan) {
    (void)reason;
    *plan = (stBreathPlan){0};
    return BREATH_CONTROL_SUCCESS;
}
void logWrite(eLogLevel level, const char *tag, const char *format, ...) {
    (void)level;
    (void)tag;
    (void)format;
}

/** Verify successful correction and timeout retention of the previous offset. */
int main(void) {
    uint32_t lNowMs;

    phaseControllerInit();
    phaseControllerProcess(0U);
    assert(phaseControllerStateGet() == PHASE_COMPEN);
    assert(gOffsetSetCount == 0U);
    assert(gOffset == 1.5F);

    gData[INSP_FLOW_FILTERED] = 0.0F;
    /* The attached board currently exhibits about 4.4 L/min of zero drift. */
    gData[MDIFF_REAL_FLOW] = 4.4F;
    gData[PAT_REAL_PRS] = 0.0F;
    for (lNowMs = 6U; lNowMs <= 66U; lNowMs += 6U) {
        phaseControllerProcess(lNowMs);
    }
    assert(phaseControllerStateGet() == PHASE_EXP);
    assert(gOffsetSetCount == 1U);
    assert((gOffset > 5.899F) && (gOffset < 5.901F));

    gRunSequence++;
    phaseControllerProcess(100U);
    assert(phaseControllerStateGet() == PHASE_COMPEN);
    gData[INSP_FLOW_FILTERED] = 1.0F;
    for (lNowMs = 106U; lNowMs <= 2110U; lNowMs += 6U) {
        phaseControllerProcess(lNowMs);
    }
    assert(phaseControllerStateGet() == PHASE_EXP);
    assert(gOffsetSetCount == 1U);
    assert((gOffset > 5.899F) && (gOffset < 5.901F));
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
    with tempfile.TemporaryDirectory(prefix="ventcore-flow-zero-") as directory:
        harness = Path(directory) / "flow_zero_offset_test.c"
        harness.write_text(HARNESS, encoding="utf-8", newline="\n")
        executable = Path(directory) / "flow_zero_offset_test.exe"
        includes = ["User/app/ventlogic", "User/app/databus", "User/app/setting",
                    "User/module/log", "User/tools/ringbuffer"]
        command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                   *[f"-I{ROOT / path}" for path in includes], str(harness),
                   str(ROOT / "User/app/ventlogic/phasecontroller.c"),
                   "-o", str(executable)]
        environment = os.environ.copy()
        environment["PATH"] = str(Path(compiler).parent) + os.pathsep + environment["PATH"]
        subprocess.run(command, check=True, env=environment)
        subprocess.run([str(executable)], check=True, env=environment)
    print("PASS: correction accumulates on the active offset and timeout preserves it")


if __name__ == "__main__":
    main()
