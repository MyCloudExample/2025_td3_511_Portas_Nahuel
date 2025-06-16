#include <stdio.h>
#include "pico/stdlib.h"
#include "lcd.h"
#include "bmp280.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/*Consigna 1: Gestionar los recursos del microcontrolador para que **compartiendo el mismo bus de I2C** se pueda ver
* la temperatura en grados Celsius y la presión en KPa con su unidad en las primeras dos líneas del display LCD.*/

/*Para esta parte se considero el uso de 3 tareas:
1- La tarea de inicializacion general configura el I2C junto a sus pines, inicializa el BMP280 y el LCD junto con 
mensajes de prueba para cada lina del LCD
2- La tarea task_bmp280 lee el estado cada 1 segundo y luego envia por una cola los datos a la latara task_lcd
3- La tarea task_lcd muestra los datos obtenido y convertido de temperatura y presion
NOTA: El caracter ° no se imprimie en e LCD
Se utilizo semaforo mutex para controlar el acceso al recusrso compartido I2C y una cola para la trasnferencia
segura de datos entre tareas*/

/*----------------------------------------MACROS-------------------------------------------------------------------*/
#define I2C         i2c0 //Utilizo el bus 0 del i2c
#define PIN_SDA     4 //Corresponde al pin 6 de la placa
#define PIN_SCL     5 //Corresponde al pin 7 de la placa
#define I2C_FREQ    400000 //Se define una frecuencia de 400 KHz
#define LCD_ADDR    0x27 //Direccion del LCD
#define BMP_ADDR    0xFF //Direccion del BMP280
/*------------------------------------Variables, incluidas las de FreeRTOS y estructuras-------------------------------*/
//---------------------------------------Estructura que alamacena los datos del BMP280----------------------------------
typedef struct 
{
    float temp;
    uint32_t pre;    
}data_t;
//------------------------------------------------Manejadores de cola y semaforo-----------------------------------------
SemaphoreHandle_t sinc;
QueueHandle_t datos;
/*------------------------------------------------Tareas--------------------------------------------------------------*/
//------------------------------------------------Lectura y conversion de datos del BMP280-----------------------------
void task_bmp280(void* pvParameters)
{ struct bmp280_calib_param params;
  int32_t raw_temp, raw_presion;
  float temp_c;
  int32_t presion_pa;
  data_t tp;

  while (1)
  {
    if(xSemaphoreTake(sinc, portMAX_DELAY) == pdTRUE)
    {
        bmp280_read_raw(&raw_temp, &raw_presion);
        temp_c = bmp280_convert_temp(raw_temp, &params);
        presion_pa = bmp280_convert_pressure(raw_presion, raw_temp, &params);

        tp.temp = temp_c;
        tp.pre = presion_pa; 

        printf("Desde la tarea BMP280\n");
        printf("Temperatura= %.2f °C\n",temp_c);
        printf("Presion= %ld Pa\n",presion_pa);
        xQueueSend(datos, &tp, pdMS_TO_TICKS(10));
        vTaskDelay(pdMS_TO_TICKS(1000)); //tiempo de espera
        xSemaphoreGive(sinc);
    }   
  }
}
//---------------------------------------------------Impresion de informacion-------------------------------------------
void task_lcd(void* pvParameters)
{ char msg1[30], msg2[30];
  data_t tplcd;
  lcd_set_cursor(2,0);
  lcd_string("Ejecutando tareas:");
  lcd_set_cursor(3,0);
  lcd_string("task_bmp280 task_lcd");
  vTaskDelay(pdMS_TO_TICKS(10));

  while (1)
  {
    if(xSemaphoreTake(sinc,portMAX_DELAY) == pdTRUE)
    {   
        xQueueReceive(datos, &tplcd, pdMS_TO_TICKS(100));
        sprintf(msg1,"T= %.2f Celcius ",tplcd.temp);
        sprintf(msg2,"P= %ld Pascales ",tplcd.pre);
        //lcd_clear();
        lcd_set_cursor(0,0);
        lcd_string(msg1);
        lcd_set_cursor(1,0);
        lcd_string(msg2);

        /*printf("Desde la tarea LCD\n");
        printf("Temperatura= %.2f °C\n",tplcd.temp);
        printf("Presion= %ld Pa\n",tplcd.pre);*/
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    xSemaphoreGive(sinc);
    vTaskDelay(pdMS_TO_TICKS(100));
    
  }
}
//----------------------------------Tarea de inicializacion, solo se jecuta una vez y luego se elimina------------------
void init_general(void* pvParameters)
{
    //Inicializo y configuro el I2C0
    i2c_init(I2C, I2C_FREQ);
    gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_SDA);
    gpio_pull_up(PIN_SCL);
    //Inicializo el BMP280
    bmp280_init(I2C);
    
    //Inicializo y configuro el LCD
    lcd_init(I2C,LCD_ADDR);
    lcd_clear();
    lcd_set_cursor(0,0);
    lcd_string("Procesando...");
    lcd_set_cursor(1,0);
    lcd_string("Sensor activado");
    lcd_set_cursor(2,0);
    lcd_string("LCD activado");
    lcd_set_cursor(3,0);
    lcd_string("Tarea Borrada");
    lcd_clear();

    vTaskDelete(NULL);
}
/*------------------------------------Progrma principal-------------------------------------------------------------------*/
int main()
{
    stdio_init_all();

    sinc = xSemaphoreCreateMutex();
    datos = xQueueCreate(5,sizeof(data_t));

    if(sinc == NULL || datos == NULL)
    {
        printf("Problemas para crear la cola o el semaforo revisar\n");
    }

    xTaskCreate(task_bmp280,"BMP280",256,NULL,1,NULL);
    xTaskCreate(task_lcd,"LCD",256,NULL,2,NULL);
    xTaskCreate(init_general,"Inicializacion",256,NULL,4,NULL);
    xSemaphoreGive(sinc);

    vTaskStartScheduler();

    while (true) 
    {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }
}
