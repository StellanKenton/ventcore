/************************************************************************************
* @file     : venttest.h
* @brief    : Ventilation test console command.
* @details  : Declares ventilation test commands and incremental transient logging.
* @author   :
* @date     :
* @version  :
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef VENTTEST_H
#define VENTTEST_H

#include <stdbool.h>
#include <stdint.h>

#define VENT_TEST_TRANSIENT_SAMPLE_INTERVAL_MS  6U
#define VENT_TEST_TRANSIENT_DURATION_MS         2000U
#define VENT_TEST_TRANSIENT_SAMPLE_COUNT        ((VENT_TEST_TRANSIENT_DURATION_MS + \
                                                  VENT_TEST_TRANSIENT_SAMPLE_INTERVAL_MS - 1U) / \
                                                 VENT_TEST_TRANSIENT_SAMPLE_INTERVAL_MS)

typedef struct stVentTestTransientSample {
    int32_t patientPressureCenti;
    int32_t inspPressureCenti;
    int32_t pressureReferenceCenti;
    int32_t airFlowCenti;
    int32_t o2FlowCenti;
    int32_t expFlowCenti;
    uint16_t blowerTarget;
    uint16_t blowerActual;
    uint8_t phase;
    uint8_t valveDuty;
} stVentTestTransientSample;

#ifdef __cplusplus
extern "C" {
#endif

bool ventTestConsoleRegister(void);
void ventTestTransientRecord(void);

#ifdef __cplusplus
}
#endif

#endif /* VENTTEST_H */
/**************************End of file********************************/
