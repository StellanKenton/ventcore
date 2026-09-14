"""Host regression for VAC EMA feedback, plan timing and bounded adaptation."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HARNESS = r'''
/************************************************************************************
* @file     : vti_compensation_test.c
* @brief    : Real scheduler, phase, monitor and flow-controller feedback regression.
***********************************************************************************/
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include "breathscheduler.h"
#include "phasecontroller.h"
#include "triggerengine.h"
#include "cycleengine.h"
#include "apneaengine.h"
#include "physalarmmanager.h"
#include "expirationcontroller.h"
#include "monitorengine.h"
#include "flowcontroller.h"
#include "controldata.h"
#include "databus.h"
#include "calibration.h"
#include "calibtrans.h"
#include "log.h"
#include "rtos.h"

static float gData[CONTROL_DATA_COUNT];
static float gOffset;
static float gInspiratoryFlow = 40.0F;
static uint32_t gNow;
static float gQuietFlow;

float controlDataGet(ControlData_Index_EnumDef index) { return gData[index]; }
float controlDataMdiffFlowZeroOffsetGet(void) { return gOffset; }
int8_t controlDataMdiffFlowZeroOffsetSet(float offset) {
    gOffset = offset;
    return DATABUS_STATUS_OK;
}
uint8_t calibrationIsValid(eCalibrationType type) { (void)type; return 1U; }
int8_t calibtransPrsSpeed(float pressure, float *speed) {
    *speed = pressure * 10.0F;
    return CALIBTRANS_STATUS_OK;
}
void logWrite(eLogLevel level, const char *tag, const char *format, ...) {
    (void)level; (void)tag; (void)format;
}
void repRtosEnterCritical(void) {}
void repRtosExitCritical(void) {}

/** Compare physical values with tolerance for single-precision integration. */
static void near(float actual, float expected) { assert(fabsf(actual - expected) < 0.02F); }

/** Fetch a real next plan in scheduler-only tests. */
static stBreathPlan next(void) {
    stBreathPlan lPlan;
    assert(breathSchedulerNextPlanGet(BREATH_TRIGGER_REASON_TIME, &lPlan) == BREATH_CONTROL_SUCCESS);
    return lPlan;
}

/** Restore user settings and all production modules. */
static void reset(void) {
    *GetVentVacSettings() = (stVentVacSettings){.oxygen = 21.0F, .peep = 5.0F,
        .freq = 15.0F, .inspTimeMs = 2000U, .tidalVolume = 500.0F,
        .triggerType = VENT_TRIGGER_OFF, .inspPausePct = 50.0F};
    GetVentLimitSettings()->pressureHigh = 50.0F;
    gOffset = 0.0F;
    gQuietFlow = 0.0F;
    gNow = 0U;
    gInspiratoryFlow = 40.0F;
    assert(breathSchedulerInit() == BREATH_CONTROL_SUCCESS);
    phaseControllerInit();
    monitorEngineInit();
    flowControllerInit();
    assert(breathSchedulerStart(VENT_MD_VAC) == BREATH_CONTROL_SUCCESS);
}

/** Verify EMA initialization, exact update, once-only consumption and conversion. */
static void testEma(void) {
    stBreathPlan lPlan;
    stBreathPlan lNext;
    reset();
    lPlan = next();
    near(lPlan.deliveryTargetMl, 500.0F);
    near(lPlan.filteredVtiMl, 0.0F);
    breathSchedulerVolumeFeedback(&lPlan, 400.0F, 1U);
    breathSchedulerVolumeFeedback(&lPlan, 1.0F, 1U); /* Same sequence is ignored. */
    lNext = next();
    near(lNext.filteredVtiMl, 400.0F);
    near(lNext.volumeCorrectionMl, 80.0F);
    near(lNext.deliveryTargetMl, 580.0F);
    near(lNext.targetTidalVolumeMl, 500.0F);
    near(lNext.inspiratoryFlowLpm, 610.0F * 60.0F / 958.55F);
    assert(lNext.riseTimeMs == 1000U && lNext.holdTimeMs == 1000U);
    breathSchedulerVolumeFeedback(&lPlan, 1.0F, 1U); /* Older plan is ignored. */
    breathSchedulerVolumeFeedback(&lNext, 600.0F, 1U);
    lPlan = next();
    near(lPlan.filteredVtiMl, 500.0F);
    near(lPlan.volumeCorrectionMl, 40.0F);
    breathSchedulerVolumeFeedback(&lPlan, NAN, 1U);
    lPlan = next();
    near(lPlan.filteredVtiMl, 500.0F);
    near(lPlan.volumeCorrectionMl, 40.0F);
    breathSchedulerVolumeFeedback(&lPlan, 100.0F, 0U);
    lPlan = next();
    near(lPlan.filteredVtiMl, 500.0F);
    near(lPlan.volumeCorrectionMl, 40.0F);
    breathSchedulerVolumeFeedback(&lPlan, 0.0F, 1U);
    lPlan = next();
    near(lPlan.volumeCorrectionMl, 40.0F);
}

/** Settings epochs, live limits, modes and restarts must discard old feedback. */
static void testReset(void) {
    stBreathPlan lPlan;
    stBreathPlan lOld;
    unsigned int lIndex;
    for (lIndex = 0U; lIndex < 5U; lIndex++) {
        reset();
        lOld = next();
        breathSchedulerVolumeFeedback(&lOld, 400.0F, 1U);
        if (lIndex == 0U) { GetVentVacSettings()->tidalVolume = 600.0F; }
        if (lIndex == 1U) { GetVentVacSettings()->inspTimeMs = 1500U; }
        if (lIndex == 2U) { GetVentVacSettings()->inspPausePct = 25.0F; }
        if (lIndex == 3U) { GetVentVacSettings()->peep = 10.0F; }
        if (lIndex == 4U) { GetVentLimitSettings()->pressureHigh = 40.0F; }
        breathSchedulerProcess();
        breathSchedulerVolumeFeedback(&lOld, 10.0F, 1U);
        lPlan = next();
        near(lPlan.volumeCorrectionMl, 0.0F);
        near(lPlan.filteredVtiMl, 0.0F);
    }
    reset();
    lOld = next();
    breathSchedulerVolumeFeedback(&lOld, 400.0F, 1U);
    assert(breathSchedulerStop() == BREATH_CONTROL_SUCCESS);
    assert(breathSchedulerStart(VENT_MD_VAC) == BREATH_CONTROL_SUCCESS);
    breathSchedulerVolumeFeedback(&lOld, 10.0F, 1U);
    lPlan = next();
    near(lPlan.volumeCorrectionMl, 0.0F);
    breathSchedulerVolumeFeedback(&lPlan, 400.0F, 1U);
    assert(breathSchedulerSettingsUpdate(VENT_MD_PAC) == BREATH_CONTROL_SUCCESS);
    lPlan = next();
    near(lPlan.volumeCorrectionMl, 0.0F);
    assert(breathSchedulerSettingsUpdate(VENT_MD_VAC) == BREATH_CONTROL_SUCCESS);
    lPlan = next();
    near(lPlan.filteredVtiMl, 0.0F);
    /* A fractional delivery time retains the original effective-time formula. */
    GetVentVacSettings()->inspTimeMs = 1001U;
    GetVentVacSettings()->inspPausePct = 33.0F;
    breathSchedulerProcess();
    lPlan = next();
    #if BREATH_VOLUME_FLOW_COMPENSATION_ENABLE
    near(lPlan.inspiratoryFlowLpm, 530.0F * 60.0F / (670.67F - 41.45F));
#else
    near(lPlan.inspiratoryFlowLpm, 500.0F * 60.0F / 670.67F);
#endif
}

/** Exercise a simple repeatable volume loss, bounds and the no-chasing deadband. */
static void testConvergence(void) {
    stBreathPlan lPlan;
    float lMeasured;
    float lPrevious;
    unsigned int lIndex;
    reset();
    lPlan = next();
    for (lIndex = 0U; lIndex < 80U; lIndex++) {
        lMeasured = lPlan.deliveryTargetMl - 80.0F;
        lPrevious = lPlan.volumeCorrectionMl;
        breathSchedulerVolumeFeedback(&lPlan, lMeasured, 1U);
        lPlan = next();
        assert(fabsf(lPlan.volumeCorrectionMl - lPrevious) <= 125.01F);
        assert(fabsf(lPlan.volumeCorrectionMl) <= 150.01F);
        if (lIndex == 0U) { near(lPlan.volumeCorrectionMl, 64.0F); }
        if (lIndex == 1U) { near(lMeasured, 484.0F); }
        if (lIndex > 1U) { near(lMeasured, 500.0F); }
    }
    assert(fabsf(lMeasured - 500.0F) <= 5.1F);
    /* Huge persistent mismatch stays bounded in both directions. */
    reset();
    lPlan = next();
    for (lIndex = 0U; lIndex < 30U; lIndex++) {
        breathSchedulerVolumeFeedback(&lPlan, 1.0F, 1U);
        lPlan = next();
    }
    near(lPlan.volumeCorrectionMl, 150.0F);
    reset();
    lPlan = next();
    for (lIndex = 0U; lIndex < 30U; lIndex++) {
        breathSchedulerVolumeFeedback(&lPlan, 1000.0F, 1U);
        lPlan = next();
    }
    near(lPlan.volumeCorrectionMl, -150.0F);
    reset();
    lPlan = next();
    breathSchedulerVolumeFeedback(&lPlan, 498.0F, 1U);
    lPlan = next();
    near(lPlan.filteredVtiMl, 498.0F);
    near(lPlan.volumeCorrectionMl, 0.0F);
}

/** Match production scheduling order with prescribed proximal measurements. */
static void step(void) {
    ePhaseControllerState lPhase;
    gNow += 6U;
    breathSchedulerProcess();
    phaseControllerProcess(gNow);
    lPhase = phaseControllerStateGet();
    gData[PAT_REAL_FLOW] = (lPhase == PHASE_INSP) ? gInspiratoryFlow :
                           (lPhase == PHASE_EXP) ? -20.0F : gQuietFlow;
    gData[PAT_REAL_PRS] = (lPhase == PHASE_INSP) ? 25.0F :
                         (lPhase == PHASE_EXP) ? 5.0F : 0.0F;
    monitorEngineProcess(gNow);
}

/** Wait for a new actual inspiration, not merely the initial expiration plan. */
static stBreathPlan advance(uint32_t previousSequence) {
    stBreathPlan lPlan;
    unsigned int lIndex;
    for (lIndex = 0U; lIndex < 3000U; lIndex++) {
        step();
        if ((phaseControllerStateGet() == PHASE_INSP) &&
            (phaseControllerActivePlanGet(&lPlan) == PHASE_CONTROL_SUCCESS) &&
            (lPlan.sequence != previousSequence)) {
            return lPlan;
        }
    }
    assert(0);
    return (stBreathPlan){0};
}

/** Verify same-boundary feedback, saturation rejection and re-zero recovery. */
static void testIntegration(void) {
    stBreathPlan lPlan;
    stBreathResult lResult;
    stActuatorRequest lRequest;
    float lFiltered;
    float lCorrection;
    reset();
    GetVentVacSettings()->inspTimeMs = 600U;
    GetVentVacSettings()->inspPausePct = 0.0F;
    GetVentVacSettings()->freq = 60.0F;
    gQuietFlow = 1.5F; /* Real startup zero correction after initial plan load. */
    gData[INSP_REAL_FLOW] = 0.0F;
    gData[PAT_REAL_FLOW] = 0.0F;
    gData[PAT_REAL_PRS] = 0.0F;
    lPlan = advance(0U);
    near(lPlan.filteredVtiMl, 0.0F);
    near(phaseControlGet(PHASE_REF_VOLUME), 0.0F);
    for (unsigned int lIndex = 0U; lIndex < 50U; lIndex++) { step(); }
    near(phaseControlGet(PHASE_REF_VOLUME), 500.0F * 258.55F / 558.55F);
    lPlan = advance(lPlan.sequence);
    near(phaseControlGet(PHASE_REF_VOLUME), 0.0F);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert(lResult.sequence + 1U == lPlan.sequence);
    near(lResult.vtiMl, 400.0F);
    near(lPlan.filteredVtiMl, lResult.vtiMl);
#if BREATH_VOLUME_FLOW_COMPENSATION_ENABLE
    near(lPlan.volumeCorrectionMl, 80.0F);
#else
    near(lPlan.volumeCorrectionMl, 500.0F * 20.0F / 600.0F);
#endif
    gInspiratoryFlow = 60.0F;
    lFiltered = lPlan.filteredVtiMl;
    lPlan = advance(lPlan.sequence);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    near(lPlan.filteredVtiMl, lFiltered + 0.5F * (lResult.vtiMl - lFiltered));
    lFiltered = lPlan.filteredVtiMl;
    lCorrection = lPlan.volumeCorrectionMl;
    /* Drive the real flow controller into its feedforward pressure limit. */
    gData[PAT_REAL_PRS] = 60.0F;
    assert(flowControllerProcess(&lPlan, &lRequest) == ACTUATOR_REQUEST_SUCCESS);
    lPlan = advance(lPlan.sequence);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert((lResult.validMask & BREATH_RESULT_VOLUME_LIMITED) != 0U);
    near(lPlan.filteredVtiMl, lFiltered);
    near(lPlan.volumeCorrectionMl, lCorrection);
    gData[PAT_REAL_FLOW] = NAN;
    monitorEngineProcess(gNow);
    lPlan = advance(lPlan.sequence);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert((lResult.validMask & BREATH_RESULT_VALID_VTI) == 0U);
    near(lPlan.volumeCorrectionMl, lCorrection);
    gOffset += 1.0F;
    step();
    lPlan = advance(lPlan.sequence);
    near(lPlan.filteredVtiMl, 0.0F);
    near(lPlan.volumeCorrectionMl, 0.0F);
}

/** A delivery shorter than the ramp still reaches user VT without dividing by zero. */
static void testShortVolumeReference(void) {
    reset();
    GetVentVacSettings()->inspTimeMs = 1000U;
    GetVentVacSettings()->inspPausePct = 99.0F;
    gData[PAT_REAL_FLOW] = 0.0F;
    gData[INSP_REAL_FLOW] = 0.0F;
    gData[PAT_REAL_PRS] = 0.0F;
    (void)advance(0U);
    near(phaseControlGet(PHASE_REF_VOLUME), 0.0F);
    step();
    near(phaseControlGet(PHASE_REF_VOLUME), 180.0F);
    step();
    assert(phaseControllerVolumePauseActiveGet() != 0U);
    near(phaseControlGet(PHASE_REF_VOLUME), 500.0F);
    assert(breathSchedulerStop() == BREATH_CONTROL_SUCCESS);
    step();
    near(phaseControlGet(PHASE_REF_VOLUME), 0.0F);
}


/** Exercise fixed flow, both adaptation directions and all requested targets. */
static void testTimeCompensation(void) {
    const float lTargets[] = {300.0F, 500.0F, 800.0F};
    for (unsigned int lTarget = 0U; lTarget < 3U; lTarget++) {
        for (int lDirection = -1; lDirection <= 1; lDirection += 2) {
            reset();
            GetVentVacSettings()->tidalVolume = lTargets[lTarget];
            breathSchedulerProcess();
            stBreathPlan lPlan = next();
            float lFlow = lPlan.inspiratoryFlowLpm;
            float lMeasured = 0.0F;
            for (unsigned int lIndex = 0U; lIndex < 50U; lIndex++) {
                uint32_t lPrevious = lPlan.riseTimeMs;
                /* Independent constant-flow plant with a signed 100 ms area error. */
                lMeasured = lFlow * ((float)lPlan.riseTimeMs - lDirection * 100.0F) / 60.0F;
                breathSchedulerVolumeFeedback(&lPlan, lMeasured, 1U);
                lPlan = next();
                near(lPlan.inspiratoryFlowLpm, lFlow);
                assert(abs((int)lPlan.riseTimeMs - (int)lPrevious) <= 20);
                assert(lPlan.holdTimeMs == 1000U);
                assert(lPlan.maximumInspiratoryTimeMs == lPlan.riseTimeMs + lPlan.holdTimeMs);
                assert(lPlan.maximumInspiratoryTimeMs + lPlan.expiratoryTimeMs == 4000U);
                assert(lPlan.expiratoryTimeMs >= lPlan.minimumExpiratoryTimeMs);
                assert(lPlan.riseTimeMs >= 700U && lPlan.riseTimeMs <= 1300U);
            }
            assert(fabsf(lMeasured - lTargets[lTarget]) <= lTargets[lTarget] * 0.005F + 1.0F);
            float lCorrection = lPlan.volumeCorrectionMl;
            breathSchedulerVolumeFeedback(&lPlan, NAN, 1U);
            lPlan = next();
            near(lPlan.volumeCorrectionMl, lCorrection);
            breathSchedulerVolumeFeedback(&lPlan, 1.0F, 0U);
            lPlan = next();
            near(lPlan.volumeCorrectionMl, lCorrection);
            GetVentVacSettings()->peep += 1.0F;
            breathSchedulerProcess();
            lPlan = next();
            near(lPlan.volumeCorrectionMl, 0.0F);
            for (unsigned int lIndex = 0U; lIndex < 100U; lIndex++) {
                breathSchedulerVolumeFeedback(&lPlan, 1.0F, 1U);
                lPlan = next();
            }
            assert(lPlan.riseTimeMs == 1300U);
        }
    }
    /* No positive adaptation may consume the mandatory expiration reserve. */
    reset();
    GetVentVacSettings()->freq = 60.0F;
    GetVentVacSettings()->inspTimeMs = 800U;
    GetVentVacSettings()->inspPausePct = 0.0F;
    breathSchedulerProcess();
    stBreathPlan lPlan = next();
    for (unsigned int lIndex = 0U; lIndex < 30U; lIndex++) {
        breathSchedulerVolumeFeedback(&lPlan, 1.0F, 1U);
        lPlan = next();
        assert(lPlan.expiratoryTimeMs >= BREATH_PEEP_LOCK_TIME_MS);
        assert(lPlan.maximumInspiratoryTimeMs + lPlan.expiratoryTimeMs == 1000U);
    }
    assert(lPlan.maximumInspiratoryTimeMs == 808U);
}

/** Verify VAC efforts start volume breaths and restart the mandatory timer. */
static void testVacTrigger(void) {
    stBreathPlan lPlan;
    uint32_t lExpirationStart;
    uint32_t lInspirationStart;
    for (unsigned int lCase = 0U; lCase < 5U; lCase++) {
        unsigned int lType = lCase >= 3U ? VENT_TRIGGER_FLOW : lCase;
        reset();
        triggerEngineInit();
        GetVentVacSettings()->triggerType = (eVentTriggerType)lType;
        GetVentVacSettings()->flowTriggerLpm = lCase == 3U ? 10.0F : 1.0F;
        GetVentVacSettings()->pressureTriggerCmh2o = -1.0F;
        breathSchedulerProcess();
        gData[INSP_REAL_FLOW] = 0.0F;
        gData[PAT_REAL_FLOW] = 0.0F;
        gData[PAT_REAL_PRS] = 0.0F;
        for (gNow = 0U; gNow < 2000U; gNow += 6U) {
            phaseControllerProcess(gNow);
            if (phaseControllerStateGet() == PHASE_EXP) { break; }
        }
        assert(phaseControllerStateGet() == PHASE_EXP);
        lExpirationStart = gNow;
        assert(phaseControllerExpirationCaptureNotify() == PHASE_CONTROL_SUCCESS);
        gData[PAT_REAL_PRS] = 5.0F;
        gData[PAT_REAL_FLOW] = lCase == 3U ? -25.0F : 0.0F;
        for (unsigned int lIndex = 0U; lIndex < 10U; lIndex++, gNow += 6U) {
            phaseControllerProcess(gNow);
            triggerEngineProcess(gNow);
        }
        gData[PAT_REAL_FLOW] = lCase == 3U ? -14.0F : 1.1F;
        gData[PAT_REAL_PRS] = 3.9F;
        for (; gNow < lExpirationStart + BREATH_PEEP_LOCK_TIME_MS; gNow += 6U) {
            phaseControllerProcess(gNow);
            triggerEngineProcess(gNow);
            assert(phaseControllerStateGet() == PHASE_EXP);
        }
        assert(phaseControllerActivePlanGet(&lPlan) == PHASE_CONTROL_SUCCESS);
        for (; gNow <= lExpirationStart + lPlan.expiratoryTimeMs + 6U; gNow += 6U) {
            phaseControllerProcess(gNow);
            triggerEngineProcess(gNow);
            if (phaseControllerStateGet() == PHASE_INSP) { break; }
        }
        assert(phaseControllerStateGet() == PHASE_INSP);
        assert(phaseControllerActivePlanGet(&lPlan) == PHASE_CONTROL_SUCCESS);
        assert(lPlan.mode == VENT_MD_VAC);
        assert(lPlan.breathType == BREATH_TYPE_MANDATORY_VOLUME);
        assert(lPlan.triggerReason == (lType == VENT_TRIGGER_OFF ? BREATH_TRIGGER_REASON_TIME :
               lType == VENT_TRIGGER_FLOW ? BREATH_TRIGGER_REASON_FLOW : BREATH_TRIGGER_REASON_PRESSURE));
        near(lPlan.targetTidalVolumeMl, 500.0F);
        assert(lPlan.timeTriggerEnabled == 1U && lPlan.cycleType == BREATH_CYCLE_TYPE_TIME);
        lInspirationStart = gNow;
        gData[PAT_REAL_FLOW] = 0.0F;
        gData[PAT_REAL_PRS] = 5.0F;
        phaseControllerProcess(gNow + 100U);
        assert(phaseControlGet(PHASE_REF_FLOW) > 0.0F);
        gNow = lInspirationStart + lPlan.maximumInspiratoryTimeMs;
        phaseControllerProcess(gNow);
        assert(phaseControllerStateGet() == PHASE_EXP);
        lExpirationStart = gNow;
        phaseControllerProcess(lExpirationStart + lPlan.expiratoryTimeMs - 1U);
        assert(phaseControllerStateGet() == PHASE_EXP);
        phaseControllerProcess(lExpirationStart + lPlan.expiratoryTimeMs);
        assert(phaseControllerStateGet() == PHASE_INSP);
        assert(phaseControllerActivePlanGet(&lPlan) == PHASE_CONTROL_SUCCESS);
        assert(lPlan.triggerReason == BREATH_TRIGGER_REASON_TIME);
        assert(lPlan.breathType == BREATH_TYPE_MANDATORY_VOLUME);
    }
}

/** Exercise production PSV flow cycling, timeout, apnea and ST backup selection. */
static void testPsv(void) {
    stBreathPlan lPlan;
    for (unsigned int lCase = 0U; lCase < 5U; lCase++) {
        reset();
        triggerEngineInit();
        cycleEngineInit();
        apneaEngineInit();
        gData[INSP_REAL_FLOW] = 0.0F;
        gData[PAT_REAL_FLOW] = 0.0F;
        gData[PAT_REAL_PRS] = 0.0F;
        GetVentCpapPsvSettings()->triggerType = lCase >= 3U ? VENT_TRIGGER_FLOW : VENT_TRIGGER_PRESSURE;
        assert(breathSchedulerStart(VENT_MD_CPAP_PSV) == BREATH_CONTROL_SUCCESS);
        for (gNow = 0U; gNow < 2000U; gNow += 6U) {
            phaseControllerProcess(gNow);
            if (phaseControllerStateGet() == PHASE_EXP) { break; }
        }
        assert(phaseControllerStateGet() == PHASE_EXP);
        if (lCase != 4U) {
            assert(phaseControllerExpirationCaptureNotify() == PHASE_CONTROL_SUCCESS);
        }
        gData[PAT_REAL_PRS] = 5.0F;
        gData[PAT_REAL_FLOW] = 0.0F;
        for (unsigned int lIndex = 0; lIndex < 50; lIndex++, gNow += 6U) {
            phaseControllerProcess(gNow);
            triggerEngineProcess(gNow);
        }
        if ((lCase == 2U) || (lCase == 4U)) {
            uint32_t lBackupStart;
            GetVentLimitSettings()->apneaTimeAlarm = 2U;
            apneaEngineProcess(gNow);
            gNow += 1999U;
            apneaEngineProcess(gNow);
            assert(!physAlarmApneaDetect(gNow));
            gNow++;
            apneaEngineProcess(gNow);
            if (lCase == 4U) {
                assert(apneaEngineStateGet() == APNEA_ENGINE_ALARM);
                assert(physAlarmApneaDetect(gNow));
                assert(phaseControllerStateGet() == PHASE_EXP);
                assert(phaseControllerExpirationCaptureNotify() == PHASE_CONTROL_SUCCESS);
                apneaEngineProcess(gNow);
            }
            assert(apneaEngineStateGet() == APNEA_ENGINE_BACKUP);
            assert(physAlarmApneaDetect(gNow));
            assert(phaseControllerStateGet() == PHASE_INSP);
            lBackupStart = gNow;
            assert(phaseControllerActivePlanGet(&lPlan) == PHASE_CONTROL_SUCCESS);
            near(lPlan.inspiratoryPressureCmh2o, 25.0F);
            near(lPlan.pressureLimitCmh2o, GetVentLimitSettings()->pressureHigh);
            assert(lPlan.riseTimeMs == 200U && lPlan.holdTimeMs == 1100U);
            assert(lPlan.maximumInspiratoryTimeMs == 1300U);
            assert(lPlan.backupBreathIntervalMs == 4000U);
            triggerEngineProcess(gNow);
            gNow += 1300U;
            phaseControllerProcess(gNow);
            apneaEngineProcess(gNow);
            assert(phaseControllerStateGet() == PHASE_EXP);
            assert(phaseControllerExpirationCaptureNotify() == PHASE_CONTROL_SUCCESS);
            gNow = lBackupStart + 3999U;
            apneaEngineProcess(gNow);
            assert(phaseControllerStateGet() == PHASE_EXP);
            gNow++;
            apneaEngineProcess(gNow);
            assert(phaseControllerStateGet() == PHASE_INSP);
            assert(physAlarmApneaDetect(gNow));
            triggerEngineProcess(gNow);
            gNow += 1300U;
            phaseControllerProcess(gNow);
            apneaEngineProcess(gNow);
            assert(phaseControllerExpirationCaptureNotify() == PHASE_CONTROL_SUCCESS);
            for (unsigned int lIndex = 0; lIndex < 50; lIndex++, gNow += 6U) {
                triggerEngineProcess(gNow);
            }
        }
        gData[PAT_REAL_PRS] = 2.5F;
        gData[PAT_REAL_FLOW] = 4.0F;
        for (unsigned int lIndex = 0; lIndex < 5; lIndex++, gNow += 6U) {
            triggerEngineProcess(gNow);
            if (phaseControllerStateGet() == PHASE_INSP) { break; }
        }
        assert(phaseControllerStateGet() == PHASE_INSP);
        assert(phaseControllerActivePlanGet(&lPlan) == PHASE_CONTROL_SUCCESS);
        assert(lPlan.triggerReason == (lCase >= 3U ? BREATH_TRIGGER_REASON_FLOW : BREATH_TRIGGER_REASON_PRESSURE));
        apneaEngineProcess(gNow);
        assert(!physAlarmApneaDetect(gNow));
        assert(lPlan.timeTriggerEnabled == 0U && lPlan.cycleType == BREATH_CYCLE_TYPE_FLOW);
        near(lPlan.inspiratoryPressureCmh2o, 15.0F);
        gData[PAT_REAL_FLOW] = 40.0F;
        cycleEngineProcess(gNow);
        if (lCase == 0U) {
            gData[PAT_REAL_FLOW] = 9.0F;
            cycleEngineProcess(gNow + 100U);
            assert(phaseControllerStateGet() == PHASE_INSP);
            for (unsigned int lIndex = 0; lIndex < 10; lIndex++) {
                cycleEngineProcess(gNow + 300U + lIndex * 6U);
            }
        } else {
            cycleEngineProcess(gNow + lPlan.maximumInspiratoryTimeMs);
        }
        assert(phaseControllerStateGet() == PHASE_EXP);
    }
    assert(breathSchedulerStop() == BREATH_CONTROL_SUCCESS);
    apneaEngineProcess(gNow);
    assert(!physAlarmApneaDetect(gNow));
    GetVentLimitSettings()->apneaTimeAlarm = 60U;
    GetVentCpapPsvSettings()->apneaRateBpm = 0.0F;
    assert(breathSchedulerSettingsUpdate(VENT_MD_CPAP_PSV) == BREATH_CONTROL_ERROR_SETTINGS);
    GetVentCpapPsvSettings()->apneaRateBpm = 15.0F;
    GetVentCpapPsvSettings()->apneaInspTimeMs = 4000U;
    assert(breathSchedulerSettingsUpdate(VENT_MD_CPAP_PSV) == BREATH_CONTROL_ERROR_SETTINGS);
    GetVentCpapPsvSettings()->apneaInspTimeMs = 1300U;
    GetVentCpapPsvSettings()->cycleOffPercent = NAN;
    assert(breathSchedulerSettingsUpdate(VENT_MD_CPAP_PSV) == BREATH_CONTROL_ERROR_SETTINGS);
    GetVentCpapPsvSettings()->cycleOffPercent = 25.0F;
    reset();
    assert(breathSchedulerStart(VENT_MD_PSV_ST) == BREATH_CONTROL_SUCCESS);
    assert(breathSchedulerNextPlanGet(BREATH_TRIGGER_REASON_APNEA_BACKUP, &lPlan) == BREATH_CONTROL_SUCCESS);
    near(lPlan.inspiratoryPressureCmh2o, 15.0F);
    assert(lPlan.maximumInspiratoryTimeMs == 1300U && lPlan.backupBreathIntervalMs == 4000U);
    assert(lPlan.riseTimeMs + lPlan.holdTimeMs == 1300U);
    GetVentPsvStSettings()->inspRateBpm = 0.0F;
    assert(breathSchedulerSettingsUpdate(VENT_MD_PSV_ST) == BREATH_CONTROL_ERROR_SETTINGS);
    GetVentPsvStSettings()->inspRateBpm = 15.0F;
}

/** Exercise ST startup, timed cycles, patient recovery and wrap-safe deadlines. */
static void testPsvSt(void) {
    stBreathPlan lPlan;
    stVentPsvStSettings lSaved = *GetVentPsvStSettings();
    for (unsigned int lCase = 0U; lCase < 3U; lCase++) {
        reset();
        triggerEngineInit();
        cycleEngineInit();
        apneaEngineInit();
        *GetVentPsvStSettings() = lSaved;
        GetVentPsvStSettings()->triggerType = lCase == 0U ? VENT_TRIGGER_PRESSURE : VENT_TRIGGER_FLOW;
        /* Normal ST timing must work with the separate apnea timeout disabled. */
        GetVentLimitSettings()->apneaTimeAlarm = 0U;
        assert(breathSchedulerStart(VENT_MD_PSV_ST) == BREATH_CONTROL_SUCCESS);
        gData[INSP_REAL_FLOW] = 0.0F;
        gData[PAT_REAL_FLOW] = 0.0F;
        gData[PAT_REAL_PRS] = 0.0F;
        gNow = lCase == 2U ? UINT32_MAX - 2000U : 0U;
        for (unsigned int lIndex = 0U; lIndex < 400U; lIndex++, gNow += 6U) {
            phaseControllerProcess(gNow);
            apneaEngineProcess(gNow);
            if (phaseControllerStateGet() == PHASE_EXP) { break; }
        }
        assert(phaseControllerStateGet() == PHASE_EXP);
        uint32_t lReference = gNow;
        /* No expiration capture: a machine breath still starts on its deadline. */
        for (unsigned int lBreath = 0U; lBreath < 2U; lBreath++) {
            gNow = lReference + 3999U;
            apneaEngineProcess(gNow);
            assert(phaseControllerStateGet() == PHASE_EXP);
            apneaEngineProcess(++gNow);
            assert(phaseControllerStateGet() == PHASE_INSP);
            assert(apneaEngineStateGet() == APNEA_ENGINE_TIMED);
            assert(!physAlarmApneaDetect(gNow));
            assert(phaseControllerActivePlanGet(&lPlan) == PHASE_CONTROL_SUCCESS);
            assert(lPlan.breathType == BREATH_TYPE_MANDATORY_PRESSURE);
            assert(lPlan.cycleType == BREATH_CYCLE_TYPE_TIME);
            assert(lPlan.maximumInspiratoryTimeMs == 1300U);
            assert(lPlan.riseTimeMs == 200U && lPlan.holdTimeMs == 1100U);
            near(lPlan.inspiratoryPressureCmh2o, 15.0F);
            lReference = gNow;
            gData[PAT_REAL_FLOW] = 0.0F;
            cycleEngineProcess(gNow + 600U);
            assert(phaseControllerStateGet() == PHASE_INSP);
            phaseControllerProcess(gNow + 1299U);
            assert(phaseControllerStateGet() == PHASE_INSP);
            gNow += 1300U;
            phaseControllerProcess(gNow);
            apneaEngineProcess(gNow);
            assert(phaseControllerStateGet() == PHASE_EXP);
        }
        /* A real patient trigger wins over a timed deadline on the same tick. */
        assert(phaseControllerExpirationCaptureNotify() == PHASE_CONTROL_SUCCESS);
        gData[PAT_REAL_PRS] = 5.0F;
        gData[PAT_REAL_FLOW] = 0.0F;
        for (unsigned int lIndex = 0U; lIndex < 50U; lIndex++, gNow += 6U) {
            triggerEngineProcess(gNow);
        }
        gNow = lReference + 3988U;
        gData[PAT_REAL_PRS] = 2.5F;
        gData[PAT_REAL_FLOW] = 4.0F;
        for (unsigned int lIndex = 0U; lIndex < 3U; lIndex++, gNow += 6U) {
            triggerEngineProcess(gNow);
            apneaEngineProcess(gNow);
        }
        gNow -= 6U;
        assert(phaseControllerStateGet() == PHASE_INSP);
        assert(phaseControllerActivePlanGet(&lPlan) == PHASE_CONTROL_SUCCESS);
        assert(lPlan.breathType == BREATH_TYPE_SPONTANEOUS_PRESSURE_SUPPORT);
        assert(lPlan.triggerReason == (lCase == 0U ? BREATH_TRIGGER_REASON_PRESSURE : BREATH_TRIGGER_REASON_FLOW));
        assert(lPlan.maximumInspiratoryTimeMs == 2000U);
        assert(apneaEngineStateGet() == APNEA_ENGINE_MONITORING);
        lReference = gNow;
        gData[PAT_REAL_FLOW] = 40.0F;
        cycleEngineProcess(gNow);
        gData[PAT_REAL_FLOW] = 9.0F;
        cycleEngineProcess(gNow + 100U);
        assert(phaseControllerStateGet() == PHASE_INSP);
        if (lCase == 2U) {
            gNow += 2000U;
            cycleEngineProcess(gNow);
            assert(phaseControllerCycleReasonGet() == BREATH_CYCLE_REASON_MAX_INSPIRATORY_TIME);
        } else {
            gNow += 300U;
            for (unsigned int lIndex = 0U; lIndex < 3U; lIndex++, gNow += 6U) {
                cycleEngineProcess(gNow);
            }
            assert(phaseControllerCycleReasonGet() == BREATH_CYCLE_REASON_FLOW);
        }
        apneaEngineProcess(gNow);
        assert(phaseControllerStateGet() == PHASE_EXP);
        apneaEngineProcess(lReference + 3999U);
        assert(phaseControllerStateGet() == PHASE_EXP);
        apneaEngineProcess(lReference + 4000U);
        assert(phaseControllerStateGet() == PHASE_INSP);
        assert(!physAlarmApneaDetect(gNow));
        assert(breathSchedulerStop() == BREATH_CONTROL_SUCCESS);
        apneaEngineProcess(gNow);
        assert(apneaEngineStateGet() == APNEA_ENGINE_IDLE);
    }
    /* Reject nonfinite timing and combinations that consume minimum expiration. */
    for (unsigned int lCase = 0U; lCase < 6U; lCase++) {
        *GetVentPsvStSettings() = lSaved;
        if (lCase == 0U) { GetVentPsvStSettings()->inspRateBpm = NAN; }
        if (lCase == 1U) { GetVentPsvStSettings()->inspTimeMs = 4000U; }
        if (lCase == 2U) { GetVentPsvStSettings()->maxInspiratoryTimeMs = 4000U; }
        if (lCase == 3U) { GetVentPsvStSettings()->maxInspiratoryTimeMs = 0U; }
        if (lCase == 4U) { GetVentPsvStSettings()->riseTimeMs = 1500U; }
        if (lCase == 5U) { GetVentPsvStSettings()->cycleOffPercent = NAN; }
        assert(breathSchedulerSettingsUpdate(VENT_MD_PSV_ST) == BREATH_CONTROL_ERROR_SETTINGS);
    }
    *GetVentPsvStSettings() = lSaved;
    GetVentLimitSettings()->apneaTimeAlarm = 60U;
}

/** Run repeated PSV breaths with real expiration readiness, without forced capture. */
static void testPsvRepeat(void) {
    stBreathPlan lPlan;
    stActuatorRequest lRequest = {0};
    for (unsigned int lType = VENT_TRIGGER_PRESSURE; lType <= VENT_TRIGGER_FLOW; lType++) {
        reset();
        triggerEngineInit();
        cycleEngineInit();
        expirationControllerInit();
        GetVentCpapPsvSettings()->triggerType = (eVentTriggerType)lType;
        assert(breathSchedulerStart(VENT_MD_CPAP_PSV) == BREATH_CONTROL_SUCCESS);
        gData[INSP_REAL_FLOW] = 0.0F;
        gData[PAT_REAL_FLOW] = 0.0F;
        gData[PAT_REAL_PRS] = 0.0F;
        for (gNow = 0U; gNow < 2000U; gNow += 6U) {
            phaseControllerProcess(gNow);
            if (phaseControllerStateGet() == PHASE_EXP) { break; }
        }
        for (unsigned int lBreath = 0U; lBreath < 3U; lBreath++) {
            gData[PAT_REAL_PRS] = 5.0F;
            gData[INSP_REAL_PRS] = 5.0F;
            gData[PAT_REAL_FLOW] = 0.0F;
            for (unsigned int lIndex = 0; lIndex < 200U; lIndex++, gNow += 6U) {
                phaseControllerProcess(gNow);
                triggerEngineProcess(gNow);
                assert(phaseControllerActivePlanGet(&lPlan) == PHASE_CONTROL_SUCCESS);
                assert(expirationControllerProcess(&lPlan, &lRequest, &lRequest) == ACTUATOR_REQUEST_SUCCESS);
            }
            assert(phaseControllerExpirationReadyGet() != 0U);
            gData[PAT_REAL_PRS] = 2.5F;
            gData[PAT_REAL_FLOW] = 4.0F;
            for (unsigned int lIndex = 0; lIndex < 5U; lIndex++, gNow += 6U) {
                triggerEngineProcess(gNow);
                if (phaseControllerStateGet() == PHASE_INSP) { break; }
            }
            assert(phaseControllerStateGet() == PHASE_INSP);
            triggerEngineProcess(gNow);
            gData[PAT_REAL_FLOW] = 40.0F;
            cycleEngineProcess(gNow);
            gNow += 300U;
            gData[PAT_REAL_FLOW] = 9.0F;
            for (unsigned int lIndex = 0; lIndex < 5U; lIndex++, gNow += 6U) {
                cycleEngineProcess(gNow);
            }
            assert(phaseControllerStateGet() == PHASE_EXP);
            assert(phaseControllerExpirationReadyGet() == 0U);
        }
    }
}

/** Exercise SIMV windows, mandatory deadlines and tick wrap with real modules. */
static void testSimv(void) {
    stBreathPlan lPlan;
    for (unsigned int lMode = VENT_MD_P_SIMV; lMode <= VENT_MD_V_SIMV; lMode++) {
        for (unsigned int lPatient = 0U; lPatient < VENT_PATIENT_TYPE_COUNT; lPatient++) {
            uint32_t lStart = UINT32_MAX - 500U;
            uint32_t lWindow = lPatient == VENT_PATIENT_ADULT ? 5000U : 1500U;
            uint32_t lFirst = lStart + 1006U + 11000U;
            uint32_t lTrigger;
            reset();
            GetVentPatientSettings()->Type = (eVentPatientType)lPatient;
            GetVentPSimvSettings()->SIMVRateBpm = 5.0F;
            GetVentVSimvSettings()->SIMVRateBpm = 5.0F;
            GetVentPSimvSettings()->triggerType = VENT_TRIGGER_PRESSURE;
            GetVentVSimvSettings()->triggerType = VENT_TRIGGER_PRESSURE;
            GetVentPSimvSettings()->apneaSwitch = VENT_APNEA_OFF;
            GetVentVSimvSettings()->apneaSwitch = VENT_APNEA_OFF;
            assert(breathSchedulerStart((eVentMode)lMode) == BREATH_CONTROL_SUCCESS);
            phaseControllerInit();
            phaseControllerProcess(lStart);
            phaseControllerProcess(lStart + 6U);
            phaseControllerProcess(lStart + 1006U);
            assert(phaseControllerStateGet() == PHASE_EXP);
            phaseControllerProcess(lFirst - 1U);
            assert(phaseControllerStateGet() == PHASE_EXP);
            phaseControllerProcess(lFirst);
            assert(phaseControllerStateGet() == PHASE_INSP);
            assert(phaseControllerActivePlanGet(&lPlan) == PHASE_CONTROL_SUCCESS);
            assert(lPlan.syncWindowMs == lWindow);
            assert(lPlan.breathType == (lMode == VENT_MD_P_SIMV ? BREATH_TYPE_MANDATORY_PRESSURE : BREATH_TYPE_MANDATORY_VOLUME));
            if (lMode == VENT_MD_V_SIMV) { near(lPlan.targetTidalVolumeMl, 500.0F); }
            else { near(lPlan.inspiratoryPressureCmh2o, 25.0F); }
            phaseControllerProcess(lFirst + 1000U);
            assert(phaseControllerExpirationCaptureNotify() == PHASE_CONTROL_SUCCESS);
            assert(phaseControllerTrigger(BREATH_TRIGGER_REASON_PRESSURE, lFirst + 1100U) == PHASE_CONTROL_ERROR_STATE);
            lTrigger = lFirst + 12000U - lWindow - 1U;
            assert(phaseControllerTrigger(BREATH_TRIGGER_REASON_PRESSURE, lTrigger) == PHASE_CONTROL_SUCCESS);
            assert(phaseControllerActivePlanGet(&lPlan) == PHASE_CONTROL_SUCCESS);
            assert(lPlan.breathType == BREATH_TYPE_SPONTANEOUS_PRESSURE_SUPPORT);
            near(lPlan.inspiratoryPressureCmh2o, 15.0F);
            cycleEngineInit();
            cycleEngineProcess(lTrigger);
            cycleEngineProcess(lTrigger + lPlan.maximumInspiratoryTimeMs);
            assert(phaseControllerStateGet() == PHASE_EXP);
            phaseControllerProcess(lFirst + 12000U - 1U);
            assert(phaseControllerStateGet() == PHASE_EXP);
            phaseControllerProcess(lFirst + 12000U);
            assert(phaseControllerStateGet() == PHASE_INSP);
            lFirst += 12000U;
            phaseControllerProcess(lFirst + 1000U);
            assert(phaseControllerExpirationCaptureNotify() == PHASE_CONTROL_SUCCESS);
            lTrigger = lFirst + 12000U - lWindow;
            assert(phaseControllerTrigger(BREATH_TRIGGER_REASON_PRESSURE, lTrigger) == PHASE_CONTROL_SUCCESS);
            assert(phaseControllerActivePlanGet(&lPlan) == PHASE_CONTROL_SUCCESS);
            assert(lPlan.breathType != BREATH_TYPE_SPONTANEOUS_PRESSURE_SUPPORT);
            assert(lPlan.triggerReason == BREATH_TRIGGER_REASON_PRESSURE);
            phaseControllerProcess(lTrigger + 1000U);
            phaseControllerProcess(lFirst + 12000U);
            assert(phaseControllerStateGet() == PHASE_EXP);
            phaseControllerProcess(lTrigger + 12000U);
            assert(phaseControllerStateGet() == PHASE_INSP);
            assert(breathSchedulerStop() == BREATH_CONTROL_SUCCESS);
            phaseControllerProcess(lTrigger + 12006U);
            assert(phaseControllerStateGet() == PHASE_IDLE);
        }
    }
    GetVentPatientSettings()->Type = VENT_PATIENT_ADULT;
    GetVentPSimvSettings()->SIMVRateBpm = 20.0F;
    assert(breathSchedulerStart(VENT_MD_P_SIMV) == BREATH_CONTROL_SUCCESS);
    lPlan = next();
    assert(lPlan.syncWindowMs == 2000U);
    GetVentPSimvSettings()->SIMVRateBpm = NAN;
    assert(breathSchedulerSettingsUpdate(VENT_MD_P_SIMV) == BREATH_CONTROL_ERROR_SETTINGS);
    GetVentPSimvSettings()->SIMVRateBpm = 10.0F;
    GetVentVSimvSettings()->tidalVolumeMl = 0.0F;
    assert(breathSchedulerSettingsUpdate(VENT_MD_V_SIMV) == BREATH_CONTROL_ERROR_SETTINGS);
    GetVentVSimvSettings()->tidalVolumeMl = 500.0F;
    GetVentVSimvSettings()->SIMVRateBpm = 10.0F;
    GetVentVSimvSettings()->inspPausePct = 25.0F;
    assert(breathSchedulerStart(VENT_MD_V_SIMV) == BREATH_CONTROL_SUCCESS);
    lPlan = next();
    assert(lPlan.holdTimeMs == 250U);
    breathSchedulerVolumeFeedback(&lPlan, 400.0F, 1U);
    lPlan = next();
    assert(lPlan.volumeCorrectionMl > 0.0F);
    assert(breathSchedulerSupportPlanGet(BREATH_TRIGGER_REASON_FLOW, &lPlan) == BREATH_CONTROL_SUCCESS);
    breathSchedulerVolumeFeedback(&lPlan, 1000.0F, 1U);
    lPlan = next();
    near(lPlan.filteredVtiMl, 400.0F); /* Support volume must not train mandatory delivery. */
    GetVentVSimvSettings()->tidalVolumeMl = 600.0F;
    breathSchedulerProcess();
    lPlan = next();
    near(lPlan.targetTidalVolumeMl, 600.0F);
    near(lPlan.volumeCorrectionMl, 0.0F);
    GetVentVSimvSettings()->tidalVolumeMl = 500.0F;
    GetVentVSimvSettings()->inspPausePct = 0.0F;
}

/** Validate only the selected backup target and reject unknown enum values. */
static void testSimvBackupSelection(void) {
    stBreathPlan lPlan;
    reset();
    GetVentPSimvSettings()->apneaSwitch = VENT_APNEA_VOLUME;
    GetVentPSimvSettings()->apneaPressureCmh2o = NAN;
    GetVentPSimvSettings()->apneaVolumeTidalMl = 420.0F;
    assert(breathSchedulerStart(VENT_MD_P_SIMV) == BREATH_CONTROL_SUCCESS);
    assert(breathSchedulerNextPlanGet(BREATH_TRIGGER_REASON_APNEA_BACKUP, &lPlan) == BREATH_CONTROL_SUCCESS);
    assert(lPlan.breathType == BREATH_TYPE_MANDATORY_VOLUME);
    near(lPlan.targetTidalVolumeMl, 420.0F);
    GetVentPSimvSettings()->apneaSwitch = VENT_APNEA_PRESSURE;
    assert(breathSchedulerSettingsUpdate(VENT_MD_P_SIMV) == BREATH_CONTROL_ERROR_SETTINGS);
    GetVentPSimvSettings()->apneaPressureCmh2o = 20.0F;
    GetVentPSimvSettings()->apneaVolumeTidalMl = 500.0F;
    GetVentPSimvSettings()->apneaSwitch = VENT_APNEA_OFF;
    GetVentVSimvSettings()->apneaSwitch = VENT_APNEA_PRESSURE;
    GetVentVSimvSettings()->apneaPressureCmh2o = 17.0F;
    GetVentVSimvSettings()->apneaVolumeTidalMl = NAN;
    assert(breathSchedulerStart(VENT_MD_V_SIMV) == BREATH_CONTROL_SUCCESS);
    assert(breathSchedulerNextPlanGet(BREATH_TRIGGER_REASON_APNEA_BACKUP, &lPlan) == BREATH_CONTROL_SUCCESS);
    assert(lPlan.breathType == BREATH_TYPE_MANDATORY_PRESSURE);
    near(lPlan.inspiratoryPressureCmh2o, 22.0F);
    GetVentVSimvSettings()->apneaSwitch = VENT_APNEA_VOLUME;
    assert(breathSchedulerSettingsUpdate(VENT_MD_V_SIMV) == BREATH_CONTROL_ERROR_SETTINGS);
    GetVentVSimvSettings()->apneaSwitch = (eVentApneaType)-1;
    assert(breathSchedulerSettingsUpdate(VENT_MD_V_SIMV) == BREATH_CONTROL_ERROR_SETTINGS);
    GetVentVSimvSettings()->apneaSwitch = VENT_APNEA_COUNT;
    assert(breathSchedulerSettingsUpdate(VENT_MD_V_SIMV) == BREATH_CONTROL_ERROR_SETTINGS);
    GetVentVSimvSettings()->apneaSwitch = VENT_APNEA_OFF;
    assert(breathSchedulerSettingsUpdate(VENT_MD_V_SIMV) == BREATH_CONTROL_SUCCESS);
    GetVentVSimvSettings()->apneaPressureCmh2o = 20.0F;
    GetVentVSimvSettings()->apneaVolumeTidalMl = 500.0F;
}

/** Timed SIMV breaths must not mask apnea; patient effort exits backup. */
static void testSimvBackup(void) {
    stBreathPlan lPlan;
    for (unsigned int lBackup = VENT_APNEA_PRESSURE; lBackup <= VENT_APNEA_VOLUME; lBackup++) {
    for (unsigned int lMode = VENT_MD_P_SIMV; lMode <= VENT_MD_V_SIMV; lMode++) {
        reset();
        GetVentLimitSettings()->apneaTimeAlarm = 7U;
        GetVentPSimvSettings()->apneaSwitch = (eVentApneaType)lBackup;
        GetVentVSimvSettings()->apneaSwitch = (eVentApneaType)lBackup;
        assert(breathSchedulerStart((eVentMode)lMode) == BREATH_CONTROL_SUCCESS);
        phaseControllerInit();
        apneaEngineInit();
        /* Normal timed inspiration at 6 s, then apnea backup at 8.008 s. */
        for (uint32_t lNow = 0U; lNow <= 8010U; lNow += 6U) {
            phaseControllerProcess(lNow);
            apneaEngineProcess(lNow);
        }
        assert(apneaEngineStateGet() == APNEA_ENGINE_BACKUP);
        assert(phaseControllerActivePlanGet(&lPlan) == PHASE_CONTROL_SUCCESS);
        assert(lPlan.triggerReason == BREATH_TRIGGER_REASON_APNEA_BACKUP);
        assert(lPlan.breathType == (lBackup == VENT_APNEA_PRESSURE ? BREATH_TYPE_MANDATORY_PRESSURE : BREATH_TYPE_MANDATORY_VOLUME));
        for (uint32_t lNow = 8016U; lNow <= 12210U; lNow += 6U) {
            phaseControllerProcess(lNow);
            apneaEngineProcess(lNow);
        }
        assert(phaseControllerStateGet() == PHASE_INSP);
        assert(phaseControllerActivePlanGet(&lPlan) == PHASE_CONTROL_SUCCESS);
        assert(lPlan.triggerReason == BREATH_TRIGGER_REASON_APNEA_BACKUP);
        phaseControllerProcess(13218U);
        apneaEngineProcess(13218U);
        assert(phaseControllerExpirationCaptureNotify() == PHASE_CONTROL_SUCCESS);
        assert(phaseControllerTrigger(BREATH_TRIGGER_REASON_PRESSURE, 13416U) == PHASE_CONTROL_SUCCESS);
        apneaEngineProcess(13416U);
        assert(apneaEngineStateGet() == APNEA_ENGINE_MONITORING);
        assert(phaseControllerActivePlanGet(&lPlan) == PHASE_CONTROL_SUCCESS);
        assert(lPlan.triggerReason == BREATH_TRIGGER_REASON_PRESSURE);
        GetVentPSimvSettings()->apneaSwitch = VENT_APNEA_OFF;
        GetVentVSimvSettings()->apneaSwitch = VENT_APNEA_OFF;
        breathSchedulerProcess();
        assert(breathSchedulerNextPlanGet(BREATH_TRIGGER_REASON_APNEA_BACKUP, &lPlan) == BREATH_CONTROL_SUCCESS);
        assert(lPlan.triggerReason == BREATH_TRIGGER_REASON_TIME && lPlan.apneaTimeMs == 0U);
    }
    }
    GetVentLimitSettings()->apneaTimeAlarm = 60U;
}

int main(void) {
    testSimv();
    testSimvBackup();
    testSimvBackupSelection();
    testPsvSt();
    testPsvRepeat();
    testPsv();
    testVacTrigger();
    (void)testTimeCompensation;
#if BREATH_VOLUME_FLOW_COMPENSATION_ENABLE
    testEma();
    testReset();
    testConvergence();
    testIntegration();
    testShortVolumeReference();
#else
    (void)testEma; (void)testReset; (void)testConvergence;
    (void)testIntegration; (void)testShortVolumeReference;
    testTimeCompensation();
    testReset();
    testIntegration();
    testShortVolumeReference();
#endif
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
    with tempfile.TemporaryDirectory(prefix="ventcore-vti-") as directory:
        harness = Path(directory) / "vti_compensation_test.c"
        harness.write_text(HARNESS, encoding="utf-8", newline="\n")
        executable = Path(directory) / "vti_compensation_test.exe"
        includes = ["user/app/physalarm", "user/app/ventlogic", "user/app/ventalgo", "user/app/databus",
                    "user/app/calibration", "user/module/rtos", "user/tools/controller",
                    "user/tools/filter/numfilter", "user/module/log", "user/tools/ringbuffer"]
        sources = ["user/app/physalarm/physalarmapnea.c", "user/app/ventlogic/breathscheduler.c", "user/app/ventlogic/phasecontroller.c",
                   "user/app/ventlogic/triggerengine.c",
                   "user/app/ventalgo/expirationcontroller.c",
                   "user/app/ventlogic/cycleengine.c", "user/app/ventlogic/apneaengine.c",
                   "user/app/ventlogic/monitorengine.c", "user/app/ventalgo/flowcontroller.c",
                   "user/app/databus/settingdata.c", "user/tools/controller/pid.c"]
        command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                   *[f"-I{ROOT / path}" for path in includes], str(harness),
                   *[str(ROOT / path) for path in sources], "-o", str(executable)]
        environment = os.environ.copy()
        environment["PATH"] = str(Path(compiler).parent) + os.pathsep + environment["PATH"]
        for legacy in (0, 1):
            subprocess.run(command + [f"-DBREATH_VOLUME_FLOW_COMPENSATION_ENABLE={legacy}"], check=True, env=environment)
            subprocess.run([str(executable)], check=True, env=environment)
            print(f"PASS: {'legacy flow' if legacy else 'time'} compensation")
    print("PASS: EMA, next-breath timing, once-only feedback, conversion, bounds, convergence, faults, reset")


if __name__ == "__main__":
    main()
