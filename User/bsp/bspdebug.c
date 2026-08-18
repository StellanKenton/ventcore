/************************************************************************************
* @file     : bspdebug.c
* @brief    : BSP debug console command implementation.
* @details  : Reads ADC channels and gets or sets digital valve states.
* @author   :
* @date     :
* @version  :
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "bspdebug.h"

#include <stddef.h>
#include <stdint.h>

#include "adc.h"
#include "console.h"
#include "dvalve.h"
#include "log.h"
#include "valve.h"

static const char *const gBspDebugTag = "bsp";

static const char *const gBspDebugAdcNames[ADC_CH_COUNT] = {
    "sfcur",
    "flow",
    "hw",
    "3v3",
    "temp",
    "24v",
    "peepcur",
    "insp",
    "5v",
    "peep",
    "exp",
    "mdiff",
    "o2cur",
    "26v"
};

static const char *const gBspDebugValveNames[VALVE_COUNT] = {
    "insp",
    "exp",
    "diff",
    "flush"
};

static const char *const gBspDebugDvalveNames[DVALVE_COUNT] = {
    "o2",
    "relief",
    "exp"
};

/**
 * @brief Skip spaces in a command argument string.
 * @param arguments Current argument position.
 * @return First non-space position.
 */
static const char *bspDebugSkipSpaces(const char *arguments)
{
    while ((*arguments == ' ') || (*arguments == '\t')) {
        arguments++;
    }

    return arguments;
}

/**
 * @brief Read the next token without modifying the console buffer.
 * @param arguments Current argument position, updated after the token.
 * @param token Token start address.
 * @param length Token length.
 * @return true when a token was found.
 */
static bool bspDebugTokenRead(const char **arguments, const char **token, uint32_t *length)
{
    const char *lStart = bspDebugSkipSpaces(*arguments);
    const char *lEnd = lStart;

    if (*lStart == '\0') {
        return false;
    }
    while ((*lEnd != '\0') && (*lEnd != ' ') && (*lEnd != '\t')) {
        lEnd++;
    }

    *token = lStart;
    *length = (uint32_t)(lEnd - lStart);
    *arguments = lEnd;
    return true;
}

/**
 * @brief Compare a parsed token with a zero-terminated name.
 * @param token Token start address.
 * @param length Token length.
 * @param name Expected name.
 * @return true when both values match exactly.
 */
static bool bspDebugTokenEqual(const char *token, uint32_t length, const char *name)
{
    uint32_t lIndex;

    for (lIndex = 0U; lIndex < length; lIndex++) {
        if ((name[lIndex] == '\0') || (token[lIndex] != name[lIndex])) {
            return false;
        }
    }

    return name[length] == '\0';
}

/**
 * @brief Find a name in a fixed string table.
 * @param token Token start address.
 * @param length Token length.
 * @param names String table.
 * @param count String table entry count.
 * @return Matching index, or -1 when not found.
 */
static int32_t bspDebugNameFind(const char *token, uint32_t length, const char *const *names, uint32_t count)
{
    uint32_t lIndex;

    for (lIndex = 0U; lIndex < count; lIndex++) {
        if (bspDebugTokenEqual(token, length, names[lIndex])) {
            return (int32_t)lIndex;
        }
    }

    return -1;
}

/**
 * @brief Parse a PWM duty percentage.
 * @param token Token start address.
 * @param length Token length.
 * @param duty Parsed percentage.
 * @return true for a decimal value from 0 through 100.
 */
static bool bspDebugDutyParse(const char *token, uint32_t length, uint8_t *duty)
{
    uint32_t lIndex;
    uint32_t lValue = 0U;

    if (length == 0U) {
        return false;
    }
    for (lIndex = 0U; lIndex < length; lIndex++) {
        if ((token[lIndex] < '0') || (token[lIndex] > '9')) {
            return false;
        }
        lValue = (lValue * 10U) + (uint32_t)(token[lIndex] - '0');
        if (lValue > DVALVE_DUTY_MAX_PERCENT) {
            return false;
        }
    }

    *duty = (uint8_t)lValue;
    return true;
}

/**
 * @brief Print the compact BSP command syntax.
 */
static void bspDebugUsageShow(void)
{
    LOG_I(gBspDebugTag, "adc: bsp a|adc <sfcur|flow|hw|3v3|temp|24v|peepcur|insp|5v|peep|exp|mdiff|o2cur|26v>");
    LOG_I(gBspDebugTag, "valve: bsp v|valve <insp|exp|diff|flush> [0|1]");
    LOG_I(gBspDebugTag, "dvalve: bsp d|dv|dvalve <o2|relief|exp> <0-100>");
}

/**
 * @brief Handle an ADC value query.
 * @param arguments Arguments following the ADC subcommand.
 * @return Console command result.
 */
static eConsoleCommandResult bspDebugAdcCommand(const char *arguments)
{
    const char *lToken;
    uint32_t lLength;
    int32_t lIndex;

    if (!bspDebugTokenRead(&arguments, &lToken, &lLength)) {
        bspDebugUsageShow();
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }
    lIndex = bspDebugNameFind(lToken, lLength, gBspDebugAdcNames, (uint32_t)ADC_CH_COUNT);
    if ((lIndex < 0) || bspDebugTokenRead(&arguments, &lToken, &lLength)) {
        bspDebugUsageShow();
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    LOG_I(gBspDebugTag, "adc %s %u", gBspDebugAdcNames[lIndex], (unsigned int)adc_value[lIndex]);
    return CONSOLE_COMMAND_RESULT_OK;
}

/**
 * @brief Handle a digital valve state query or control request.
 * @param arguments Arguments following the valve subcommand.
 * @return Console command result.
 */
static eConsoleCommandResult bspDebugValveCommand(const char *arguments)
{
    const char *lToken;
    uint32_t lLength;
    int32_t lIndex;
    eValveLevel lLevel;
    int8_t lStatus;

    if (!bspDebugTokenRead(&arguments, &lToken, &lLength)) {
        bspDebugUsageShow();
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }
    lIndex = bspDebugNameFind(lToken, lLength, gBspDebugValveNames, (uint32_t)VALVE_COUNT);
    if (lIndex < 0) {
        bspDebugUsageShow();
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (!bspDebugTokenRead(&arguments, &lToken, &lLength)) {
        lStatus = valveStateGet((eValveIndex)lIndex, &lLevel);
        if (lStatus != VALVE_STATUS_OK) {
            LOG_E(gBspDebugTag, "valve %s read failed: %d", gBspDebugValveNames[lIndex], (int)lStatus);
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
        LOG_I(gBspDebugTag, "valve %s %u", gBspDebugValveNames[lIndex], (unsigned int)lLevel);
        return CONSOLE_COMMAND_RESULT_OK;
    }

    if ((lLength != 1U) || ((lToken[0] != '0') && (lToken[0] != '1')) ||
        bspDebugTokenRead(&arguments, &lToken, &lLength)) {
        bspDebugUsageShow();
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lLevel = (lToken[0] == '1') ? VALVE_LEVEL_HIGH : VALVE_LEVEL_LOW;
    lStatus = valveControlSet((eValveIndex)lIndex, lLevel);
    if (lStatus != VALVE_STATUS_OK) {
        LOG_E(gBspDebugTag, "valve %s set failed: %d", gBspDebugValveNames[lIndex], (int)lStatus);
        return CONSOLE_COMMAND_RESULT_ERROR;
    }
    LOG_I(gBspDebugTag, "valve %s set %u", gBspDebugValveNames[lIndex], (unsigned int)lLevel);
    return CONSOLE_COMMAND_RESULT_OK;
}

/**
 * @brief Handle a proportional valve PWM duty request.
 * @param arguments Arguments following the dvalve subcommand.
 * @return Console command result.
 */
static eConsoleCommandResult bspDebugDvalveCommand(const char *arguments)
{
    const char *lToken;
    uint32_t lLength;
    int32_t lIndex;
    uint8_t lDuty;
    int8_t lStatus;

    if (!bspDebugTokenRead(&arguments, &lToken, &lLength)) {
        bspDebugUsageShow();
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }
    lIndex = bspDebugNameFind(lToken, lLength, gBspDebugDvalveNames, (uint32_t)DVALVE_COUNT);
    if ((lIndex < 0) || !bspDebugTokenRead(&arguments, &lToken, &lLength) ||
        !bspDebugDutyParse(lToken, lLength, &lDuty) ||
        bspDebugTokenRead(&arguments, &lToken, &lLength)) {
        bspDebugUsageShow();
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lStatus = dvalveDutySet((eDvalveIndex)lIndex, lDuty);
    if (lStatus != DVALVE_STATUS_OK) {
        LOG_E(gBspDebugTag, "dvalve %s set failed: %d", gBspDebugDvalveNames[lIndex], (int)lStatus);
        return CONSOLE_COMMAND_RESULT_ERROR;
    }
    LOG_I(gBspDebugTag, "dvalve %s %u%%", gBspDebugDvalveNames[lIndex], (unsigned int)lDuty);
    return CONSOLE_COMMAND_RESULT_OK;
}

/**
 * @brief Dispatch the BSP debug subcommand.
 * @param arguments Arguments following the bsp command.
 * @return Console command result.
 */
static eConsoleCommandResult bspDebugConsoleCommand(const char *arguments)
{
    const char *lToken;
    uint32_t lLength;

    if (!bspDebugTokenRead(&arguments, &lToken, &lLength)) {
        bspDebugUsageShow();
        return CONSOLE_COMMAND_RESULT_OK;
    }
    if (bspDebugTokenEqual(lToken, lLength, "a") || bspDebugTokenEqual(lToken, lLength, "adc")) {
        return bspDebugAdcCommand(arguments);
    }
    if (bspDebugTokenEqual(lToken, lLength, "v") || bspDebugTokenEqual(lToken, lLength, "valve")) {
        return bspDebugValveCommand(arguments);
    }
    if (bspDebugTokenEqual(lToken, lLength, "d") || bspDebugTokenEqual(lToken, lLength, "dv") ||
        bspDebugTokenEqual(lToken, lLength, "dvalve")) {
        return bspDebugDvalveCommand(arguments);
    }

    bspDebugUsageShow();
    return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
}

static const stConsoleCommand gBspDebugConsoleCommand = {
    "bsp",
    "BSP debug: adc values and valve states",
    bspDebugConsoleCommand
};

/**
 * @brief Register the BSP debug command with the RTT console.
 * @return true when registration succeeds.
 */
bool bspDebugConsoleRegister(void)
{
    return consoleRegisterCommand(&gBspDebugConsoleCommand);
}

/**************************End of file********************************/
