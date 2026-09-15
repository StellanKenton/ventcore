"""Exercise production RTOS time conversion across long uptime and tick wrap."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
STUB = r'''
#ifndef TEST_FREERTOS_H
#define TEST_FREERTOS_H
#include <stdint.h>
typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned UBaseType_t;
typedef uint16_t configSTACK_DEPTH_TYPE;
typedef void *TaskHandle_t;
#define configMAX_PRIORITIES 32U
#define configTICK_RATE_HZ 1000U
#define pdPASS 1
#define pdMS_TO_TICKS(ms) ((TickType_t)(((TickType_t)(ms) * configTICK_RATE_HZ) / 1000U))
#define taskYIELD() ((void)0)
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
BaseType_t xTaskCreate(void (*entry)(void *), const char *name, configSTACK_DEPTH_TYPE stack,
                       void *arg, UBaseType_t priority, TaskHandle_t *handle);
void vTaskDelete(TaskHandle_t handle);
void vTaskDelay(TickType_t ticks);
BaseType_t xTaskDelayUntil(TickType_t *previous, TickType_t period);
TickType_t xTaskGetTickCount(void);
void vTaskStartScheduler(void);
#endif
'''
HARNESS = r'''
/************************************************************************************
* @file     : rtos_timing_test.c
* @brief    : Verify production provider timing at long uptime and wrap boundaries.
***********************************************************************************/
#include <assert.h>
#include <stddef.h>
#include "FreeRTOS.h"
#include "portrtos.h"

static TickType_t gNow;
static TickType_t gDelay;
static TickType_t gPrevious;
static TickType_t gPeriod;
BaseType_t xTaskCreate(void (*entry)(void *), const char *name, configSTACK_DEPTH_TYPE stack,
                       void *arg, UBaseType_t priority, TaskHandle_t *handle) {
    (void)entry; (void)name; (void)stack; (void)arg; (void)priority; (void)handle;
    return pdPASS;
}
void vTaskDelete(TaskHandle_t handle) { (void)handle; }
void vTaskDelay(TickType_t ticks) { gDelay = ticks; }
TickType_t xTaskGetTickCount(void) { return gNow; }
void vTaskStartScheduler(void) {}
/** Model the kernel's unsigned absolute wake update and observe blocking time. */
BaseType_t xTaskDelayUntil(TickType_t *previous, TickType_t period) {
    gPrevious = *previous;
    gPeriod = period;
    *previous += period;
    gDelay = *previous - gNow;
    return pdPASS;
}
int main(void) {
    const stRepRtosOps *lOps = portRtosGetOps();
    const uint32_t lStarts[] = {0U, 4294940U, 4294967U, 4294968U, 6675780U,
                               86400000U, UINT32_MAX - 40U};
    const uint32_t lPeriods[] = {3U, 6U, 10U, 20U};
    for (unsigned int i = 0; i < sizeof(lStarts) / sizeof(lStarts[0]); ++i) {
        for (unsigned int j = 0; j < sizeof(lPeriods) / sizeof(lPeriods[0]); ++j) {
            uint32_t lWake = lStarts[i];
            for (unsigned int k = 0; k < 1000; ++k) {
                gNow = lWake;
                assert(lOps->getTickMs() == gNow);
                assert(lOps->taskDelayUntilMs(&lWake, lPeriods[j]) == REP_RTOS_STATUS_OK);
                assert(gPrevious == gNow);
                assert(gPeriod == lPeriods[j]);
                assert(gDelay == lPeriods[j]);
                assert(lWake == (uint32_t)(gNow + lPeriods[j]));
            }
        }
        assert(lOps->taskDelayMs(lStarts[i]) == REP_RTOS_STATUS_OK);
        if (lStarts[i] != 0U) assert(gDelay == lStarts[i]);
    }
    uint32_t lWake = 123U;
    assert(lOps->taskDelayUntilMs(NULL, 20U) == REP_RTOS_STATUS_INVALID_PARAM);
    assert(lOps->taskDelayUntilMs(&lWake, 0U) == REP_RTOS_STATUS_INVALID_PARAM);
    assert(lWake == 123U);
    return 0;
}
/**************************End of file********************************/
'''


def main():
    compiler = os.environ.get('CC') or shutil.which('gcc') or shutil.which('clang')
    if not compiler:
        compiler = next((str(p) for p in (
            Path('C:/msys64/ucrt64/bin/gcc.exe'),
            Path('C:/Qt/Tools/mingw1310_64/bin/gcc.exe')) if p.is_file()), None)
    if not compiler:
        raise SystemExit('Set CC to a native GCC or Clang compiler')
    environment = os.environ.copy()
    environment['PATH'] = str(Path(compiler).parent) + os.pathsep + environment['PATH']
    with tempfile.TemporaryDirectory(prefix='vent-rtos-timing-') as directory:
        work = Path(directory)
        (work / 'FreeRTOS.h').write_text(STUB, encoding='utf-8', newline='\n')
        (work / 'task.h').write_text('#include "FreeRTOS.h"\n', encoding='utf-8')
        (work / 'rtos_timing_test.c').write_text(HARNESS, encoding='utf-8', newline='\n')
        executable = work / 'test.exe'
        subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                        f'-I{work}', f'-I{ROOT / "User/module/rtos"}',
                        str(work / 'rtos_timing_test.c'),
                        str(ROOT / 'User/module/rtos/portrtos.c'),
                        '-o', str(executable)], check=True, env=environment)
        subprocess.run([str(executable)], check=True, env=environment)
    print('PASS: 71-minute boundary, captured uptime, 24 hours, 32-bit tick wrap, invalid arguments')


if __name__ == '__main__':
    main()
