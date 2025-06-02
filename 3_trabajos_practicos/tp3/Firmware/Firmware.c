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

/*----------------------------------------NOTA-------------------------------------------------------------------------------*/
//SE uso una ventana de 10 segundos la cual fue implementada en la API vTaskDelay(). Esto indicara que la tarea de bloqueara
//por 10 segundos y luego calculara la frecuencia e impirmira el resultaado en el LCD. Si se tomara una ventana de 1 segundo
//el error seria mayor.
/*---------------------------------------------------------------------------------------------------------------------------*/

/*---------------------------------------Definiciones------------------------------------------------------------------------*/
#define INPUT_PIN 15          // Pin GPIO para la señal de entrada (pin 20 de la placa)
#define pwm 6               //Pin de salida PWM de 10KHz (pin 9 de la placa)
#define sda      2          //Pin para SDA (pin 4 de la placa)
#define sck      3          //Pin para SCK (pin 5 de la placa)
/*--------------------------------------Variables de FreeRTOS y de codigo---------------------------------------------------*/ 
uint16_t fre = 10000;           //Fijo la frecuencia del PWM
volatile uint64_t count=0;
QueueHandle_t pulse_count;      //Cola para trasferir datos de manera segur<
SemaphoreHandle_t xSemaphore;  // Semáforo para sincronización
char text1[20];                 //Buffer destiando a almacenar la cantidad los pulsos
char text2[20];                 //Buffer destinado a alamceanr la frecuencia calculada
float ventana=10.0;
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
    float f = 0;   
   
    while (1) 
    {   
        if(xSemaphoreTake(xSemaphore,0) == pdTRUE)
        {
            aux = count;
            count = 0;
            f = aux / ventana;
            lcd_set_cursor(1,0);
            sprintf(text1,"Pulsos=%llu",aux);  
            lcd_string(text1);
            lcd_set_cursor(2,0);
            sprintf(text2,"Frecuencia=%.2f ",f);
            lcd_string(text2); 
            //printf("Pulsos= %llu, Frecuencia= %.2f Hz \n", aux, f);
        }
        xSemaphoreGive(xSemaphore);  
        vTaskDelay(pdMS_TO_TICKS(10000));
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
/*----------------------------------------------Tarea para setear el LCD------------------------------------------------------------*/
void init_lcd(void *pvParameter)
{
    // Inicializar hardware
    i2c_init(i2c1, 100000);
    // Habilito la funcion de I2C en los GPIOs
    gpio_set_function(sda, GPIO_FUNC_I2C);
    gpio_set_function(sck, GPIO_FUNC_I2C);
    // Habilito pull-ups
    gpio_pull_up(sda);
    gpio_pull_up(sck);
    // Inicializo LCD
    lcd_init(i2c1, 0x27);
    // Limpio el LCD
    lcd_clear();
    // Escribo al comienzo
    lcd_string("Hello");
    // Muevo el cursor a la segunda fila, tercer columna
    lcd_set_cursor(1, 2);
    // Escribo
    lcd_string("from RPi Pico!");
    lcd_clear();
    lcd_string("Iniciando");
    vTaskDelay(5000);
    lcd_clear();
    lcd_set_cursor(0,0);
    lcd_string("Test con 10KHz");
    vTaskDelete(NULL);
}
/*----------------------------------------------Programa principal------------------------------------------------------------------*/
int main() 
{
    stdio_init_all();
    
    // Crear tareas
    xTaskCreate(set_init,"Seteo",256,NULL,2,NULL);
    xTaskCreate(init_lcd,"LCD",256,NULL,3,NULL);
    xTaskCreate(frequency_calculator_task, "FreqCalculator", 256, NULL, 1, NULL);
    // Iniciar scheduler
    vTaskStartScheduler();
    // Nunca deberíamos llegar aquí
    while (1) 
    {}
}