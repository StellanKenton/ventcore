/************************************************************************************
* @file     : venttest.c
* @brief    : Ventilation test console command implementation.
* @details  : Provides direct ventilation mode and running-state test controls.
* @author   :
* @date     :
* @version  :
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "venttest.h"

#include <stdint.h>

#include "apneaengine.h"
#include "breathscheduler.h"
#include "console.h"
#include "log.h"
#include "monitordata.h"
#include "monitorengine.h"
#include "rtos.h"

static const char *const gVentTestTag = "venttest";
static stMonitorWaveformData gVentTestTransientBuffer[VENT_TEST_TRANSIENT_SAMPLE_COUNT];
static stMonitorWaveformData gVentTestTransientUpload[VENT_TEST_TRANSIENT_SAMPLE_COUNT];
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
    LOG_I(gVentTestTag, "usage: vt mode <x> | run <0|1> | pac | vac | psv | psvst | stop | set <peep> <delta> | trigger off | trigger pressure <cmh2o100> | trigger flow <lpm100> | status");
}

/** Upload only samples recorded since the previous status command. */
static void ventTestStatusShow(void)
{
    stBreathResult lBreathResult;
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

    if (monitorEngineBreathResultGet(&lBreathResult) == MONITOR_ENGINE_SUCCESS) {
        LOG_R("VT_BREATH_RESULT,sequence=%lu,mode=%u,type=%u,trigger=%u,cycle_reason=%u,vti100=%ld,vte100=%ld,ppeak100=%ld,peep100=%ld,peak_insp_flow100=%ld,ti_ms=%lu,cycle_ms=%lu,valid=0x%08lX",
              (unsigned long)lBreathResult.sequence,
              (unsigned int)lBreathResult.mode,
              (unsigned int)lBreathResult.breathType,
              (unsigned int)lBreathResult.triggerReason,
              (unsigned int)lBreathResult.cycleReason,
              (long)ventTestCenti(lBreathResult.vtiMl),
              (long)ventTestCenti(lBreathResult.vteMl),
              (long)ventTestCenti(lBreathResult.ppeakCmh2o),
              (long)ventTestCenti(lBreathResult.peepCmh2o),
              (long)ventTestCenti(lBreathResult.peakInspiratoryFlowLpm),
              (unsigned long)lBreathResult.inspiratoryTimeMs,
              (unsigned long)lBreathResult.cycleTimeMs,
              (unsigned long)lBreathResult.validMask);
    }
    LOG_R("VT_APNEA_STATE,state=%u", (unsigned int)apneaEngineStateGet());
    LOG_R("VT_TRANSIENT_BEGIN,count=%u,interval_ms=%u,first_sequence=%lu,dropped=%lu",
          (unsigned int)lCount,
          (unsigned int)VENT_TEST_TRANSIENT_SAMPLE_INTERVAL_MS,
          (unsigned long)lFirstSequence,
          (unsigned long)lDroppedCount);
    LOG_R("VT_MONITOR_SCALE,float_fields=100");
    LOG_R("sequence,time_ms,air_x2,o2_x2,prox_x2,pinsp_x1,ppeep_x1,pexp_x1,ppat_x1,blower_x10,pref_x1,fastref_x1,flowcomp_x1,pcorr_x1,effort_x1,ff_x1,vt_x10,vti_x10,vte_x10,target_x100,valve_x2,expiration_state,pressure_state");
    for (lIndex = 0U; lIndex < lCount; lIndex++) {
        const stMonitorWaveformData *lSample = &gVentTestTransientUpload[lIndex];

        lSequence = lFirstSequence + lIndex;
        LOG_R("%lu,%lu,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%u,%u,%u,%u",
              (unsigned long)lSequence,
              (unsigned long)(lSequence * VENT_TEST_TRANSIENT_SAMPLE_INTERVAL_MS),
              (long)ventTestCenti(lSample->airFlowX2),
              (long)ventTestCenti(lSample->oxygenFlowX2),
              (long)ventTestCenti(lSample->proximalFlowX2),
              (long)ventTestCenti(lSample->inspPressureX1),
              (long)ventTestCenti(lSample->peepPressureX1),
              (long)ventTestCenti(lSample->expPressureX1),
              (long)ventTestCenti(lSample->patientPressureX1),
              (long)ventTestCenti(lSample->blowerSpeedX10),
              (long)ventTestCenti(lSample->patientRefPressureX1),
              (long)ventTestCenti(lSample->fastRefX1),
              (long)ventTestCenti(lSample->flowCompensationX1),
              (long)ventTestCenti(lSample->patientCorrectionX1),
              (long)ventTestCenti(lSample->innerEffortX1),
              (long)ventTestCenti(lSample->blowerFeedforwardX1),
              (long)ventTestCenti(lSample->tidalVolumeX10),
              (long)ventTestCenti(lSample->tidalVolumeInspX10),
              (long)ventTestCenti(lSample->tidalVolumeExpX10),
              (unsigned int)lSample->blowerTargetX100,
              (unsigned int)lSample->valveDutyX2,
              (unsigned int)lSample->expirationControllerState,
              (unsigned int)lSample->pressureControllerState);
    }
    LOG_R("VT_TRANSIENT_END,count=%u", (unsigned int)lCount);
}

/** Record one control-cycle sample into the rolling one-second buffer. */
void ventTestTransientRecord(void)
{
    uint32_t lSequence = gVentTestTransientTotalCount;

    gVentTestTransientBuffer[lSequence % VENT_TEST_TRANSIENT_SAMPLE_COUNT] =
        gMonitorWaveformData;
    gVentTestTransientTotalCount = lSequence + 1U;
}

/** Dispatch a ventilation test command. */
static eConsoleCommandResult ventTestConsoleCommand(const char *arguments)
{
    stVentPacSettings lPreviousSettings;
    stVentCpapPsvSettings lPreviousCpapPsvSettings;
    stVentPsvStSettings lPreviousPsvStSettings;
    stVentPacSettings *lPacSettings;
    stVentCpapPsvSettings *lCpapPsvSettings;
    stVentPsvStSettings *lPsvStSettings;
    int8_t lStatus;
    uint16_t lDeltaPressure;
    uint16_t lMode;
    uint16_t lPeep;
    uint16_t lTriggerThreshold;
    uint8_t lRun;
    eVentTriggerType lTriggerType;
    eVentMode lConfiguredMode;

    if (ventTestTokenMatch(&arguments, "pac") &&
        (*ventTestSkipSpaces(arguments) == '\0')) {
        lStatus = breathSchedulerStart(VENT_MD_PAC);
        LOG_I(gVentTestTag, "PAC start status=%d", (int)lStatus);
    } else if (ventTestTokenMatch(&arguments, "vac") &&
               (*ventTestSkipSpaces(arguments) == '\0')) {
        lStatus = breathSchedulerStart(VENT_MD_VAC);
        LOG_I(gVentTestTag, "VAC start status=%d", (int)lStatus);
    } else if (ventTestTokenMatch(&arguments, "psv") &&
               (*ventTestSkipSpaces(arguments) == '\0')) {
        lStatus = breathSchedulerStart(VENT_MD_CPAP_PSV);
        LOG_I(gVentTestTag, "PSV start status=%d", (int)lStatus);
    } else if (ventTestTokenMatch(&arguments, "psvst") &&
               (*ventTestSkipSpaces(arguments) == '\0')) {
        lStatus = breathSchedulerStart(VENT_MD_PSV_ST);
        LOG_I(gVentTestTag, "PSV-ST start status=%d", (int)lStatus);
    } else if (ventTestTokenMatch(&arguments, "stop") &&
               (*ventTestSkipSpaces(arguments) == '\0')) {
        lStatus = breathSchedulerTestRunSet(0U);
        LOG_I(gVentTestTag, "stop status=%d", (int)lStatus);
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
    } else if (ventTestTokenMatch(&arguments, "trigger")) {
        if (ventTestTokenMatch(&arguments, "off") &&
            (*ventTestSkipSpaces(arguments) == '\0')) {
            lTriggerType = VENT_TRIGGER_OFF;
            lTriggerThreshold = 0U;
        } else if (ventTestTokenMatch(&arguments, "pressure") &&
                   ventTestUnsignedParse(&arguments, &lTriggerThreshold) &&
                   (lTriggerThreshold > 0U) &&
                   (*ventTestSkipSpaces(arguments) == '\0')) {
            lTriggerType = VENT_TRIGGER_PRESSURE;
        } else if (ventTestTokenMatch(&arguments, "flow") &&
                   ventTestUnsignedParse(&arguments, &lTriggerThreshold) &&
                   (lTriggerThreshold > 0U) &&
                   (*ventTestSkipSpaces(arguments) == '\0')) {
            lTriggerType = VENT_TRIGGER_FLOW;
        } else {
            ventTestUsageShow();
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        lConfiguredMode = breathSchedulerModeGet();
        (void)breathSchedulerStop();
        if (lConfiguredMode == VENT_MD_CPAP_PSV) {
            lCpapPsvSettings = GetVentCpapPsvSettings();
            lPreviousCpapPsvSettings = *lCpapPsvSettings;
            lCpapPsvSettings->triggerType = lTriggerType;
            if (lTriggerType == VENT_TRIGGER_PRESSURE) {
                lCpapPsvSettings->pressureTriggerCmh2o =
                    -((float)lTriggerThreshold / 100.0F);
            } else if (lTriggerType == VENT_TRIGGER_FLOW) {
                lCpapPsvSettings->flowTriggerLpm =
                    (float)lTriggerThreshold / 100.0F;
            }
            lStatus = breathSchedulerTestModeSet((uint8_t)lConfiguredMode);
            if (lStatus != BREATH_CONTROL_SUCCESS) {
                *lCpapPsvSettings = lPreviousCpapPsvSettings;
                (void)breathSchedulerTestModeSet((uint8_t)lConfiguredMode);
            }
        } else if (lConfiguredMode == VENT_MD_PSV_ST) {
            lPsvStSettings = GetVentPsvStSettings();
            lPreviousPsvStSettings = *lPsvStSettings;
            lPsvStSettings->triggerType = lTriggerType;
            if (lTriggerType == VENT_TRIGGER_PRESSURE) {
                lPsvStSettings->pressureTriggerCmh2o =
                    -((float)lTriggerThreshold / 100.0F);
            } else if (lTriggerType == VENT_TRIGGER_FLOW) {
                lPsvStSettings->flowTriggerLpm =
                    (float)lTriggerThreshold / 100.0F;
            }
            lStatus = breathSchedulerTestModeSet((uint8_t)lConfiguredMode);
            if (lStatus != BREATH_CONTROL_SUCCESS) {
                *lPsvStSettings = lPreviousPsvStSettings;
                (void)breathSchedulerTestModeSet((uint8_t)lConfiguredMode);
            }
        } else {
            lConfiguredMode = VENT_MD_PAC;
            lPacSettings = GetVentPacSettings();
            lPreviousSettings = *lPacSettings;
            lPacSettings->triggerType = lTriggerType;
            if (lTriggerType == VENT_TRIGGER_PRESSURE) {
                lPacSettings->pressureTriggerCmh2o =
                    -((float)lTriggerThreshold / 100.0F);
            } else if (lTriggerType == VENT_TRIGGER_FLOW) {
                lPacSettings->flowTriggerLpm =
                    (float)lTriggerThreshold / 100.0F;
            }
            lStatus = breathSchedulerTestModeSet((uint8_t)lConfiguredMode);
            if (lStatus != BREATH_CONTROL_SUCCESS) {
                *lPacSettings = lPreviousSettings;
                (void)breathSchedulerTestModeSet((uint8_t)lConfiguredMode);
            }
        }
        LOG_I(gVentTestTag,
              "trigger mode=%u type=%u threshold100=%u status=%d",
              (unsigned int)lConfiguredMode,
              (unsigned int)lTriggerType,
              (unsigned int)lTriggerThreshold,
              (int)lStatus);
    } else if (ventTestTokenMatch(&arguments, "mode") &&
               ventTestUnsignedParse(&arguments, &lMode) &&
               (lMode <= UINT8_MAX) &&
               (*ventTestSkipSpaces(arguments) == '\0')) {
        lStatus = breathSchedulerTestModeSet((uint8_t)lMode);
        LOG_I(gVentTestTag, "mode %u status=%d", (unsigned int)lMode, (int)lStatus);
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
    "Ventilation test: select mode or running state",
    ventTestConsoleCommand
};

bool ventTestConsoleRegister(void)
{
    return consoleRegisterCommand(&gVentTestConsoleCommand);
}
/**************************End of file********************************/
