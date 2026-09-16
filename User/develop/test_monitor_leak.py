"""Exercise production leak monitoring and VAC pause control with host inputs."""
import os
import argparse
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
#include "apneaengine.h"
#include "techalarmmanager.h"
#include "techphys.h"
#include "pipeflowtable.h"

static float gData[CONTROL_DATA_COUNT];
static float gOffset;
static uint32_t gNow;
static ePhaseControllerState gPhase;
static uint8_t gPause;
static uint8_t gExpirationReady;
static stVentLimitSettings gLimits = {.pressureLow = 1.0F, .pressureHigh = 60.0F};
static stBreathPlan gPlan;
static stVentPatientSettings gPatient = {.Type = VENT_PATIENT_ADULT};

stVentLimitSettings *GetVentLimitSettings(void) { return &gLimits; }
stVentPatientSettings *GetVentPatientSettings(void) { return &gPatient; }
float controlDataGet(ControlData_Index_EnumDef index) { return gData[index]; }
float controlDataMdiffFlowZeroOffsetGet(void) { return gOffset; }
ePhaseControllerState phaseControllerStateGet(void) { return gPhase; }
uint8_t phaseControllerVolumePauseActiveGet(void) { return gPause; }
uint8_t phaseControllerExpirationReadyGet(void) { return gExpirationReady; }
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
eApneaEngineState apneaEngineStateGet(void) { return APNEA_ENGINE_IDLE; }
uint8_t breathSchedulerRunningGet(void) { return 1U; }
void breathSchedulerVolumeReset(void) {}
void breathSchedulerVolumeFeedback(const stBreathPlan *plan, float vtiMl, uint8_t valid) {
    (void)plan;
    (void)vtiMl;
    (void)valid;
}

/** Advance one nominal 6 ms measurement, independently of estimated flow. */
static void sample(ePhaseControllerState phase, float flow, float pressure) {
    gPhase = phase;
    gData[PAT_REAL_FLOW] = flow;
    gData[PAT_REAL_PRS] = pressure;
    gNow += 6U;
    monitorEngineProcess(gNow);
}

/** Restore independent test state. */
static void reset(void) {
    gNow = 0U;
    gOffset = 0.0F;
    gPause = 0U;
    gExpirationReady = 0U;
    gPatient.Type = VENT_PATIENT_ADULT;
    gLimits.tidalVolumeHigh = 6000U;
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

/** Verify sliding windows, shared results, short windows and cycle isolation. */
static void dynamicPeep(void) {
    stBreathResult lResult;
    reset();
    gPlan.mode = VENT_MD_VAC;
    sample(PHASE_EXP, 0.0F, 4.0F); /* Initial trigger wait has no completed breath. */
    assert(monitorEngineGet(MONITOR_HMI_PEEP) == 4.0F);
    assert(monitorEngineGet(MONITOR_DYN_PEEP_VALID) == 1.0F);
    sample(PHASE_INSP, 30.0F, 25.0F);
    assert(monitorEngineGet(MONITOR_HMI_PEEP) == 4.0F);
    sample(PHASE_EXP, -20.0F, 1.0F);
    for (unsigned int lIndex = 2U; lIndex <= 8U; lIndex++) {
        sample(PHASE_EXP, -20.0F, (float)lIndex);
        float lExpected = lIndex < 5U ? (1.0F + (float)lIndex) / 2.0F : (float)lIndex - 2.0F;
        assert(fabsf(monitorEngineGet(MONITOR_DYN_PEEP) - lExpected) < 0.0001F);
        assert(monitorEngineGet(MONITOR_HMI_PEEP) == monitorEngineGet(MONITOR_DYN_PEEP));
    }
    sample(PHASE_EXP, 0.0F, NAN);
    sample(PHASE_EXP, 0.0F, INFINITY);
    monitorEngineBreathComplete(gNow);
    assert(monitorEngineGet(MONITOR_DYN_PEEP) == 6.0F);
    assert(monitorEngineGet(MONITOR_HMI_PEEP) == 6.0F);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert(lResult.peepCmh2o == 6.0F);
    assert((lResult.validMask & BREATH_RESULT_VALID_PEEP) != 0U);
    sample(PHASE_EXP, 0.0F, 1.0F);
    monitorEngineBreathComplete(gNow);
    assert(monitorEngineGet(MONITOR_DYN_PEEP) == 6.0F);

    for (unsigned int lCount = 1U; lCount <= 5U; lCount++) {
        gPlan.sequence++;
        sample(PHASE_INSP, 30.0F, 25.0F);
        for (unsigned int lPoint = 0U; lPoint < lCount; lPoint++) {
            sample(PHASE_EXP, 10.0F, 10.0F + (float)lPoint);
        }
        monitorEngineBreathComplete(gNow);
        assert(monitorEngineGet(MONITOR_DYN_PEEP) == 10.0F + (float)(lCount - 1U) / 2.0F);
        assert(monitorEngineGet(MONITOR_HMI_PEEP) == monitorEngineGet(MONITOR_DYN_PEEP));
    }

    gPlan.sequence++;
    sample(PHASE_INSP, 30.0F, 25.0F);
    sample(PHASE_EXP, 0.0F, 7.0F);
    sample(PHASE_EXP, 0.0F, 9.0F);
    gPlan.sequence++;
    sample(PHASE_INSP, 30.0F, 25.0F); /* Observed boundary excludes inspiration pressure. */
    assert(monitorEngineGet(MONITOR_DYN_PEEP) == 8.0F);
    assert(monitorEngineGet(MONITOR_HMI_PEEP) == 8.0F);
    sample(PHASE_INSP, 20.0F, 30.0F);
    assert(monitorEngineGet(MONITOR_HMI_PEEP) == 8.0F);
    sample(PHASE_EXP, 0.0F, NAN);
    monitorEngineBreathComplete(gNow);
    assert(monitorEngineGet(MONITOR_DYN_PEEP) == 0.0F);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert((lResult.validMask & BREATH_RESULT_VALID_PEEP) == 0U);
    sample(PHASE_IDLE, 0.0F, 0.0F);
    assert(monitorEngineGet(MONITOR_HMI_PEEP) == 0.0F);
}

/** Flow magnitude, slope and validity do not gate pressure samples. */
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
               5.0F);
    }
}

/** PAC excludes release transients; PSV/ST refresh without another breath. */
static void peepDisplayTiming(void) {
    stBreathResult lResult;
    reset();
    gPlan.mode = VENT_MD_PAC;
    sample(PHASE_INSP, 30.0F, 30.0F);
    sample(PHASE_EXP, -20.0F, 7.0F);
    sample(PHASE_EXP, -20.0F, 6.0F);
    assert(monitorEngineGet(MONITOR_DYN_PEEP_VALID) == 1.0F);
    assert(monitorEngineGet(MONITOR_HMI_PEEP_VALID) == 0.0F);
    for (unsigned int lIndex = 0U; lIndex < 5U; lIndex++) {
        sample(PHASE_EXP, 0.0F, 4.7F);
        assert(monitorEngineGet(MONITOR_HMI_PEEP_VALID) == 0.0F);
    }
    monitorEngineBreathComplete(gNow);
    assert(fabsf(monitorEngineGet(MONITOR_HMI_PEEP) - 4.7F) < 0.0001F);
    assert(monitorEngineGet(MONITOR_HMI_PEEP_VALID) == 1.0F);
    gPlan.sequence++;
    sample(PHASE_INSP, 30.0F, 30.0F);
    sample(PHASE_EXP, -20.0F, 7.0F);
    sample(PHASE_EXP, -20.0F, 6.0F);
    assert(fabsf(monitorEngineGet(MONITOR_HMI_PEEP) - 4.7F) < 0.0001F);
    assert(monitorEngineGet(MONITOR_DYN_PEEP) == 6.5F);
    for (unsigned int lIndex = 0U; lIndex < 5U; lIndex++) {
        sample(PHASE_EXP, 0.0F, 4.8F);
    }
    gPlan.sequence++;
    sample(PHASE_INSP, 30.0F, 30.0F); /* Fallback boundary excludes rising pressure. */
    assert(fabsf(monitorEngineGet(MONITOR_HMI_PEEP) - 4.8F) < 0.0001F);
    sample(PHASE_EXP, 0.0F, NAN);
    monitorEngineBreathComplete(gNow);
    assert(monitorEngineGet(MONITOR_HMI_PEEP_VALID) == 0.0F);

    for (unsigned int lMode = 0U; lMode < 2U; lMode++) {
        reset();
        gPlan.mode = lMode == 0U ? VENT_MD_CPAP_PSV : VENT_MD_PSV_ST;
        for (unsigned int lIndex = 0U; lIndex < 10U; lIndex++) {
            sample(PHASE_EXP, 0.0F, 4.7F);
        }
        assert(monitorEngineGet(MONITOR_HMI_PEEP_VALID) == 0.0F);
        gExpirationReady = 1U;
        for (unsigned int lIndex = 0U; lIndex < MONITOR_HMI_PEEP_STABLE_SAMPLE_COUNT; lIndex++) {
            sample(PHASE_EXP, 0.0F, 7.0F - 0.4F * (float)lIndex);
            assert(monitorEngineGet(MONITOR_HMI_PEEP_VALID) == 0.0F);
        }
        for (unsigned int lIndex = 0U; lIndex < MONITOR_HMI_PEEP_STABLE_SAMPLE_COUNT; lIndex++) {
            sample(PHASE_EXP, 0.0F, 4.7F);
        }
        assert(fabsf(monitorEngineGet(MONITOR_HMI_PEEP) - 4.7F) < 0.0001F);
        assert(monitorEngineGet(MONITOR_HMI_PEEP_VALID) == 1.0F);
        assert(monitorEngineBreathResultGet(&lResult) != MONITOR_ENGINE_SUCCESS);
        for (unsigned int lIndex = 0U; lIndex < 1000U; lIndex++) {
            sample(PHASE_EXP, 0.0F, 5.2F);
        }
        assert(fabsf(monitorEngineGet(MONITOR_HMI_PEEP) - 5.2F) < 0.0001F);
        /* A real stable high PEEP must remain visible, without setpoint clamping. */
        for (unsigned int lIndex = 0U; lIndex < MONITOR_HMI_PEEP_STABLE_SAMPLE_COUNT; lIndex++) {
            sample(PHASE_EXP, 0.0F, 8.0F);
        }
        assert(monitorEngineGet(MONITOR_HMI_PEEP) == 8.0F);
        sample(PHASE_INSP, 30.0F, 30.0F);
        gExpirationReady = 0U;
        sample(PHASE_EXP, -20.0F, 20.0F);
        assert(monitorEngineGet(MONITOR_HMI_PEEP) == 8.0F);
        gExpirationReady = 1U;
        for (unsigned int lIndex = 0U; lIndex < MONITOR_HMI_PEEP_STABLE_SAMPLE_COUNT - 1U; lIndex++) {
            sample(PHASE_EXP, 0.0F, 4.7F);
            assert(monitorEngineGet(MONITOR_HMI_PEEP) == 8.0F);
        }
        sample(PHASE_EXP, 0.0F, NAN);
        for (unsigned int lIndex = 0U; lIndex < MONITOR_HMI_PEEP_STABLE_SAMPLE_COUNT - 1U; lIndex++) {
            sample(PHASE_EXP, 0.0F, 4.7F);
            assert(monitorEngineGet(MONITOR_HMI_PEEP) == 8.0F);
        }
        sample(PHASE_EXP, 0.0F, 4.7F);
        assert(fabsf(monitorEngineGet(MONITOR_HMI_PEEP) - 4.7F) < 0.0001F);
        /* A 30 ms pressure plateau during manual effort must not become PEEP. */
        for (unsigned int lIndex = 0U; lIndex < 5U; lIndex++) {
            sample(PHASE_EXP, 0.0F, 7.5F);
            assert(fabsf(monitorEngineGet(MONITOR_HMI_PEEP) - 4.7F) < 0.0001F);
        }
        sample(PHASE_EXP, 0.0F, 2.0F); /* Trigger effort must not replace stable display. */
        monitorEngineBreathComplete(gNow);
        gPlan.sequence++;
        sample(PHASE_INSP, 30.0F, 30.0F);
        assert(fabsf(monitorEngineGet(MONITOR_HMI_PEEP) - 4.7F) < 0.0001F);
        sample(PHASE_IDLE, 0.0F, 0.0F);
        assert(monitorEngineGet(MONITOR_HMI_PEEP_VALID) == 0.0F);
    }
}

/** Publish a cycle's pressure mean and enter the next inspiration. */
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
        eTechAlarmType lType = lCase == 0U ? TECH_ALARM_PEEP_HIGH : TECH_ALARM_PEEP_LOW;
        float lBad = lCase == 0U ? 11.0F : 1.0F;
        float lEqual = lCase == 0U ? 10.0F : 2.0F;
        uint32_t lStart;
        reset();
        techAlarmManagerInit();
        sample(PHASE_INSP, 30.0F, 25.0F);
        techAlarmManagerProcess(gNow);
        assert(!techAlarmManagerStateGet(lType)); /* No previous cycle. */
        sample(PHASE_EXP, 0.0F, lBad);
        monitorEngineBreathComplete(gNow);
        techAlarmManagerProcess(gNow);
        assert(!techAlarmManagerStateGet(lType)); /* Expiration cannot trigger. */
        gPlan.sequence++;
        sample(PHASE_INSP, 30.0F, 25.0F);
        techAlarmManagerProcess(gNow);
        assert(techAlarmManagerStateGet(lType));
        stMcmTechAlarmStatusSnapshot lStatus;
        techAlarmManagerSnapshotGet(&lStatus);
        assert(lStatus.phys.value == (lCase == 0U ? 1U : 2U));
        assert(lStatus.tech.value == 0U && lStatus.power.value == 0U);
        assert(lStatus.comm.value == 0U && lStatus.cal.value == 0U);
        assert(!techAlarmManagerStateGet(lCase == 0U ? TECH_ALARM_PEEP_LOW : TECH_ALARM_PEEP_HIGH));

        peepAlarmNextBreath(5.0F);
        lStart = gNow;
        techAlarmManagerProcess(lStart);
        techAlarmManagerProcess(lStart + 199U);
        assert(techAlarmManagerStateGet(lType));
        gNow += 199U;
        peepAlarmNextBreath(lEqual); /* Equality interrupts recovery. */
        techAlarmManagerProcess(gNow);
        gNow += 250U;
        techAlarmManagerProcess(gNow);
        assert(techAlarmManagerStateGet(lType));

        peepAlarmNextBreath(5.0F);
        lStart = gNow;
        techAlarmManagerProcess(lStart);
        techAlarmManagerProcess(lStart + 199U);
        assert(techAlarmManagerStateGet(lType));
        techAlarmManagerProcess(lStart + 200U);
        assert(!techAlarmManagerStateGet(lType));
        gNow += 200U;
        peepAlarmNextBreath(lEqual);
        techAlarmManagerProcess(gNow);
        assert(!techAlarmManagerStateGet(lType)); /* Equality cannot trigger. */
        peepAlarmNextBreath(lBad);
        techAlarmManagerProcess(gNow);
        assert(techAlarmManagerStateGet(lType));
        peepAlarmNextBreath(5.0F);
        lStart = UINT32_MAX - 100U;
        techAlarmManagerProcess(lStart);
        techAlarmManagerProcess(lStart + 199U);
        assert(techAlarmManagerStateGet(lType));
        techAlarmManagerProcess(lStart + 200U);
        assert(!techAlarmManagerStateGet(lType));
        peepAlarmNextBreath(lBad);
        techAlarmManagerProcess(gNow);
        assert(techAlarmManagerStateGet(lType));
        sample(PHASE_IDLE, 0.0F, 0.0F);
        techAlarmManagerProcess(gNow);
        assert(!techAlarmManagerStateGet(lType));
    }
}

/** Check CPAP timing boundaries, interruptions, phase changes and tick wrap. */
static void cpapCheck(uint32_t nowMs, float insp, float patient, bool active) {
    gData[INSP_REAL_PRS] = insp;
    gData[PAT_REAL_PRS] = patient;
    techAlarmManagerProcess(nowMs);
    assert(techAlarmManagerStateGet(TECH_ALARM_CPAP_TOO_HIGH) == active);
    stMcmTechAlarmStatusSnapshot lStatus;
    techAlarmManagerSnapshotGet(&lStatus);
    assert(((lStatus.phys.value & 0x10U) != 0U) == active);
    assert((lStatus.phys.value & 0x1000U) == 0U);
}

/** Exercise the enabled CPAP detector through the alarm manager. */
static void cpapAlarms(void) {
    stMcmTechAlarmStatusSnapshot lStatus;
    reset();
    techAlarmManagerInit();
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
    assert(!techAlarmManagerStateGet((eTechAlarmType)-1));
    assert(!techAlarmManagerStateGet(TECH_ALARM_COUNT));
    techAlarmManagerSnapshotGet(NULL);
    physAlarmManagerInit();
    physAlarmManagerProcess(4999U);
    assert(techAlarmManagerStateGet(TECH_ALARM_CPAP_TOO_HIGH));
    techAlarmManagerSnapshotGet(&lStatus);
    assert(lStatus.phys.value == (1UL << PHYSIO_FAULT_CPAP_TOO_HIGH));
    assert(lStatus.tech.value == 0U && lStatus.power.value == 0U);
    assert(lStatus.comm.value == 0U && lStatus.cal.value == 0U);
    techAlarmManagerInit();
    techAlarmManagerSnapshotGet(&lStatus);
    assert(lStatus.phys.value == 0U);
    for (uint32_t lType = 0U; lType < TECH_ALARM_COUNT; lType++) {
        assert(!techAlarmManagerStateGet((eTechAlarmType)lType));
    }
}

/** Verify whole-cycle averaging, publication timing and invalid-cycle handling. */
static void meanPressure(void) {
    stBreathResult lResult;

    reset();
    sample(PHASE_INSP, 0.0F, 20.0F);
    sample(PHASE_INSP, 0.0F, 20.0F);
    sample(PHASE_EXP, 0.0F, 8.0F);
    assert(monitorEngineGet(MONITOR_HMI_PRS_MEAN) == 0.0F);
    monitorEngineBreathComplete(gNow);
    assert(monitorEngineGet(MONITOR_HMI_PRS_MEAN) == 16.0F);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert(lResult.meanPressureCmh2o == 16.0F);
    assert((lResult.validMask & BREATH_RESULT_VALID_MEAN_PRESSURE) != 0U);
    monitorEngineBreathComplete(gNow);
    gPlan.sequence++;
    sample(PHASE_INSP, 0.0F, -2.0F);
    assert(monitorEngineGet(MONITOR_HMI_PRS_MEAN) == 16.0F);
    sample(PHASE_EXP, 0.0F, 0.0F);
    gPlan.sequence++;
    sample(PHASE_INSP, 0.0F, 100.0F);
    assert(monitorEngineGet(MONITOR_HMI_PRS_MEAN) == -1.0F);
    sample(PHASE_EXP, 0.0F, NAN);
    monitorEngineBreathComplete(gNow);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert((lResult.validMask & BREATH_RESULT_VALID_MEAN_PRESSURE) == 0U);
    assert(monitorEngineGet(MONITOR_HMI_PRS_MEAN) == 0.0F);
    sample(PHASE_IDLE, 0.0F, 0.0F);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_ERROR_STATE);
}

/** Verify leak averaging uses both phases and preserves completed snapshots. */
static void minuteLeak(void) {
    stBreathResult lResult;
    reset();
    sample(PHASE_INSP, 10.0F, 25.0F);
    sample(PHASE_EXP, 10.0F, 25.0F);
    monitorEngineBreathComplete(gNow);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert((lResult.validMask & BREATH_RESULT_VALID_MINUTE_LEAK) == 0U);
    assert((lResult.validMask & BREATH_RESULT_VALID_LEAK_PERCENT) == 0U);
    gPlan.sequence++;
    sample(PHASE_INSP, 10.0F, 25.0F);
    sample(PHASE_INSP, 10.0F, 25.0F);
    sample(PHASE_EXP, -4.0F, 4.0F);
    assert(monitorEngineGet(MONITOR_HMI_MV_LEAK) == 0.0F);
    monitorEngineBreathComplete(gNow);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert((lResult.validMask & BREATH_RESULT_VALID_MINUTE_LEAK) != 0U);
    assert(fabsf(lResult.minuteLeakLpm - 8.0F) < 0.001F);
    assert(fabsf(lResult.minuteTotalLpm - 2.0F) < 0.001F);
    assert((lResult.validMask & BREATH_RESULT_VALID_MVI) != 0U);
    assert((lResult.validMask & BREATH_RESULT_VALID_MVE) != 0U);
    assert(fabsf(lResult.minuteInspiratoryLpm - 10.0F) < 0.001F);
    assert(fabsf(monitorEngineGet(MONITOR_HMI_MV_INSP) - 10.0F) < 0.001F);
    assert(fabsf(lResult.leakPercent - 80.0F) < 0.001F);
    assert((lResult.validMask & BREATH_RESULT_VALID_LEAK_PERCENT) != 0U);
    assert(fabsf(monitorEngineGet(MONITOR_HMI_LEAK_PERCENT) - 80.0F) < 0.001F);
    assert(fabsf(monitorEngineGet(MONITOR_HMI_MV_LEAK) - 8.0F) < 0.001F);
    gPlan.sequence++;
    sample(PHASE_INSP, 10.0F, 25.0F);
    assert(fabsf(monitorEngineGet(MONITOR_HMI_MV_LEAK) - 8.0F) < 0.001F);
    sample(PHASE_EXP, 4.0F, NAN);
    monitorEngineBreathComplete(gNow);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert((lResult.validMask & BREATH_RESULT_VALID_MINUTE_LEAK) == 0U);
    assert((lResult.validMask & BREATH_RESULT_VALID_LEAK_PERCENT) == 0U);
    assert(monitorEngineGet(MONITOR_HMI_MV_LEAK) == 0.0F);
    sample(PHASE_IDLE, 0.0F, 0.0F);
    assert(monitorEngineGet(MONITOR_HMI_MV_LEAK) == 0.0F);
}

/** Reject zero denominators and retain valid zero-leak percentages. */
static void leakPercent(void) {
    stBreathResult lResult;
    reset();
    sample(PHASE_INSP, 10.0F, 25.0F);
    sample(PHASE_EXP, -10.0F, 25.0F);
    monitorEngineBreathComplete(gNow);
    gPlan.sequence++;
    sample(PHASE_INSP, 10.0F, 25.0F);
    sample(PHASE_EXP, -10.0F, 25.0F);
    monitorEngineBreathComplete(gNow);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert((lResult.validMask & BREATH_RESULT_VALID_LEAK_PERCENT) != 0U);
    assert(lResult.leakPercent == 0.0F);
    gPlan.sequence++;
    sample(PHASE_INSP, 0.0F, 25.0F);
    sample(PHASE_EXP, 0.0F, 25.0F);
    monitorEngineBreathComplete(gNow);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert((lResult.validMask & BREATH_RESULT_VALID_LEAK_PERCENT) == 0U);
    assert(monitorEngineGet(MONITOR_HMI_LEAK_PERCENT) == 0.0F);
}

/** Verify resistance formulas, phase peaks, invalid inputs and cycle reset. */
static void resistance(void) {
    stBreathResult lResult;
    reset();
    sample(PHASE_INSP, 10.0F, 25.0F);
    gPause = 1U;
    sample(PHASE_INSP, 0.0F, 20.0F);
    sample(PHASE_EXP, -30.0F, 10.0F);
    sample(PHASE_EXP, 100.0F, 5.0F);
    monitorEngineBreathComplete(gNow);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert((lResult.validMask & BREATH_RESULT_VALID_RES_INSP) != 0U);
    assert((lResult.validMask & BREATH_RESULT_VALID_RES_EXP) != 0U);
    assert(lResult.resistanceInspiratory == 30.0F);
    assert(lResult.resistanceExpiratory == 25.0F);
    assert(monitorEngineGet(MONITOR_HMI_RES_INSP) == 30.0F);
    assert(monitorEngineGet(MONITOR_HMI_RES_EXP) == 25.0F);
    gPlan.sequence++;
    sample(PHASE_INSP, 0.0F, 20.0F);
    assert(monitorEngineGet(MONITOR_HMI_RES_EXP) == 25.0F);
    sample(PHASE_EXP, 0.0F, 5.0F);
    monitorEngineBreathComplete(gNow);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert((lResult.validMask & (BREATH_RESULT_VALID_RES_INSP | BREATH_RESULT_VALID_RES_EXP)) == 0U);
    assert(monitorEngineGet(MONITOR_HMI_RES_INSP) == 0.0F);
    assert(monitorEngineGet(MONITOR_HMI_RES_EXP) == 0.0F);
    reset();
    sample(PHASE_INSP, 10.0F, 25.0F);
    sample(PHASE_EXP, -30.0F, 5.0F);
    monitorEngineBreathComplete(gNow);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert((lResult.validMask & BREATH_RESULT_VALID_RES_INSP) == 0U);
    assert(lResult.resistanceInspiratory == 0.0F);
    assert((lResult.validMask & BREATH_RESULT_VALID_RES_EXP) == 0U);
    reset();
    sample(PHASE_INSP, 10.0F, NAN);
    sample(PHASE_EXP, -30.0F, 5.0F);
    monitorEngineBreathComplete(gNow);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert((lResult.validMask & (BREATH_RESULT_VALID_RES_INSP | BREATH_RESULT_VALID_RES_EXP)) == 0U);
    sample(PHASE_IDLE, 0.0F, 0.0F);
    assert(monitorEngineGet(MONITOR_HMI_RES_INSP) == 0.0F);
}

/** Verify compliance formulas, invalid denominators and snapshot lifetime. */
static void compliance(void) {
    stBreathResult lResult;
    for (unsigned int lCase = 0U; lCase < 6U; lCase++) {
        reset();
        sample(PHASE_INSP, 100.0F, lCase == 4U ? NAN : 25.0F);
        if (lCase != 3U) {
            gPause = 1U;
            sample(PHASE_INSP, 0.0F, 20.0F);
        }
        sample(PHASE_EXP, -60.0F, 10.0F);
        /* Fill the end-expiration window at the pressure used by the formula. */
        for (unsigned int lPoint = 0U; lPoint < 5U; lPoint++) {
            sample(PHASE_EXP, 0.0F, lCase == 1U ? 25.0F :
                   lCase == 2U ? 30.0F : lCase == 5U ? 20.0F : 5.0F);
        }
        monitorEngineBreathComplete(gNow);
        assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
        if (lCase == 0U || lCase == 3U || lCase == 5U) {
            assert((lResult.validMask & BREATH_RESULT_VALID_C_DYNC) != 0U);
            assert(fabsf(lResult.complianceDynamic - (lCase == 5U ? 2.0F : 0.5F)) < 0.0001F);
        } else {
            assert((lResult.validMask & BREATH_RESULT_VALID_C_DYNC) == 0U);
            assert(lResult.complianceDynamic == 0.0F);
        }
        if (lCase == 0U) {
            assert((lResult.validMask & BREATH_RESULT_VALID_C_STAT) != 0U);
            assert(fabsf(lResult.complianceStatic - 0.4F) < 0.0001F);
            assert(monitorEngineGet(MONITOR_HMI_C_DYNC) == lResult.complianceDynamic);
            assert(monitorEngineGet(MONITOR_HMI_C_STAT) == lResult.complianceStatic);
            gPlan.sequence++;
            sample(PHASE_INSP, 0.0F, 5.0F);
            assert(monitorEngineGet(MONITOR_HMI_C_STAT) == lResult.complianceStatic);
            sample(PHASE_EXP, 0.0F, 5.0F);
            monitorEngineBreathComplete(gNow);
            assert(monitorEngineGet(MONITOR_HMI_C_STAT) == 0.0F);
            assert(monitorEngineGet(MONITOR_HMI_C_DYNC) == 0.0F);
        } else {
            assert((lResult.validMask & BREATH_RESULT_VALID_C_STAT) == 0U);
            assert(lResult.complianceStatic == 0.0F);
        }
        sample(PHASE_IDLE, 0.0F, 0.0F);
        assert(monitorEngineGet(MONITOR_HMI_C_STAT) == 0.0F);
        assert(monitorEngineGet(MONITOR_HMI_C_DYNC) == 0.0F);
    }
}

/** Verify expiratory peak magnitude, snapshot hold, invalid cycles and reset. */
static void peakExpiratoryFlow(void) {
    stBreathResult lResult;
    reset();
    sample(PHASE_INSP, -80.0F, 25.0F);
    sample(PHASE_INSP, 60.0F, 25.0F);
    sample(PHASE_EXP, -10.0F, 5.0F);
    sample(PHASE_EXP, -32.5F, 5.0F);
    sample(PHASE_EXP, -20.0F, 5.0F);
    sample(PHASE_EXP, 100.0F, 5.0F);
    assert(monitorEngineGet(MONITOR_HMI_PEAK_EXP_FLOW) == 0.0F);
    monitorEngineBreathComplete(gNow);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert((lResult.validMask & BREATH_RESULT_VALID_PEAK_EXP_FLOW) != 0U);
    assert(lResult.peakExpiratoryFlowLpm == 32.5F);
    assert(monitorEngineGet(MONITOR_HMI_PEAK_EXP_FLOW) == 32.5F);
    for (unsigned int lCase = 0U; lCase < 3U; lCase++) {
        gPlan.sequence++;
        sample(PHASE_INSP, 10.0F, 25.0F);
        if (lCase == 0U) { assert(monitorEngineGet(MONITOR_HMI_PEAK_EXP_FLOW) == 32.5F); }
        sample(PHASE_EXP, lCase == 0U ? 0.0F : -12.0F, 5.0F);
        if (lCase != 0U) { sample(PHASE_EXP, lCase == 1U ? NAN : INFINITY, 5.0F); }
        monitorEngineBreathComplete(gNow);
        assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
        assert(((lResult.validMask & BREATH_RESULT_VALID_PEAK_EXP_FLOW) != 0U) == (lCase == 0U));
        assert(lResult.peakExpiratoryFlowLpm == 0.0F);
        assert(monitorEngineGet(MONITOR_HMI_PEAK_EXP_FLOW) == 0.0F);
    }
    sample(PHASE_IDLE, 0.0F, 0.0F);
    assert(monitorEngineGet(MONITOR_HMI_PEAK_EXP_FLOW) == 0.0F);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_ERROR_STATE);
}

/** Verify zero flow, invalid flow, zero duration and tick wrap. */
static void minuteVolumeBoundaries(void) {
    stBreathResult lResult;
    for (unsigned int lCase = 0U; lCase < 4U; lCase++) {
        reset();
        if (lCase == 3U) { gNow = UINT32_MAX - 8U; }
        sample(PHASE_INSP, lCase == 1U ? NAN : (lCase == 3U ? 30.0F : 0.0F), 25.0F);
        sample(PHASE_EXP, lCase == 3U ? -20.0F : 0.0F, 5.0F);
        monitorEngineBreathComplete(lCase == 2U ? 6U : gNow);
        assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
        if ((lCase == 1U) || (lCase == 2U)) {
            assert((lResult.validMask & (BREATH_RESULT_VALID_MVI | BREATH_RESULT_VALID_MVE)) == 0U);
        } else {
            assert((lResult.validMask & (BREATH_RESULT_VALID_MVI | BREATH_RESULT_VALID_MVE)) ==
                   (BREATH_RESULT_VALID_MVI | BREATH_RESULT_VALID_MVE));
            assert(fabsf(lResult.minuteInspiratoryLpm - (lCase == 3U ? 30.0F : 0.0F)) < 0.001F);
            assert(fabsf(lResult.minuteTotalLpm - (lCase == 3U ? 20.0F : 0.0F)) < 0.001F);
        }
        sample(PHASE_IDLE, 0.0F, 0.0F);
        assert(monitorEngineGet(MONITOR_HMI_MV_INSP) == 0.0F);
        assert(monitorEngineGet(MONITOR_HMI_MV_TOTAL) == 0.0F);
    }
}

/** PAC publishes measured terminal pressure despite residual inspiratory flow. */
static void pacPlateau(void) {
    const eVentMode lModes[] = {VENT_MD_PAC, VENT_MD_VAC, VENT_MD_CPAP_PSV, VENT_MD_PSV_ST};
    stBreathResult lResult;

    for (unsigned int lMode = 0U; lMode < sizeof(lModes) / sizeof(lModes[0]); lMode++) {
        reset();
        gPlan.mode = lModes[lMode];
        gPlan.maximumInspiratoryTimeMs = 120U;
        for (unsigned int lBreath = 0U; lBreath < 2U; lBreath++) {
            float lPressure = 25.0F + 5.0F * (float)lBreath;
            sample(PHASE_INSP, 30.0F, 50.0F);
            sample(PHASE_INSP, 30.0F, 50.0F);
            sample(PHASE_INSP, 30.0F, 50.0F);
            sample(PHASE_INSP, 30.0F, 50.0F);
            assert(monitorEngineGet(MONITOR_PLATEAU_PRS) == 0.0F);
            sample(PHASE_INSP, 10.0F, lPressure - 1.0F);
            sample(PHASE_INSP, 8.0F, lPressure + 1.0F);
            sample(PHASE_INSP, 8.0F, NAN);
            sample(PHASE_INSP, NAN, 80.0F);
            sample(PHASE_INSP, 8.0F, INFINITY);
            sample(PHASE_EXP, 0.0F, 5.0F);
            sample(PHASE_EXP, -20.0F, 5.0F);
            monitorEngineBreathComplete(gNow);
            assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
            assert((lResult.validMask & BREATH_RESULT_VALID_PLATEAU_PRESSURE) != 0U);
            /* Other modes retain their zero-flow boundary sample. */
            assert(lResult.plateauPressureCmh2o == (lMode == 0U ? lPressure : 5.0F));
            assert(monitorEngineGet(MONITOR_HMI_PLATEAU_PRS) == lResult.plateauPressureCmh2o);
            gPlan.sequence++;
        }
    }

    reset();
    gPlan.mode = VENT_MD_PAC;
    sample(PHASE_INSP, 30.0F, 25.0F);
    sample(PHASE_EXP, 0.0F, 5.0F); /* Early cycling has no terminal window. */
    monitorEngineBreathComplete(gNow);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert((lResult.validMask & BREATH_RESULT_VALID_PLATEAU_PRESSURE) == 0U);
    assert(monitorEngineGet(MONITOR_HMI_PLATEAU_PRS) == 0.0F);
    sample(PHASE_IDLE, 0.0F, 0.0F);
    assert(monitorEngineGet(MONITOR_PLATEAU_PRS) == 0.0F);
}

/** Verify blockage signals, phase exclusion and one-cycle alarm recovery. */
static void pipelineBlockage(void) {
    stBreathResult lResult;
    reset();
    techAlarmManagerInit();
    gPlan.mode = VENT_MD_PAC;
    sample(PHASE_INSP, 0.1F, 5.0F);
    sample(PHASE_INSP, -0.3F, 10.0F);
    sample(PHASE_INSP, 0.2F, 8.0F);
    assert(!techPhysPipelineBlockageDetect(gNow));
    sample(PHASE_EXP, 20.0F, 30.0F);
    monitorEngineBreathComplete(gNow);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert(lResult.inspiratoryDeltaPeakCmh2o == 5.0F);
    assert(lResult.inspiratoryDeltaEndCmh2o == 3.0F);
    assert(fabsf(lResult.inspiratoryAbsolutePeakFlowLpm - 0.3F) < 0.0001F);
    assert(fabsf(lResult.inspiratorySignedVolumeMl) < 0.0001F);
    assert(techPhysPipelineBlockageDetect(gNow));
    techAlarmManagerProcess(gNow);
    assert(techAlarmManagerStateGet(TECH_ALARM_PIPELINE_BLOCKAGE));
    gPlan.sequence++;
    sample(PHASE_INSP, 30.0F, 5.0F);
    assert(techPhysPipelineBlockageDetect(gNow));
    sample(PHASE_INSP, 30.0F, 6.0F);
    sample(PHASE_EXP, -30.0F, 5.0F);
    monitorEngineBreathComplete(gNow);
    assert(!techPhysPipelineBlockageDetect(gNow));

    /* Strict flow-ratio boundary must not trigger the end-pressure branch. */
    reset();
    techPhysInit();
    sample(PHASE_INSP, 0.6F, 5.0F);
    sample(PHASE_INSP, 0.6F, 8.0F);
    sample(PHASE_EXP, -1.0F, 5.0F);
    monitorEngineBreathComplete(gNow);
    assert(!techPhysPipelineBlockageDetect(gNow));

    /* An invalid inspiration must not produce a partial-cycle alarm. */
    reset();
    techPhysInit();
    sample(PHASE_INSP, 0.1F, 5.0F);
    sample(PHASE_INSP, NAN, 8.0F);
    sample(PHASE_EXP, -1.0F, 5.0F);
    monitorEngineBreathComplete(gNow);
    assert(!techPhysPipelineBlockageDetect(gNow));

    /* Isolate the peak-pressure/resistance branch; end pressure stays low. */
    reset();
    techPhysInit();
    gPlan.mode = VENT_MD_PAC;
    sample(PHASE_INSP, 0.6F, 5.0F);
    sample(PHASE_INSP, 0.6F, 15.0F);
    sample(PHASE_INSP, 0.6F, 5.0F);
    gNow = 960U;
    sample(PHASE_INSP, 0.6F, 5.0F);
    sample(PHASE_EXP, -0.6F, 5.0F);
    monitorEngineBreathComplete(gNow);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert(lResult.resistanceInspiratory > 600.0F);
    assert(lResult.inspiratoryDeltaEndCmh2o == 0.0F);
    assert(techPhysPipelineBlockageDetect(gNow));
    sample(PHASE_IDLE, 0.0F, 5.0F);
    assert(!techPhysPipelineBlockageDetect(gNow));
}

/** Publish a branch measurement before the alarm task evaluates it. */
static bool branchSample(uint32_t nowMs, float flow, float inspPressure, float patientPressure) {
    gData[INSP_REAL_FLOW] = flow;
    gData[INSP_REAL_PRS] = inspPressure;
    sample(PHASE_INSP, 0.0F, patientPressure);
    return techPhysInspBranchBlockageDetect(nowMs);
}

/** Check table lookup, strict boundaries, uninterrupted timing and recovery. */
static void inspBranchBlockage(void) {
    float lFlow;
    const float lAdult[6][2] = {{1.07F,10.4F},{5.14F,39.5F},{11.39F,60.6F},
        {24.56F,90.5F},{38.98F,117.3F},{49.34F,133.8F}};
    const float lNeonatal[6][2] = {{3.42F,1.1F},{5.61F,6.4F},{10.08F,14.1F},
        {21.26F,26.2F},{51.34F,46.6F},{76.75F,56.9F}};
    for (unsigned int lIndex = 0U; lIndex < 6U; lIndex++) {
        assert(pipeFlowTableGet(VENT_PATIENT_ADULT, lAdult[lIndex][0], &lFlow) == 1);
        assert(fabsf(lFlow - lAdult[lIndex][1]) < 0.0001F);
        assert(pipeFlowTableGet(VENT_PATIENT_PEDIATRIC, lAdult[lIndex][0], &lFlow) == 1);
        assert(fabsf(lFlow - lAdult[lIndex][1]) < 0.0001F);
        assert(pipeFlowTableGet(VENT_PATIENT_NEONATAL, lNeonatal[lIndex][0], &lFlow) == 1);
        assert(fabsf(lFlow - lNeonatal[lIndex][1]) < 0.0001F);
    }
    assert(pipeFlowTableGet(VENT_PATIENT_ADULT, (1.07F + 5.14F) / 2.0F, &lFlow) == 1);
    assert(fabsf(lFlow - (10.4F + 39.5F) / 2.0F) < 0.0001F);
    assert(pipeFlowTableGet(VENT_PATIENT_NEONATAL, -1.0F, &lFlow) == 1 && lFlow == 1.1F);
    assert(pipeFlowTableGet(VENT_PATIENT_ADULT, 100.0F, &lFlow) == 1 && lFlow == 133.8F);
    assert(pipeFlowTableGet(VENT_PATIENT_TYPE_COUNT, 10.0F, &lFlow) < 0);
    assert(pipeFlowTableGet(VENT_PATIENT_ADULT, NAN, &lFlow) < 0);
    assert(pipeFlowTableGet(VENT_PATIENT_ADULT, INFINITY, &lFlow) < 0);
    assert(pipeFlowTableGet(VENT_PATIENT_ADULT, 10.0F, NULL) < 0);

    reset();
    techAlarmManagerInit();
    assert(!branchSample(0U, 15.0F, 20.0F, 10.0F)); /* deltaP == 10 is excluded. */
    assert(!branchSample(1000U, 15.0F, 20.0F, 10.0F));
    assert(!branchSample(1010U, 16.5F, 21.0F, 10.0F)); /* Q equality is included. */
    assert(!branchSample(2009U, 16.5F, 21.0F, 10.0F));
    assert(branchSample(2010U, 16.5F, 21.0F, 10.0F));
    techAlarmManagerProcess(2010U);
    assert(techAlarmManagerStateGet(TECH_ALARM_INSP_BRANCH_BLOCKAGE));
    assert(branchSample(2020U, 66.9F, 49.34F, 10.0F)); /* Recovery equality excluded. */
    assert(branchSample(3020U, 66.9F, 49.34F, 10.0F));
    assert(branchSample(3030U, 70.0F, 49.34F, 10.0F));
    assert(branchSample(4029U, 70.0F, 49.34F, 10.0F));
    assert(!branchSample(4030U, 70.0F, 49.34F, 10.0F));

    reset();
    techPhysInit();
    gPatient.Type = VENT_PATIENT_NEONATAL;
    assert(!branchSample(0U, 0.0F, 21.26F, 5.0F));
    assert(!branchSample(999U, 100.0F, 21.26F, 5.0F)); /* Interrupt confirmation. */
    assert(!branchSample(1000U, 0.0F, 21.26F, 5.0F));
    assert(!branchSample(1999U, NAN, 21.26F, 5.0F)); /* Invalid input restarts timing. */
    assert(!branchSample(2000U, 0.0F, 21.26F, 5.0F));
    assert(branchSample(3000U, 0.0F, 21.26F, 5.0F));
    assert(branchSample(3010U, 15.0F, 21.26F, 5.0F)); /* max floor is 15. */
    assert(branchSample(4010U, 15.0F, 21.26F, 5.0F));
    assert(branchSample(4020U, 16.0F, 21.26F, 5.0F));
    assert(branchSample(5019U, 0.0F, 21.26F, 5.0F)); /* Interrupt recovery. */
    assert(branchSample(5020U, 16.0F, 21.26F, 5.0F));
    assert(branchSample(6019U, 16.0F, 21.26F, 5.0F));
    assert(!branchSample(6020U, 16.0F, 21.26F, 5.0F));

    techPhysInit();
    assert(!branchSample(UINT32_MAX - 499U, 0.0F, 21.26F, 5.0F));
    assert(!branchSample(499U, 0.0F, 21.26F, 5.0F));
    assert(branchSample(500U, 0.0F, 21.26F, 5.0F));
    sample(PHASE_IDLE, 0.0F, 5.0F);
    assert(!techPhysInspBranchBlockageDetect(501U));
}

/** Keep K=2 while choosing a known whole-cycle leak peak. */
static bool leakCycle(float peakLpm) {
    stBreathResult lResult;
    float lPressure = (peakLpm * 0.5F) * (peakLpm * 0.5F);

    sample(PHASE_INSP, 2.0F, 1.0F);
    sample(PHASE_EXP, peakLpm, lPressure); /* Peak can occur during expiration. */
    sample(PHASE_EXP, 2.0F, 1.0F);
    monitorEngineBreathComplete(gNow);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert((lResult.validMask & BREATH_RESULT_VALID_MINUTE_LEAK) != 0U);
    assert(fabsf(lResult.peakLeakLpm - peakLpm) < 0.001F);
    gPlan.sequence++;
    return techPhysPipelineLeakDetect(gNow);
}

/** Check per-cycle leak counting, hysteresis, saturation and invalid-cycle hold. */
static void pipelineLeak(void) {
    reset();
    techAlarmManagerInit();
    assert(!techPhysPipelineLeakDetect(gNow));
    sample(PHASE_INSP, 10.0F, 25.0F);
    sample(PHASE_EXP, 4.0F, 4.0F);
    monitorEngineBreathComplete(gNow); /* Establish K=2; first estimate is invalid. */
    assert(!techPhysPipelineLeakDetect(gNow));
    gPlan.sequence++;
    assert(!leakCycle(6.0F));
    for (unsigned int lIndex = 0U; lIndex < 10U; lIndex++) {
        assert(!techPhysPipelineLeakDetect(gNow)); /* One count per result. */
    }
    assert(!leakCycle(5.0F)); /* Exact high boundary holds count. */
    assert(!leakCycle(3.0F)); /* Exact low boundary holds count. */
    assert(!leakCycle(4.0F));
    assert(!leakCycle(6.0F));
    assert(!leakCycle(6.0F));
    assert(!leakCycle(6.0F)); /* Four high cycles are insufficient. */
    sample(PHASE_INSP, 2.0F, 1.0F);
    assert(!techPhysPipelineLeakDetect(gNow)); /* Partial cycle does not count. */
    sample(PHASE_INSP, 6.0F, 9.0F); /* Verify an inspiratory peak as well. */
    sample(PHASE_EXP, 2.0F, 1.0F);
    monitorEngineBreathComplete(gNow);
    assert(techPhysPipelineLeakDetect(gNow));
    gPlan.sequence++;
    techAlarmManagerProcess(gNow);
    assert(techAlarmManagerStateGet(TECH_ALARM_PIPELINE_LEAK));
    assert(leakCycle(3.0F));
    assert(leakCycle(5.0F));
    assert(leakCycle(4.0F));
    for (unsigned int lIndex = 0U; lIndex < 260U; lIndex++) {
        assert(leakCycle(6.0F)); /* Count never wraps and clears an active alarm. */
    }
    assert(!leakCycle(2.0F)); /* Recover and reset the entire count. */
    for (unsigned int lIndex = 0U; lIndex < 4U; lIndex++) {
        assert(!leakCycle(6.0F));
    }
    assert(leakCycle(6.0F));
    sample(PHASE_INSP, 2.0F, 1.0F);
    sample(PHASE_EXP, 2.0F, NAN);
    monitorEngineBreathComplete(gNow);
    assert(techPhysPipelineLeakDetect(gNow)); /* Invalid zero is not recovery. */
    sample(PHASE_IDLE, 0.0F, 1.0F);
    assert(!techPhysPipelineLeakDetect(gNow));
}

/** Produce exact 900 mL patient/machine inspiration with negligible expiration. */
static bool disconnectVolumeCycle(float pressure, float endFlow) {
    stBreathResult lResult;
    gData[INSP_REAL_FLOW] = 100.0F;
    gData[INSP_REAL_PRS] = 10.0F;
    for (unsigned int lIndex = 0U; lIndex < 90U; lIndex++) {
        sample(PHASE_INSP, 100.0F, pressure);
    }
    gData[INSP_REAL_FLOW] = 0.0F;
    sample(PHASE_INSP, endFlow, pressure);
    sample(PHASE_EXP, -1.0F, pressure);
    monitorEngineBreathComplete(gNow);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert(lResult.machineInspiratoryVolumeMl == 900.0F);
    assert(lResult.patientPeakFlowLpm == 100.0F);
    assert(lResult.patientEndInspiratoryFlowLpm == endFlow);
    assert(fabsf(lResult.patientExpiratoryVolumeMl - 0.1F) < 0.0001F);
    gPlan.sequence++;
    return techPhysPipelineDisconnectDetect(gNow);
}

/** Supply a completed raw leak coefficient independent of the volume path. */
static bool disconnectLeakCycle(float coefficient) {
    stBreathResult lResult;
    gData[INSP_REAL_FLOW] = 0.0F;
    gData[INSP_REAL_PRS] = 10.0F;
    sample(PHASE_INSP, coefficient, 1.0F);
    sample(PHASE_EXP, coefficient, 1.0F);
    monitorEngineBreathComplete(gNow);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert(lResult.leakBalanceCoefficient == coefficient);
    assert(monitorEngineGet(MONITOR_LEAK_COEFFICIENT) <= 50.0F);
    gPlan.sequence++;
    return techPhysPipelineDisconnectDetect(gNow);
}

/** Verify two-stage compliance, raw leak counting, latching and shared recovery. */
static void pipelineDisconnect(void) {
    reset();
    techAlarmManagerInit();
    assert(!disconnectVolumeCycle(2.0F, 0.0F)); /* C == 450 cannot start confirmation. */
    assert(!disconnectVolumeCycle(1.0F, 0.0F));
    assert(!techPhysPipelineDisconnectDetect(gNow)); /* Same result cannot confirm twice. */
    assert(!disconnectVolumeCycle(4.5F, 0.0F)); /* C == 200 resets the first detection. */
    assert(!disconnectVolumeCycle(3.0F, 0.0F)); /* C > 200 alone cannot restart. */
    assert(!disconnectVolumeCycle(1.0F, 0.0F));
    assert(disconnectVolumeCycle(3.0F, 0.0F));
    techAlarmManagerProcess(gNow);
    assert(techAlarmManagerStateGet(TECH_ALARM_PIPELINE_DISCONNECT));
    assert(disconnectVolumeCycle(10.0F, 0.0F)); /* Nonmatching cycle does not recover. */
    gData[INSP_REAL_FLOW] = 0.0F;
    gData[INSP_REAL_PRS] = 20.0F;
    sample(PHASE_EXP, 0.0F, 5.0F);
    assert(techPhysPipelineDisconnectDetect(gNow)); /* Patient pressure equality excluded. */
    gData[INSP_REAL_PRS] = 15.0F;
    sample(PHASE_EXP, 0.0F, 6.0F);
    assert(techPhysPipelineDisconnectDetect(gNow)); /* INSP pressure equality excluded. */
    gData[INSP_REAL_PRS] = 20.0F;
    sample(PHASE_EXP, 0.0F, 6.0F);
    gData[INSP_REAL_FLOW] = 0.3F * monitorEngineGet(MONITOR_INSP_BRANCH_PIPE_FLOW);
    sample(PHASE_EXP, 0.0F, 6.0F);
    assert(techPhysPipelineDisconnectDetect(gNow)); /* Flow equality excluded. */
    gData[INSP_REAL_FLOW] = 0.0F;
    sample(PHASE_EXP, 0.0F, 6.0F);
    assert(!techPhysPipelineDisconnectDetect(gNow));
    gData[INSP_REAL_PRS] = 10.0F;
    sample(PHASE_EXP, 0.0F, 1.0F);
    assert(!techPhysPipelineDisconnectDetect(gNow)); /* Recovery consumed the old result. */

    reset();
    techPhysInit();
    assert(!disconnectLeakCycle(60.0F));
    assert(!disconnectLeakCycle(60.0F));
    assert(!disconnectLeakCycle(50.0F)); /* Equality interrupts consecutive count. */
    for (unsigned int lIndex = 0U; lIndex < 4U; lIndex++) {
        assert(!disconnectLeakCycle(60.0F));
        assert(!techPhysPipelineDisconnectDetect(gNow));
    }
    assert(disconnectLeakCycle(60.0F));
    assert(disconnectLeakCycle(2.0F)); /* Leak normalization alone does not recover. */
    gData[INSP_REAL_FLOW] = 0.0F;
    gData[INSP_REAL_PRS] = 20.0F;
    sample(PHASE_EXP, 0.0F, 6.0F);
    assert(!techPhysPipelineDisconnectDetect(gNow));
    sample(PHASE_IDLE, 0.0F, 1.0F);
    assert(!techPhysPipelineDisconnectDetect(gNow));

    reset();
    techPhysInit();
    /* (Qend/60)*R reaches the correction cap and starts detection; division would not. */
    assert(!disconnectVolumeCycle(4.0F, 100.0F));
    assert(disconnectVolumeCycle(3.0F, 0.0F));
    reset();
    techPhysInit();
    assert(!disconnectVolumeCycle(2.0F, 100.0F)); /* Nonzero end-flow starts confirmation. */
    assert(disconnectVolumeCycle(3.0F, 0.0F));
    reset();
    techPhysInit();
    assert(!disconnectVolumeCycle(0.1F, 100.0F)); /* Correction cap and epsilon floor. */
    assert(disconnectVolumeCycle(3.0F, 0.0F));
}

/** Verify completed flow-control pressure limits use the completed plan and Pmax. */
static void pressureLimitAlarm(void) {
    stBreathResult lResult;
    reset();
    techAlarmManagerInit();
    gLimits.pressureHigh = 60.0F;
    sample(PHASE_INSP, 30.0F, 55.0F);
    assert(!techPhysPressureLimitDetect(gNow));
    sample(PHASE_EXP, -30.0F, 5.0F);
    monitorEngineBreathComplete(gNow);
    assert(techPhysPressureLimitDetect(gNow));
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert(lResult.pressureLimitCmh2o == 60.0F);
    techAlarmManagerProcess(gNow);
    assert(techAlarmManagerStateGet(TECH_ALARM_PRESSURE_LIMIT));
    gLimits.pressureHigh = 80.0F;
    gPlan.breathType = BREATH_TYPE_MANDATORY_PRESSURE;
    assert(techPhysPressureLimitDetect(gNow)); /* Completed snapshot survives live changes. */
    gPlan.sequence++;
    sample(PHASE_INSP, 30.0F, 79.0F);
    assert(techPhysPressureLimitDetect(gNow)); /* Hold until the next completion. */
    sample(PHASE_EXP, -30.0F, 5.0F);
    monitorEngineBreathComplete(gNow);
    assert(!techPhysPressureLimitDetect(gNow)); /* Non-flow breath clears. */
    gPlan.breathType = BREATH_TYPE_MANDATORY_VOLUME;
    gLimits.pressureHigh = 60.0F;
    gPlan.sequence++;
    sample(PHASE_INSP, 30.0F, 54.5F);
    sample(PHASE_EXP, 30.0F, 90.0F); /* Positive expiratory flow must not extend pressure peak. */
    monitorEngineBreathComplete(gNow);
    assert(!techPhysPressureLimitDetect(gNow)); /* Exact threshold excluded. */
    gPlan.sequence++;
    sample(PHASE_INSP, NAN, 55.0F); /* Pressure detection does not require valid patient flow. */
    sample(PHASE_EXP, -30.0F, 5.0F);
    monitorEngineBreathComplete(gNow);
    assert(techPhysPressureLimitDetect(gNow));
    gPlan.sequence++;
    sample(PHASE_INSP, 30.0F, NAN);
    sample(PHASE_INSP, 30.0F, 55.0F);
    sample(PHASE_EXP, -30.0F, 5.0F);
    monitorEngineBreathComplete(gNow);
    assert(!techPhysPressureLimitDetect(gNow)); /* Reject partial pressure history. */
    sample(PHASE_IDLE, 0.0F, 5.0F);
    assert(!techPhysPressureLimitDetect(gNow));
}

/** Verify VTI, not VTE, is compared once per cycle with the saved alarm limit. */
static void volumeLimitAlarm(void) {
    stBreathResult lResult;
    reset();
    techAlarmManagerInit();
    gLimits.tidalVolumeHigh = 90U;
    gPlan.breathType = BREATH_TYPE_MANDATORY_PRESSURE;
    for (unsigned int lIndex = 0U; lIndex < 10U; lIndex++) {
        sample(PHASE_INSP, 100.0F, 10.0F);
    }
    assert(!techPhysVolumeLimitDetect(gNow));
    sample(PHASE_EXP, -100.0F, 5.0F);
    monitorEngineBreathComplete(gNow);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert(lResult.vtiMl == 100.0F && lResult.vteMl == 10.0F);
    assert(lResult.tidalVolumeLimitMl == 90U);
    gLimits.tidalVolumeHigh = 200U;
    assert(techPhysVolumeLimitDetect(gNow)); /* Uses the saved cycle limit. */
    techAlarmManagerProcess(gNow);
    assert(techAlarmManagerStateGet(TECH_ALARM_VOLUME_LIMIT));
    gPlan.sequence++;
    gLimits.tidalVolumeHigh = 100U;
    for (unsigned int lIndex = 0U; lIndex < 10U; lIndex++) {
        sample(PHASE_INSP, 100.0F, 10.0F);
        assert(techPhysVolumeLimitDetect(gNow)); /* Hold throughout the next breath. */
    }
    sample(PHASE_EXP, -100.0F, 5.0F);
    monitorEngineBreathComplete(gNow);
    assert(!techPhysVolumeLimitDetect(gNow)); /* Equality clears. */
    gPlan.sequence++;
    sample(PHASE_INSP, NAN, 10.0F);
    sample(PHASE_EXP, -100.0F, 5.0F);
    monitorEngineBreathComplete(gNow);
    assert(!techPhysVolumeLimitDetect(gNow)); /* Invalid integral is not evaluated. */
    gPlan.sequence++;
    gPlan.limitSettings = NULL;
    sample(PHASE_INSP, 1000.0F, 10.0F);
    sample(PHASE_EXP, -100.0F, 5.0F);
    monitorEngineBreathComplete(gNow);
    assert(!techPhysVolumeLimitDetect(gNow)); /* Missing limit cannot trigger. */
    sample(PHASE_IDLE, 0.0F, 5.0F);
    assert(!techPhysVolumeLimitDetect(gNow));
}

/** Complete a pressure-target cycle with an independently controlled inspiratory peak. */
static bool inspPressureCycle(float target, float pressure) {
    stBreathResult lResult;
    gPlan.inspiratoryPressureCmh2o = target;
    sample(PHASE_INSP, 30.0F, pressure);
    sample(PHASE_EXP, 30.0F, 50.0F); /* Expiratory pressure cannot satisfy the inspiration target. */
    monitorEngineBreathComplete(gNow);
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    gPlan.sequence++;
    return techPhysInspPressNotReachedDetect(gNow);
}

/** Check both strict deficits, consecutive counting, snapshots and single-cycle recovery. */
static void inspPressureNotReached(void) {
    reset();
    techAlarmManagerInit();
    assert(!inspPressureCycle(30.0F, 12.0F));
    assert(!techPhysInspPressNotReachedDetect(gNow));
    assert(!inspPressureCycle(30.0F, 12.0F));
    gPlan.inspiratoryPressureCmh2o = 5.0F;
    assert(!techPhysInspPressNotReachedDetect(gNow)); /* Held plan changes do not recount. */
    assert(inspPressureCycle(30.0F, 12.0F));
    techAlarmManagerProcess(gNow);
    assert(techAlarmManagerStateGet(TECH_ALARM_INSP_PRESS_NOT_REACHED));
    assert(inspPressureCycle(30.0F, 12.0F));
    assert(!inspPressureCycle(30.0F, 20.0F)); /* Offset passes, ratio fails: recover. */
    assert(!inspPressureCycle(5.0F, 2.0F)); /* Exact target-3 equality excluded. */
    assert(!inspPressureCycle(5.0F, 2.5F)); /* Ratio passes, offset fails. */
    assert(!inspPressureCycle(30.0F, 30.0F * 0.6666F)); /* Exact ratio equality excluded. */
    assert(!inspPressureCycle(30.0F, 12.0F));
    assert(!inspPressureCycle(30.0F, 12.0F));
    assert(!inspPressureCycle(30.0F, NAN)); /* Invalid pressure breaks confirmation. */
    assert(!inspPressureCycle(30.0F, 12.0F));
    assert(!inspPressureCycle(30.0F, 12.0F));
    assert(inspPressureCycle(30.0F, 12.0F));
    assert(!inspPressureCycle(0.0F, 1.0F)); /* No positive pressure target clears. */
    assert(!inspPressureCycle(NAN, 1.0F));
    assert(!inspPressureCycle(30.0F, 12.0F));
    assert(!inspPressureCycle(30.0F, 12.0F));
    gPlan.sequence++; /* Missing completed sequence breaks consecutive history. */
    assert(!inspPressureCycle(30.0F, 12.0F));
    assert(!inspPressureCycle(30.0F, 12.0F));
    assert(inspPressureCycle(30.0F, 12.0F));
    sample(PHASE_IDLE, 0.0F, 5.0F);
    assert(!techPhysInspPressNotReachedDetect(gNow));
    reset();
    techPhysInit();
    gPlan.sequence = UINT32_MAX - 1U;
    assert(!inspPressureCycle(30.0F, 12.0F));
    assert(!inspPressureCycle(30.0F, 12.0F));
    assert(inspPressureCycle(30.0F, 12.0F)); /* Sequence wrap remains consecutive. */
}

int main(void) {
    inspPressureNotReached();
    volumeLimitAlarm();
    pressureLimitAlarm();
    pipelineDisconnect();
    pipelineLeak();
    inspBranchBlockage();
    pipelineBlockage();
    pacPlateau();
    peakExpiratoryFlow();
    minuteVolumeBoundaries();
    compliance();
    resistance();
    leakPercent();
    minuteLeak();
    meanPressure();
    cpapAlarms();
    stBreathResult lResult;
    stActuatorRequest lRequest;
    uint16_t lTarget;
    unsigned int lIndex;
    peepAlarms();
    peepDisplayTiming();
    dynamicPeep();
    dynamicPeepFlow();
    knownLeak();
    assert(monitorEngineBreathResultGet(&lResult) == MONITOR_ENGINE_SUCCESS);
    assert(lResult.sequence == 1U);
    /* Real estimator output is shared by pause control and plateau detection. */
    gPause = 1U;
    gData[INSP_REAL_FLOW] = 80.0F;
    for (lIndex = 0U; lIndex < 100U; lIndex++) {
        sample(PHASE_INSP, 10.0F, 25.0F);
        assert(flowControllerProcess(&gPlan, &lRequest) == ACTUATOR_REQUEST_SUCCESS);
    }
    assert(monitorEngineGet(MONITOR_PLATEAU_PRS) == 25.0F);
    lTarget = lRequest.blowerTarget;
    gData[INSP_REAL_FLOW] = 0.0F;
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
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--blockage-only", "--circuit-alarms-only", action="store_true",
                        help="Run circuit blockage, branch blockage, leak and disconnect regressions only")
    args = parser.parse_args()
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
        source = HARNESS
        if args.blockage_only:
            source = source.replace("    pipelineBlockage();", "    pipelineBlockage();\n    return 0;", 1)
        harness.write_text(source, encoding="utf-8", newline="\n")
        executable = Path(directory) / "monitor_leak_test.exe"
        includes = ["user/app/physalarm", "user/app/techalarm", "user/app/ventlogic", "user/app/ventalgo", "user/app/databus",
                    "user/app/calibration", "user/module/rtos", "user/tools/controller"]
        command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                   *[f"-I{ROOT / path}" for path in includes], str(harness),
                   str(ROOT / "user/app/ventlogic/monitorengine.c"),
                   str(ROOT / "user/app/ventlogic/pipeflowtable.c"),
                   str(ROOT / "user/app/physalarm/physalarmvent.c"),
                   str(ROOT / "user/app/physalarm/physalarmapnea.c"),
                   str(ROOT / "user/app/physalarm/physalarmmanager.c"),
                   str(ROOT / "user/app/techalarm/techalarmmanager.c"),
                   str(ROOT / "user/app/techalarm/techphys.c"),
                   str(ROOT / "user/app/techalarm/techdevice.c"),
                   str(ROOT / "user/app/techalarm/techpower.c"),
                   str(ROOT / "user/app/techalarm/techcomm.c"),
                   str(ROOT / "user/app/techalarm/techcal.c"),
                   str(ROOT / "user/app/ventalgo/flowcontroller.c"),
                   str(ROOT / "user/tools/controller/pid.c"), "-o", str(executable)]
        environment = os.environ.copy()
        environment["PATH"] = str(Path(compiler).parent) + os.pathsep + environment["PATH"]
        subprocess.run(command, check=True, env=environment)
        subprocess.run([str(executable)], check=True, env=environment)
    if args.blockage_only:
        print("PASS: circuit/branch blockage, leak, disconnect confirmation, raw coefficient, shared recovery, boundaries and timing")
    else:
        print("PASS: PEEP alarms and recovery, dynamic PEEP windows, PAC latched display and PSV/ST stable live display, leak estimate, pause integration, boundaries, invalid data, restart, re-zero, limits")


if __name__ == "__main__":
    main()
