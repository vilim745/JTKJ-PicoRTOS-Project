
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

//Add here necessary states
enum state { IDLE=1, READ_SENSOR, DATA_READY, MSG_READY, NEW_MSG, UPDATE_LCD };
enum state programState = IDLE;

static void example_task(void *arg){
    (void)arg;

    for(;;){
        tight_loop_contents(); // Modify with application code here.
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

int main() {
    stdio_init_all();
    // Uncomment this lines if you want to wait till the serial monitor is connected
    /*while (!stdio_usb_connected()){
        sleep_ms(10);
    }*/ 
    init_hat_sdk();
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.

    TaskHandle_t myExampleTask = NULL;
    // Create the tasks with xTaskCreate
    BaseType_t result = xTaskCreate(example_task,       // (en) Task function
                "example",              // (en) Name of the task 
                DEFAULT_STACK_SIZE, // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,               // (en) Arguments of the task 
                2,                  // (en) Priority of this task
                &myExampleTask);    // (en) A handle to control the execution of this task

    if(result != pdPASS) {
        printf("Example Task creation failed\n");
        return 0;
    }

    // Start the scheduler (never returns)
    vTaskStartScheduler();

    // Never reach this line.
    return 0;


    void sensorTask() {
        // Kerää dataa liikesensorilta ja tulkitsee liikekomennot
        // Collects data from movement-sensor and recognizes movement commands
    }

    void morseTask() {
        // Muuttaa raakadatan morse-merkeiksi
        // transforms raw data to morse-code
    }

    void transmitTask() {
        // Lähettää morsekoodin tietokoneelle
        // Sends morse-code to the computer using USB
    }

    void receiveTask() {
        // Vastaanottaa morseviestin tietokoneelta
        // Receives morse-code message from the computer using USB
    }

    void displayTask() {
        // Näyttää vastaanotetun tekstin näytöllä
        // Displays the received message on the screen

    }



}

