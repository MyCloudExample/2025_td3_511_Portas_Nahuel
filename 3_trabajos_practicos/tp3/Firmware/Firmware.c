#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "helper.h"

#define INPUT_PIN 15          // Pin GPIO para la señal de entrada (pin 20 de la placa)
#define pwm 6               //Pin de salida PWM de 10KHz (pin 9 de la placa)

/*Primera consigna: Implementar la consigna a través de una tarea que lea el estado del GPIO 
*por polling y mostrar el valor de frecuencia por consola.*/

/*--------------------------------------Variables de FreeRTOS y de codigo---------------------------------------------------*/ 
uint16_t fre = 10000;           //Fijo la frecuencia del PWM
QueueHandle_t pulse_count;      //Cola para trasferir datos de manera segur<
SemaphoreHandle_t xSemaphore;  // Semáforo para sincronización
/*-----------------------------------------------------------TAREAS-----------------------------------------------------------------*/
//--------------------------------------------Tarea para contar pulsos-----------------------------------------------------------
void task_pulsos(void *pvParameters) 
{
   uint64_t count = 0;
   while (1) 
    {
        if(xSemaphoreTake(xSemaphore,0)  == pdTRUE)
        {
            count = 0;
        }
        if(gpio_get(INPUT_PIN))
        {
            count++;
            xQueueOverwrite(pulse_count, &count);
            while(gpio_get(INPUT_PIN))
            {}
        }       
    }
}
//----------------------------------------Tarea para calcular y mostrar frecuencia--------------------------------------------------
void frequency_calculator_task(void *pvParameters) 
{
    uint64_t count = 0;
    float window = 1, f = 0;   
   
    while (1) 
    {   
        xSemaphoreGive(xSemaphore);
        if(xQueueReceive(pulse_count, &count, 0) == pdTRUE)
        {
            f = count / window;  
            printf("Pulsos= %llu, Frecuencia= %.2f Hz \n", count, f);
        }  
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
/*----------------------------------------------Programa principal------------------------------------------------------------------*/
int main() 
{
    stdio_init_all();
    printf("Medidor de Frecuencia con FreeRTOS\n");
    pwm_user_init(pwm, fre);
    
    // Inicializar hardware
    gpio_init(INPUT_PIN);
    gpio_set_dir(INPUT_PIN, GPIO_IN);
    gpio_pull_down(INPUT_PIN);

    // Crear cola y semáforo
    pulse_count = xQueueCreate(1, sizeof(uint64_t));
    xSemaphore = xSemaphoreCreateMutex();

    // Verificar que se crearon los objetos RTOS correctamente
    if(pulse_count == NULL || xSemaphore == NULL) 
    {
        printf("Error al crear objetos RTOS\n");
        while(1);
    }
    // Crear tareas
    xTaskCreate(task_pulsos, "PulseCounter", 256, NULL, 1, NULL);
    xTaskCreate(frequency_calculator_task, "FreqCalculator", 256, NULL, 2, NULL);
    // Iniciar scheduler
    vTaskStartScheduler();
    // Nunca deberíamos llegar aquí
    while (1) 
    {}
}