#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "helper.h"
#include "lcd.h"

/*Segunda consigna: Implementar la consigna a través de una tarea que lea el estado del GPIO 
*por polling y mostrar el valor de frecuencia por consola.*/
/*---------------------------------------Definiciones------------------------------------------------------------------------*/
#define INPUT_PIN 15          // Pin GPIO para la señal de entrada (pin 20 de la placa)
#define pwm 6               //Pin de salida PWM de 10KHz (pin 9 de la placa)
/*--------------------------------------Variables de FreeRTOS y de codigo---------------------------------------------------*/ 
uint16_t fre = 10000;           //Fijo la frecuencia del PWM
volatile uint64_t count=0;
QueueHandle_t pulse_count;      //Cola para trasferir datos de manera segur<
SemaphoreHandle_t xSemaphore;  // Semáforo para sincronización
/*---------------------------------------Funcion de interrupcion para FreeRTOS------------------------------------------------------*/
void gpio_callback(uint gpio, uint32_t event)
{BaseType_t THP = pdFALSE;
 
    //Verifico que la interrupcion corresponda con el pin usado, se peude obviar ya que se usa solo un pin
    if(gpio == INPUT_PIN  && (event & GPIO_IRQ_EDGE_RISE))
    {
        count++;
    }
    portYIELD_FROM_ISR(THP);
}
/*-----------------------------------------------------------TAREAS-----------------------------------------------------------------*/
//----------------------------------------Tarea para calcular y mostrar frecuencia--------------------------------------------------
void frequency_calculator_task(void *pvParameters) 
{
    uint64_t aux = 0;
    float window = 1, f = 0;   
   
    while (1) 
    {   
        if(xSemaphoreTake(xSemaphore,0) == pdTRUE)
        {
            aux = count;
            count = 0;
            f = aux / window;  
            printf("Pulsos= %llu, Frecuencia= %.2f Hz \n", aux, f);
        }
        xSemaphoreGive(xSemaphore);  
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
//----------------------------------------------Tarea para configuracion-------------------------------------------------------------
void set_init(void *pvParameter)
{
    printf("Inicializando parametros\n");
    pwm_user_init(pwm, fre);
    
    // Inicializar hardware
    gpio_init(INPUT_PIN);
    gpio_set_dir(INPUT_PIN, GPIO_IN);
    //gpio_pull_down(INPUT_PIN);
    gpio_set_irq_enabled_with_callback(INPUT_PIN,GPIO_IRQ_EDGE_RISE,true,&gpio_callback);
    // Crear cola y semáforo
    pulse_count = xQueueCreate(1, sizeof(uint64_t));
    xSemaphore = xSemaphoreCreateMutex();

    // Verificar que se crearon los objetos RTOS correctamente
    if(pulse_count == NULL || xSemaphore == NULL) 
    {
        printf("Error al crear objetos RTOS\n");
        while(1);
    }
    printf("Fin de inicializacion, recursos liberados\n");
    vTaskDelete(NULL);
}
/*----------------------------------------------Programa principal------------------------------------------------------------------*/
int main() 
{
    stdio_init_all();
    
    // Crear tareas
    xTaskCreate(set_init,"Seteo",256,NULL,2,NULL);
    xTaskCreate(frequency_calculator_task, "FreqCalculator", 256, NULL, 1, NULL);
    // Iniciar scheduler
    vTaskStartScheduler();
    // Nunca deberíamos llegar aquí
    while (1) 
    {}
}