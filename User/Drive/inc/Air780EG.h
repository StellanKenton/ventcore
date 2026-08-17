//
// Created by lanchanghai on 2024/3/15.
//

#ifndef GD32F470_FREERTOS_AIR780EG_H
#define GD32F470_FREERTOS_AIR780EG_H

#include "gd32f4xx.h"
#include "Usart.h"
#include "FreeRTOS.h"
#include "semphr.h"

#define USARTPort (USART1)
#define USARTPortIRQ USART1_IRQn

#define DmaClock (RCU_DMA0)
#define DmaPeriph (DMA0)
#define DmaChannel (DMA_CH5)
#define DmaSubPeriph (DMA_SUBPERI4) //USART1_RX

extern uint8_t rxbuffer[256];
extern __IO uint8_t rx_count;
extern SemaphoreHandle_t BinarySemaphore;
extern UsartClass Air780EGPort;

void Air780EGInit(void);

#endif  // GD32F470_FREERTOS_AIR780EG_H
