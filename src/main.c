
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
    (void)pvParameters;
    // Kerää dataa liikesensorilta ja tulkitsee liikekomennot
    // Collects data from movement-sensor and recognizes movement commands
    while (1)
    {
    }
}

void morseTask(void *pvParameters)
{
    (void)pvParameters;
    // Muuttaa raakadatan morse-merkeiksi
    // transforms raw data to morse symbols
    while (1)
    {
    }
}

void transmitTask(void *pvParameters)
{
    (void)pvParameters;
    // Lähettää morsekoodin tietokoneelle
    // Sends morse-code to the computer using USB
    while (1)
    {
    }
}

void receiveTask(void *pvParameters)
{
    (void)pvParameters;
    // Vastaanottaa morseviestin tietokoneelta
    // Receives morse-code message from the computer using USB
    while (1)
    {
    }
}

void displayTask(void *pvParameters)
{  
    (void)pvParameters;
    // Näyttää vastaanotetun tekstin näytöllä
    // Displays the received message on the screen
    while (1)
    {
        if (programState == UPDATE_LCD)
        {

                char text[] = ":)";
                void write_text(const char *text);

            //update_screen();

            programState = IDLE;
        }
    }

    
}

void imu_task(void *pvParameters) {
    // Taken from examples, used for printing received gyro data to terminal
    (void)pvParameters;
    
    float ax, ay, az, gx, gy, gz, t;
    // Setting up the sensor. 
    if (init_ICM42670() == 0) {
        printf("ICM-42670P initialized successfully!\n");
        if (ICM42670_start_with_default_values() != 0){
            printf("ICM-42670P could not initialize accelerometer or gyroscope");
        }
        /*int _enablegyro = ICM42670_enable_accel_gyro_ln_mode();
        printf ("Enable gyro: %d\n",_enablegyro);
        int _gyro = ICM42670_startGyro(ICM42670_GYRO_ODR_DEFAULT, ICM42670_GYRO_FSR_DEFAULT);
        printf ("Gyro return:  %d\n", _gyro);
        int _accel = ICM42670_startAccel(ICM42670_ACCEL_ODR_DEFAULT, ICM42670_ACCEL_FSR_DEFAULT);
        printf ("Accel return:  %d\n", _accel);*/
    } else {
        printf("Failed to initialize ICM-42670P.\n");
    }
    // Start collection data here. Infinite loop. 
    while (1)
    {
        if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0) {
            
            printf("Accel: X=%f, Y=%f, Z=%f | Gyro: X=%f, Y=%f, Z=%f| Temp: %2.2f°C\n", ax, ay, az, gx, gy, gz, t);

        } else {
            printf("Failed to read imu data\n");
        }
        vTaskDelay(pdMS_TO_TICKS(500));
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

    TaskHandle_t myExampleTask = NULL;
    // Create the tasks with xTaskCreate
    BaseType_t result = xTaskCreate(example_task,       // (en) Task function
                                    "example",          // (en) Name of the task
                                    DEFAULT_STACK_SIZE, // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                                    NULL,               // (en) Arguments of the task
                                    2,                  // (en) Priority of this task
                                    &myExampleTask);    // (en) A handle to control the execution of this task

    if (result != pdPASS)
    {
        printf("Example Task creation failed\n");
        return 0;
    }

    // Start the scheduler (never returns)
    vTaskStartScheduler();

    // Never reach this line.
    return 0;
}