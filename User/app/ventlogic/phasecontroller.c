/************************************************************************************
* @file     : phasecontroller.c
* @brief    : Breath phase controller.
* @details  : Provides phase controller initialization and periodic processing.
* @author   :
* @date     : 2026-08-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "phasecontroller.h"
#include "breathscheduler.h"
#include "controldata.h"

stPhaseController gPhaseController;
static float gPhaseData[PHASE_COUNT];

/** Start an inspiratory rise from the current patient pressure. */
static void phaseControllerStartInspRise(void)
{
    float lPatientPressure = controlDataGet(PAT_REAL_PRS);

    gPhaseController.inspRiseTimeTicks = 0U;
    gPhaseController.inspHoldTimeTicks = 0U;
    gPhaseController.inspRiseStartPressure = lPatientPressure;
    (void)phaseControlSet(PHASE_REF_PRESSURE, lPatientPressure);
    (void)phaseControlSet(PHASE_REF_FAST_PRESSURE, lPatientPressure);
    gPhaseController.runState = PHASE_INSP_RISE;
}

void phaseControllerInit(void)
{
    gPhaseController.runState = PHASE_IDLE;
    gPhaseController.inspRiseTimeTicks = 0U;
    gPhaseController.inspHoldTimeTicks = 0U;
    gPhaseController.expPeepTimeTicks = 0U;
    gPhaseController.inspRiseStartPressure = 0.0F;
    (void)phaseControlSet(PHASE_REF_PRESSURE, 0.0F);
    (void)phaseControlSet(PHASE_REF_FAST_PRESSURE, 0.0F);
}

int8_t phaseControlSet(ePhaseControlType type, float value)
{
    if ((type <= PHASE_NONE) || (type >= PHASE_COUNT)) {
        return PHASE_CONTROL_ERROR_PARAM;
    }

    gPhaseData[type] = value;
    return PHASE_CONTROL_SUCCESS;
}

float phaseControlGet(ePhaseControlType type)
{
    if ((type <= PHASE_NONE) || (type >= PHASE_COUNT)) {
        return 0.0F;
    }

    return gPhaseData[type];
}

ePhaseControllerState phaseControllerStateGet(void)
{
    return gPhaseController.runState;
}

void phaseControllerProcess(uint8_t tickCount)
{
    if (breathControlGet(BREATH_RUN) != 1.0F) {
        gPhaseController.runState = PHASE_IDLE;
        gPhaseController.inspRiseTimeTicks = 0U;
        gPhaseController.inspHoldTimeTicks = 0U;
        gPhaseController.expPeepTimeTicks = 0U;
        (void)phaseControlSet(PHASE_REF_PRESSURE, 0.0F);
        (void)phaseControlSet(PHASE_REF_FAST_PRESSURE, 0.0F);
        return;
    }

    switch (gPhaseController.runState)
    {
        case PHASE_IDLE:
            if(breathControlGet(BREATH_RUN) == 1.0f){
                gPhaseController.expPeepTimeTicks = 0;
                gPhaseController.inspHoldTimeTicks = 0;
                gPhaseController.inspRiseTimeTicks = 0;
                gPhaseController.runState = PHASE_EXP_RELEASE;
            }
            break;
        case PHASE_INSP_RISE:
        {
            float lRiseTime = breathControlGet(BREATH_INSP_RISE_TIME);
            float lTargetPressure = breathControlGet(BREATH_INSP_PRESSURE);
            float lElapsedTime = (float)gPhaseController.inspRiseTimeTicks + (float)tickCount;
            float lTimeProgress;
            float lRemaining;
            float lCurveProgress;
            float lFastProgress;
            float lPressureRange;
            float lReferencePressure;
            float lFastReferencePressure;

            if ((lRiseTime <= 0.0F) || (lElapsedTime >= lRiseTime)) {
                (void)phaseControlSet(PHASE_REF_PRESSURE, lTargetPressure);
                (void)phaseControlSet(PHASE_REF_FAST_PRESSURE, lTargetPressure);
                gPhaseController.runState = PHASE_INSP_HOLD;
                break;
            }

            gPhaseController.inspRiseTimeTicks += tickCount;
            lTimeProgress = lElapsedTime / lRiseTime;
            lRemaining = 1.0F - lTimeProgress;
            /* Lightweight charging curve: about 61% at T/3 and 79% at T/2. */
            lCurveProgress = 1.0F - ((lRemaining * lRemaining)
                                   * (0.65F + (0.35F * lRemaining)));
            /* Reach 80% at T/3, then rise slowly to the target. */
            if (lTimeProgress <= (1.0F / 3.0F)) {
                lFastProgress = 2.4F * lTimeProgress;
            } else {
                lFastProgress = 0.7F + (0.3F * lTimeProgress);
            }
            lPressureRange = lTargetPressure - gPhaseController.inspRiseStartPressure;
            lReferencePressure = gPhaseController.inspRiseStartPressure
                               + (lPressureRange * lCurveProgress);
            lFastReferencePressure = gPhaseController.inspRiseStartPressure
                                   + (lPressureRange * lFastProgress);
            (void)phaseControlSet(PHASE_REF_PRESSURE, lReferencePressure);
            (void)phaseControlSet(PHASE_REF_FAST_PRESSURE, lFastReferencePressure);
            break;
        }
        case PHASE_INSP_HOLD:
        {
            float lTargetPressure = breathControlGet(BREATH_INSP_PRESSURE);

            (void)phaseControlSet(PHASE_REF_PRESSURE, lTargetPressure);
            (void)phaseControlSet(PHASE_REF_FAST_PRESSURE, lTargetPressure);
            gPhaseController.inspHoldTimeTicks += tickCount;
            if(gPhaseController.inspHoldTimeTicks >= (uint16_t)breathControlGet(BREATH_INSP_HOLD_TIME)){
                gPhaseController.expPeepTimeTicks = 0U;
                gPhaseController.runState = PHASE_EXP_RELEASE;
            }
            break;
        }
        case PHASE_EXP_RELEASE:
        {
            float lPeepPressure = breathControlGet(BREATH_PEEP_PRESSURE);
            float lPatientPressure = controlDataGet(PAT_REAL_PRS);

            (void)phaseControlSet(PHASE_REF_PRESSURE, lPeepPressure);
            gPhaseController.expPeepTimeTicks += tickCount;
            if ((lPatientPressure <= (lPeepPressure + PHASE_EXP_PEEP_ENTRY_MARGIN)) ||
                (gPhaseController.expPeepTimeTicks >= PHASE_EXP_RELEASE_MAX_TIME_MS)) {
                gPhaseController.runState = PHASE_EXP_PEEP;
            }
            break;
        }
        case PHASE_EXP_PEEP:
            gPhaseController.expPeepTimeTicks += tickCount;
            if(gPhaseController.expPeepTimeTicks >= (uint16_t)breathControlGet(BREATH_INSP_PEEP_TIME)){
                phaseControllerStartInspRise();
            }
            break;
        default:
            gPhaseController.runState = PHASE_IDLE;
            break;
    }

}

/**************************End of file********************************/
