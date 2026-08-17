#include "gd32f4xx.h"
#include "FreeRTOS.h"
#include "Usart.h"
#include <stdio.h>
#include "task.h"
#include "delay.h"
#include "semphr.h"
#include "Air780EG.h"


TaskHandle_t StartTask_Handler;

void CrateTask(void *pvParameters);
void Air780EG_task(void *pvParameters);

UsartClass Debug={
    .UsartPort = USART0,
    .BaudRate = 115200U,
    .GpioTXPort = GPIOA,
    .GpioRXPort = GPIOA,
    .GpioTXPin = GPIO_PIN_9,
    .GpioRXPin = GPIO_PIN_10
};

int main(void)
{
    /* configure systick */
    delay_init(240);
    UsartInit(&Debug);
    setvbuf(stdout, NULL, _IONBF, 0);
    Debug.printf("System init...\r\n ");
    xTaskCreate(CrateTask ,"CrateTask" , 1024, NULL, 1, &StartTask_Handler);
    vTaskStartScheduler();          //�����������
    while(1)
    {
    }
}

void CrateTask(void *pvParameters){

    taskENTER_CRITICAL();
    Air780EGInit();
    //xSemaphoreTake(BinarySemaphore,portMAX_DELAY);
    //xSemaphoreGive(BinarySemaphore);
    xTaskCreate(Air780EG_task, "Air780EG_task" ,1024 ,NULL , 3,0);
    taskEXIT_CRITICAL();
    vTaskDelete(StartTask_Handler);
}

void Air780EG_task(void *pvParameters)
{
    BaseType_t err = pdPASS;
    while(1)
    {
        if(BinarySemaphore != NULL) {
            err = xSemaphoreTake(BinarySemaphore, portMAX_DELAY);
            if (err == pdPASS) {
                rxbuffer[rx_count] = '\0';
                Debug.printf("Air780EG:%s\r\n", rxbuffer);
            }
        }
    }
}

