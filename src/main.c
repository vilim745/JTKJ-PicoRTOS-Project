
#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"

// Default stack size for the tasks. It can be reduced to 1024 if task is not using lot of memory.
#define DEFAULT_STACK_SIZE 2048
// #define DISPLAY_WIDTH
// #define MSG_BUFFER

// Add here necessary states
enum state
{
    IDLE = 1,
    READ_SENSOR,
    DATA_READY,
    MSG_READY,
    NEW_MSG,
    UPDATE_LCD
};
enum state programState = IDLE;

static void example_task(void *arg)
{
    (void)arg;

    for (;;)
    {
        tight_loop_contents(); // Modify with application code here.
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}


void sensorTask(void *pvParameters)
{
    // Kerää dataa liikesensorilta ja tulkitsee liikekomennot
    // Collects data from movement-sensor and recognizes movement commands
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void morseTask(void *pvParameters)
{
    // Muuttaa raakadatan morse-merkeiksi
    // transforms raw data to morse symbols
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void transmitTask(void *pvParameters)
{
    // Lähettää morsekoodin tietokoneelle
    // Sends morse-code to the computer using USB
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void receiveTask(void *pvParameters)
{
    // Vastaanottaa morseviestin tietokoneelta
    // Receives morse-code message from the computer using USB
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void displayTask(void *pvParameters)
{
    // Näyttää vastaanotetun tekstin näytöllä
    // Displays the received message on the screen
    while (1)
    {
        (void)pvParameters;
        if (programState == UPDATE_LCD)
        {
            update_screen();
            programState = IDLE;
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

int main()
{
    stdio_init_all();
    // Uncomment this lines if you want to wait till the serial monitor is connected
    /*while (!stdio_usb_connected()){
        sleep_ms(10);
    }*/
    init_hat_sdk();
    sleep_ms(300); // Wait some time so initialization of USB and hat is done.

    TaskHandle_t hSensorTask = NULL;
    TaskHandle_t hMorseTask = NULL;
    TaskHandle_t hTransmitTask = NULL;
    TaskHandle_t hReceiveTask = NULL;
    TaskHandle_t hDisplayTask = NULL;

    if(xTaskCreate(sensorTask, "Sensor", DEFAULT_STACK_SIZE, NULL, 2, &hSensorTask)) != pdPASS {
        printf("Sensor Task creation failed\n");
        return 0;
    };

    if(xTaskCreate(morseTask, "Morse", DEFAULT_STACK_SIZE, NULL, 2, &hMorseTask)) != pdPASS {
        printf("Morse Task creation failed\n");
        return 0;
    };

    if(xTaskCreate(transmitTask, "Transmit", DEFAULT_STACK_SIZE, NULL, 2, &hTransmitTask)) != pdPASS {
        printf("Transmit Task creation failed\n");
        return 0
        ;
    };

    if(xTaskCreate(receiveTask, "Receive", DEFAULT_STACK_SIZE, NULL, 2, &hReceiveTask)) != pdPASS {
        printf("Receive Task creation failed\n");
        return 0;
    };

    if(xTaskCreate(displayTask, "Display", DEFAULT_STACK_SIZE, NULL, 2, &hDisplayTask)) != pdPASS {
        printf("Display Task creation failed\n");
        return 0;
    };


    // Start the scheduler (never returns)
    vTaskStartScheduler();

    // Never reach this line.
    return 0;
}