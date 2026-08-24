/************************************************************************************
* @file     : venttest.c
* @brief    : Ventilation test console command implementation.
* @details  : Provides direct PAC mode and running-state test controls.
* @author   :
* @date     :
* @version  :
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "venttest.h"

#include <stdint.h>

#include "blower_vcm.h"
#include "breathscheduler.h"
#include "console.h"
#include "controldata.h"
#include "log.h"
#include "phasecontroller.h"
#include "pressurecontroller.h"

static const char *const gVentTestTag = "venttest";

/** Convert a pressure to a log-friendly hundredth-unit integer. */
static int32_t ventTestPressureCenti(float pressure)
{
    return (int32_t)(pressure * 100.0F);
}

/** Skip spaces in the command arguments. */
static const char *ventTestSkipSpaces(const char *arguments)
{
    while ((*arguments == ' ') || (*arguments == '\t')) {
        arguments++;
    }

    return arguments;
}

/** Match one token and require a token boundary. */
static bool ventTestTokenMatch(const char **arguments, const char *expected)
{
    const char *lArgument = ventTestSkipSpaces(*arguments);
    uint32_t lIndex = 0U;

    while (expected[lIndex] != '\0') {
        if (lArgument[lIndex] != expected[lIndex]) {
            return false;
        }
        lIndex++;
    }
    if ((lArgument[lIndex] != '\0') && (lArgument[lIndex] != ' ') && (lArgument[lIndex] != '\t')) {
        return false;
    }

    *arguments = lArgument + lIndex;
    return true;
}

/** Show the supported ventilation test commands. */
static void ventTestUsageShow(void)
{
    LOG_I(gVentTestTag, "usage: vt mode 1 | run <0|1> | pac | status");
}

/** Show the PAC scheduling and pressure-control chain. */
static void ventTestStatusShow(void)
{
    stBlowerVcmFeedback lBlowerFeedback;
    stPressureControllerDiagnostic lDiagnostic;

    pressureControllerDiagnosticGet(&lDiagnostic);
    if (blowerVcmGetFeedback(&lBlowerFeedback) != BLOWER_VCM_STATUS_OK) {
        lBlowerFeedback.speedScaled = 0U;
    }
    LOG_I(gVentTestTag,
          "mode=%u run=%u phase=%u ref100=%ld patient100=%ld insp100=%ld exp100=%ld flow100=%ld target100=%ld flowff100=%ld patfb100=%ld inner100=%ld blowerff10=%ld blower=%u speed10=%u valve=%u",
          (unsigned int)breathSchedulerModeGet(),
          (unsigned int)breathSchedulerRunningGet(),
          (unsigned int)phaseControllerStateGet(),
          (long)ventTestPressureCenti(phaseControlGet(PHASE_REF_PRESSURE)),
          (long)ventTestPressureCenti(controlDataGet(PAT_REAL_PRS)),
          (long)ventTestPressureCenti(controlDataGet(INSP_REAL_PRS)),
          (long)ventTestPressureCenti(controlDataGet(EXP_REAL_PRS)),
          (long)ventTestPressureCenti(controlDataGet(INSP_FLOW_FILTERED) * PRESSURE_CONTROLLER_FLOW_INPUT_SCALE),
          (long)ventTestPressureCenti(lDiagnostic.inspTarget),
          (long)ventTestPressureCenti(lDiagnostic.flowCompensation),
          (long)ventTestPressureCenti(lDiagnostic.patientCorrection),
          (long)ventTestPressureCenti(lDiagnostic.innerEffort),
          (long)ventTestPressureCenti(lDiagnostic.blowerFeedforward * 0.1F),
          (unsigned int)pressureControllerBlowerTargetGet(),
          (unsigned int)lBlowerFeedback.speedScaled,
          (unsigned int)pressureControllerExpValveDutyGet());
}

/** Dispatch a ventilation test command. */
static eConsoleCommandResult ventTestConsoleCommand(const char *arguments)
{
    int8_t lStatus;
    uint8_t lRun;

    if (ventTestTokenMatch(&arguments, "pac") &&
        (*ventTestSkipSpaces(arguments) == '\0')) {
        lStatus = breathSchedulerStart(VENT_MD_PAC);
        LOG_I(gVentTestTag, "PAC start status=%d", (int)lStatus);
    } else if (ventTestTokenMatch(&arguments, "status") &&
               (*ventTestSkipSpaces(arguments) == '\0')) {
        ventTestStatusShow();
        return CONSOLE_COMMAND_RESULT_OK;
    } else if (ventTestTokenMatch(&arguments, "mode") &&
        ventTestTokenMatch(&arguments, "1") &&
        (*ventTestSkipSpaces(arguments) == '\0')) {
        lStatus = breathSchedulerTestModeSet(1U);
        LOG_I(gVentTestTag, "mode PAC status=%d", (int)lStatus);
    } else if (ventTestTokenMatch(&arguments, "run")) {
        arguments = ventTestSkipSpaces(arguments);
        if (((arguments[0] != '0') && (arguments[0] != '1')) ||
            (*ventTestSkipSpaces(arguments + 1) != '\0')) {
            ventTestUsageShow();
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }
        lRun = (uint8_t)(arguments[0] - '0');
        lStatus = breathSchedulerTestRunSet(lRun);
        LOG_I(gVentTestTag, "run %u status=%d", (unsigned int)lRun, (int)lStatus);
    } else {
        ventTestUsageShow();
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    return (lStatus == BREATH_CONTROL_SUCCESS) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static const stConsoleCommand gVentTestConsoleCommand = {
    "vt",
    "Ventilation test: set PAC mode or running state",
    ventTestConsoleCommand
};

bool ventTestConsoleRegister(void)
{
    return consoleRegisterCommand(&gVentTestConsoleCommand);
}
/**************************End of file********************************/
