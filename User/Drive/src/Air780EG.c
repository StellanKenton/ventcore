//
// Created by lanchanghai on 2024/3/15.
//

#include "Air780EG.h"
#include "FreeRTOS.h"
#include "semphr.h"

uint8_t rxbuffer[256];

__IO uint8_t rx_count = 0;
SemaphoreHandle_t BinarySemaphore;

UsartClass Air780EGPort={
    .UsartPort = USARTPort,
    .BaudRate = 115200U,
    .GpioTXPort = GPIOA,
    .GpioRXPort = GPIOD,
    .GpioTXPin = GPIO_PIN_2,
    .GpioRXPin = GPIO_PIN_6
};

void Air780EGInit(void)
{
    //初始化Air780EG

    nvic_irq_enable(USARTPortIRQ,5,0);

    BinarySemaphore = xSemaphoreCreateBinary();

    dma_single_data_parameter_struct dma_init_struct;

    rcu_periph_clock_enable(DmaClock);

    dma_deinit(DmaPeriph, DmaChannel);
    dma_init_struct.direction = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.memory0_addr = (uint32_t)rxbuffer;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.number = 256;
    dma_init_struct.periph_addr = ((uint32_t)&USART_DATA(USARTPort));
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(DmaPeriph, DmaChannel, &dma_init_struct);

    /* 配置 DMA 模式 */
    dma_circulation_disable(DmaPeriph, DmaChannel); // 禁止循环模式
    dma_channel_subperipheral_select(DmaPeriph, DmaChannel, DmaSubPeriph);
    /* 启用 DMA0 通道5 */
    dma_channel_enable(DmaPeriph, DmaChannel);

    UsartInit(&Air780EGPort);
    usart_disable(Air780EGPort.UsartPort);
    usart_dma_receive_config(Air780EGPort.UsartPort, USART_RECEIVE_DMA_ENABLE);
    usart_enable(Air780EGPort.UsartPort);

    usart_interrupt_enable(USARTPort, USART_INT_IDLE);

    Air780EGPort.printf("AT+CGMM\r\n ");
}