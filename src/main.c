
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

struct sensor_data {
    float ax;
    float ay;
    float az;
    float gx;
    float gy;
    float gz;
    float t;
}


void sensorTask(void *pvParameters)
{
    (void)pvParameters;
    // Kerää dataa liikesensorilta ja tulkitsee liikekomennot
    // Collects data from movement-sensor and recognizes movement commands
    while (1)
    {
        if (programState == IDLE) {
            programState = READ_SENSOR;
        }

        if (programState == READ_SENSOR) {
            // Reads sensor data, detects the orientation and checks if button pressed.
            read_data();
            if (/*Button pressed*/) {
                // Save orientation and the corresponding symbol (Dash or dot)

                programState = DATA_READY;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void read_data() {

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
            
            //printf("Accel: X=%f, Y=%f, Z=%f | Gyro: X=%f, Y=%f, Z=%f| Temp: %2.2f°C\n", ax, ay, az, gx, gy, gz, t);
            
            struct sensor_data data.ax = ax;
            struct sensor_data data.ay = ay;
            struct sensor_data data.az = az;
            struct sensor_data data.gx = gx;
            struct sensor_data data.gy = gy;
            struct sensor_data data.gz = gz;
            struct sensor_data data.t = t;
            

        } else {
            printf("Failed to read imu data\n");
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

}

void morseTask(void *pvParameters)
{
    (void)pvParameters;
    // Muuttaa raakadatan morse-merkeiksi
    // transforms raw data to morse symbols
    while (1)
    {
        receive_Data();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}



void transmitTask(void *pvParameters)
{
    (void)pvParameters;
    // Lähettää morsekoodin tietokoneelle
    // Sends morse-code to the computer using USB
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void receiveTask(void *pvParameters)
{
    (void)pvParameters;
    // Vastaanottaa morseviestin tietokoneelta
    // Receives morse-code message from the computer using USB
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(2000));
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
            void init_display(void);
            char text[] = ":)";
            void write_text(const char *text);
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
    TaskHandle_t hImuTask = NULL;

    

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