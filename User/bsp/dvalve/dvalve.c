/************************************************************************************
* @file     : dvalve.c
* @brief    : Board proportional valve PWM driver.
* @details  : Drives the oxygen, relief, and expiratory valves at 20 kHz.
* @author   :
* @date     :
* @version  :
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "dvalve.h"

#include "gd32f4xx_gpio.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_timer.h"



static void dvalveTimerInit(eDvalveIndex valveIndex)
{
    timer_parameter_struct lTimerConfig;
    timer_oc_parameter_struct lOutputConfig;
    uint32_t lTimer = gDvalveTimers[valveIndex];
    uint16_t lChannel = gDvalveTimerChannels[valveIndex];

    rcu_periph_clock_enable(gDvalveTimerClocks[valveIndex]);
    timer_deinit(lTimer);

    timer_struct_para_init(&lTimerConfig);
    lTimerConfig.prescaler = 0U;
    lTimerConfig.alignedmode = TIMER_COUNTER_EDGE;
    lTimerConfig.counterdirection = TIMER_COUNTER_UP;
    lTimerConfig.clockdivision = TIMER_CKDIV_DIV1;
    lTimerConfig.period = gDvalveTimerPeriods[valveIndex];
    lTimerConfig.repetitioncounter = 0U;
    timer_init(lTimer, &lTimerConfig);

    timer_channel_output_struct_para_init(&lOutputConfig);
    lOutputConfig.outputstate = TIMER_CCX_ENABLE;
    lOutputConfig.ocpolarity = TIMER_OC_POLARITY_HIGH;
    timer_channel_output_config(lTimer, lChannel, &lOutputConfig);
    timer_channel_output_pulse_value_config(lTimer, lChannel, 0U);
    timer_channel_output_mode_config(lTimer, lChannel, TIMER_OC_MODE_PWM0);
    timer_channel_output_shadow_config(lTimer, lChannel, TIMER_OC_SHADOW_ENABLE);
    timer_auto_reload_shadow_enable(lTimer);
    timer_enable(lTimer);
}

void dvalveInit(void)
{
    uint32_t lIndex;

    rcu_periph_clock_enable(RCU_GPIOB);
    for (lIndex = 0U; lIndex < (uint32_t)DVALVE_COUNT; ++lIndex) {
        gpio_af_set(GPIOB, gDvalveGpioAf[lIndex], gDvalvePins[lIndex]);
        gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_PULLUP, gDvalvePins[lIndex]);
        gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, gDvalvePins[lIndex]);
        dvalveTimerInit((eDvalveIndex)lIndex);
    }
}

int8_t dvalveDutySet(eDvalveIndex valveIndex, uint8_t dutyPercent)
{
    uint32_t lPulse;

    if ((uint32_t)valveIndex >= (uint32_t)DVALVE_COUNT) {
        return DVALVE_ERROR_INVALID_INDEX;
    }
    if (dutyPercent > DVALVE_DUTY_MAX_PERCENT) {
        return DVALVE_ERROR_INVALID_DUTY;
    }

    lPulse = ((gDvalveTimerPeriods[valveIndex] + 1U) * (uint32_t)dutyPercent) /
             DVALVE_DUTY_MAX_PERCENT;
    timer_channel_output_pulse_value_config(gDvalveTimers[valveIndex],
                                            gDvalveTimerChannels[valveIndex],
                                            lPulse);
    return DVALVE_STATUS_OK;
}

/**************************End of file********************************/
