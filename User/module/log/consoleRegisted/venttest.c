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
#include "rtos.h"

static const char *const gVentTestTag = "venttest";
static stVentTestTransientSample gVentTestTransientBuffer[VENT_TEST_TRANSIENT_SAMPLE_COUNT];
static stVentTestTransientSample gVentTestTransientUpload[VENT_TEST_TRANSIENT_SAMPLE_COUNT];
static volatile uint32_t gVentTestTransientTotalCount = 0U;
static uint32_t gVentTestTransientUploadedCount = 0U;

/** Convert a signal to a log-friendly hundredth-unit integer. */
static int32_t ventTestCenti(float value)
{
    return (int32_t)(value * 100.0F);
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

/** Parse one unsigned decimal test argument. */
static bool ventTestUnsignedParse(const char **arguments, uint16_t *value)
{
    const char *lArgument = ventTestSkipSpaces(*arguments);
    uint32_t lValue = 0U;
    uint8_t lDigitFound = 0U;

    while ((lArgument[0] >= '0') && (lArgument[0] <= '9')) {
        lDigitFound = 1U;
        lValue = (lValue * 10U) + (uint32_t)(lArgument[0] - '0');
        if (lValue > UINT16_MAX) {
            return false;
        }
        lArgument++;
    }
    if ((lDigitFound == 0U) ||
        ((lArgument[0] != '\0') && (lArgument[0] != ' ') && (lArgument[0] != '\t'))) {
        return false;
    }

    *arguments = lArgument;
    *value = (uint16_t)lValue;
    return true;
}

/** Show the supported ventilation test commands. */
static void ventTestUsageShow(void)
{
    LOG_I(gVentTestTag, "usage: vt mode 1 | run <0|1> | pac | set <peep> <delta> | status");
}

/** Upload only samples recorded since the previous status command. */
static void ventTestStatusShow(void)
{
    uint32_t lCurrentCount;
    uint32_t lDroppedCount = 0U;
    uint32_t lFirstSequence;
    uint32_t lSequence;
    uint16_t lCount;
    uint16_t lIndex;

    repRtosEnterCritical();
    lCurrentCount = gVentTestTransientTotalCount;
    lFirstSequence = gVentTestTransientUploadedCount;
    if ((lCurrentCount - lFirstSequence) > VENT_TEST_TRANSIENT_SAMPLE_COUNT) {
        lDroppedCount = (lCurrentCount - lFirstSequence) - VENT_TEST_TRANSIENT_SAMPLE_COUNT;
        lFirstSequence = lCurrentCount - VENT_TEST_TRANSIENT_SAMPLE_COUNT;
    }
    lCount = (uint16_t)(lCurrentCount - lFirstSequence);
    for (lIndex = 0U; lIndex < lCount; lIndex++) {
        lSequence = lFirstSequence + lIndex;
        gVentTestTransientUpload[lIndex] = gVentTestTransientBuffer[lSequence % VENT_TEST_TRANSIENT_SAMPLE_COUNT];
    }
    gVentTestTransientUploadedCount = lCurrentCount;
    repRtosExitCritical();

    LOG_R("VT_TRANSIENT_BEGIN,count=%u,interval_ms=%u,first_sequence=%lu,dropped=%lu",
          (unsigned int)lCount,
          (unsigned int)VENT_TEST_TRANSIENT_SAMPLE_INTERVAL_MS,
          (unsigned long)lFirstSequence,
          (unsigned long)lDroppedCount);
    LOG_R("sequence,time_ms,patient_pressure100,insp_pressure100,pressure_reference100,phase,blower_target,blower_actual,valve_duty,air_flow100,o2_flow100,exp_flow100");
    for (lIndex = 0U; lIndex < lCount; lIndex++) {
        const stVentTestTransientSample *lSample = &gVentTestTransientUpload[lIndex];

        lSequence = lFirstSequence + lIndex;
        LOG_R("%lu,%lu,%ld,%ld,%ld,%u,%u,%u,%u,%ld,%ld,%ld",
              (unsigned long)lSequence,
              (unsigned long)(lSequence * VENT_TEST_TRANSIENT_SAMPLE_INTERVAL_MS),
              (long)lSample->patientPressureCenti,
              (long)lSample->inspPressureCenti,
              (long)lSample->pressureReferenceCenti,
              (unsigned int)lSample->phase,
              (unsigned int)lSample->blowerTarget,
              (unsigned int)lSample->blowerActual,
              (unsigned int)lSample->valveDuty,
              (long)lSample->airFlowCenti,
              (long)lSample->o2FlowCenti,
              (long)lSample->expFlowCenti);
    }
    LOG_R("VT_TRANSIENT_END,count=%u", (unsigned int)lCount);
}

/** Record one control-cycle sample into the rolling two-second buffer. */
void ventTestTransientRecord(void)
{
    stBlowerVcmFeedback lBlowerFeedback;
    stVentTestTransientSample *lSample;
    uint32_t lSequence = gVentTestTransientTotalCount;

    if (blowerVcmGetFeedback(&lBlowerFeedback) != BLOWER_VCM_STATUS_OK) {
        lBlowerFeedback.speedScaled = 0U;
    }

    lSample = &gVentTestTransientBuffer[lSequence % VENT_TEST_TRANSIENT_SAMPLE_COUNT];
    lSample->patientPressureCenti = ventTestCenti(controlDataGet(PAT_REAL_PRS));
    lSample->inspPressureCenti = ventTestCenti(controlDataGet(INSP_REAL_PRS));
    lSample->pressureReferenceCenti = ventTestCenti(phaseControlGet(PHASE_REF_PRESSURE));
    lSample->airFlowCenti = ventTestCenti(controlDataGet(INSP_FLOW_FILTERED));
    lSample->o2FlowCenti = ventTestCenti(controlDataGet(O2_FLOW_FILTERED));
    lSample->expFlowCenti = ventTestCenti(controlDataGet(MDIFF_REAL_FLOW));
    lSample->blowerTarget = pressureControllerBlowerTargetGet();
    lSample->blowerActual = lBlowerFeedback.speedScaled;
    lSample->phase = (uint8_t)phaseControllerStateGet();
    lSample->valveDuty = pressureControllerExpValveDutyGet();

    gVentTestTransientTotalCount = lSequence + 1U;
}

/** Dispatch a ventilation test command. */
static eConsoleCommandResult ventTestConsoleCommand(const char *arguments)
{
    stVentPacSettings lPreviousSettings;
    stVentPacSettings *lPacSettings;
    int8_t lStatus;
    uint16_t lDeltaPressure;
    uint16_t lPeep;
    uint8_t lRun;

    if (ventTestTokenMatch(&arguments, "pac") &&
        (*ventTestSkipSpaces(arguments) == '\0')) {
        lStatus = breathSchedulerStart(VENT_MD_PAC);
        LOG_I(gVentTestTag, "PAC start status=%d", (int)lStatus);
    } else if (ventTestTokenMatch(&arguments, "status") &&
               (*ventTestSkipSpaces(arguments) == '\0')) {
        ventTestStatusShow();
        return CONSOLE_COMMAND_RESULT_OK;
    } else if (ventTestTokenMatch(&arguments, "set") &&
               ventTestUnsignedParse(&arguments, &lPeep) &&
               ventTestUnsignedParse(&arguments, &lDeltaPressure) &&
               (*ventTestSkipSpaces(arguments) == '\0')) {
        (void)breathSchedulerStop();
        lPacSettings = GetVentPacSettings();
        lPreviousSettings = *lPacSettings;
        lPacSettings->peep = (float)lPeep;
        lPacSettings->DeltaPressure = (float)lDeltaPressure;
        lStatus = breathSchedulerTestModeSet((uint8_t)VENT_MD_PAC);
        if (lStatus != BREATH_CONTROL_SUCCESS) {
            *lPacSettings = lPreviousSettings;
            (void)breathSchedulerTestModeSet((uint8_t)VENT_MD_PAC);
        }
        LOG_I(gVentTestTag,
              "set peep100=%u delta100=%u target100=%u status=%d",
              (unsigned int)(lPeep * 100U),
              (unsigned int)(lDeltaPressure * 100U),
              (unsigned int)((lPeep + lDeltaPressure) * 100U),
              (int)lStatus);
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
