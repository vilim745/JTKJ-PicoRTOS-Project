
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"

// Default stack size for the tasks. It can be reduced to 1024 if task is not using lot of memory.
#define DEFAULT_STACK_SIZE 2048
// #define DISPLAY_WIDTH
#define MSG_BUFFER 100

// Add here necessary states
enum state
{
    IDLE = 1,
    READ_SENSOR,
    MSG_READY,
    NEW_MSG,
    UPDATE_OLED
};


enum state programState = IDLE;

// Array for morse code from sensor
char morse_code[MSG_BUFFER] = "";

// Array for morse code from received computer
char received_code[MSG_BUFFER] = "";

bool last_morse_was_space = false;

void clkFxn(void *pvParameters) {
    (void)pvParameters;

    while (1) {
        if (programState == IDLE && getchar_timeout_us(10) >= 0) {
            programState = NEW_MSG;
        } else if (programState == IDLE && gpio_get(SW2_PIN) == 1) {
            printf("Button pressed, starting to read sensor data\n");
            programState = READ_SENSOR;
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void sensorTask(void *pvParameters)
{
    // Kerää dataa liikesensorilta ja tulkitsee liikekomennot
    // Collects data from movement-sensor and recognizes movement commands
    (void)pvParameters;

    float ax, ay, az, gx, gy, gz, t;
    // Setting up the sensor. 
    if (init_ICM42670() == 0) {
        printf("ICM-42670P initialized successfully!\n");
        if (ICM42670_start_with_default_values() != 0) {
            printf("ICM-42670P could not initialize accelerometer or gyroscope");
        }

    } else {
        printf("Failed to initialize ICM-42670P.\n");
    }


    // Start collection data here. Infinite loop. 
    while (1)
    {
        if (programState == READ_SENSOR) {
            
            // Reads sensor data, detects the orientation and checks if button pressed.

                if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0) {
            
                // printf("Accel: X=%f, Y=%f, Z=%f | Gyro: X=%f, Y=%f, Z=%f| Temp: %2.2f°C\n", ax, ay, az, gx, gy, gz, t);
                // printf("Gyro: X=%f, Y=%f, Z=%f\n",gx, gy, gz);

                if (gx > 200.0f) {
                    printf("VÄLILYÖNTI\n");
                    if (last_morse_was_space) {
                        programState = MSG_READY;
                        last_morse_was_space = false;
                    } else if (strlen(morse_code) < sizeof(morse_code) - 1) {
                        strcat(morse_code, " ");
                        printf("Morse code: %s\n", morse_code);
                        last_morse_was_space = true;
                    } else {
                        printf("Morse code buffer full, cannot add space\n");
                    }
                    sleep_ms(500);
                    
                }

                else if (fabs(gz) > 200.0f) {
                    last_morse_was_space = false;
                    if (gz > 0) {
                        printf("VASEN (PISTE)\n");
                        if (strlen(morse_code) < sizeof(morse_code) - 1) {
                            strcat(morse_code, ".");
                            printf("Morse code: %s\n", morse_code);
                        } else {
                            printf("Morse code buffer full, cannot add dot\n");
                        }
                        sleep_ms(500);
                    }
                    else {
                        printf("OIKEA (VIIVA)\n");
                        if (strlen(morse_code) < sizeof(morse_code) - 1) {
                            strcat(morse_code, "-");
                            printf("Morse code: %s\n", morse_code);
                        } else {
                            printf("Morse code buffer full, cannot add dash\n");
                        }
                        sleep_ms(500);
                    }
                }
            } else {
                printf("Failed to read imu data\n");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void transmitTask(void *pvParameters)
{
    // Lähettää morsekoodin tietokoneelle
    // Sends morse-code to the computer using USB
    (void)pvParameters;
 


        while (1)
        {
            if (programState == MSG_READY) {
                printf("Morse code ready: %s\n", morse_code);
            
            morse_code[0] = '\0';
            programState = IDLE;
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
}

void receiveTask(void *pvParameters)
{
    // Vastaanottaa morseviestin tietokoneelta
    // Receives morse-code message from the computer using USB

    (void)pvParameters;
    int index = 0;

    while (1)
    {
        if (programState == NEW_MSG) {
            int ch = getchar_timeout_us(10); 
            if (ch >= 0) {
                putchar((char)ch);           // kaiku: lähetä takaisin tismalleen mitä saatiin

                if (ch == '\n') {
                    received_code[index] = '\0';
                    programState = UPDATE_OLED;
                    index = 0;
                    } else if (index <= MSG_BUFFER) {
                        received_code[index++] = (char)ch; 
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void displayTask(void *pvParameters)
{  
    // Näyttää vastaanotetun tekstin näytöllä
    // Displays the received message on the screen

    (void)pvParameters;

        init_display();
        printf("Initializing the display task\n");



    while (1)
    {
        char display_text[MSG_BUFFER] = "";
        if (programState == UPDATE_OLED)
            {
            printf("Received morse code: %s\n", received_code);
            for (int i = 0; received_code[i] != '\0'; i++) {
                if (received_code[i] == ' ') {
                    display_text[i] = '\0';
                    write_text(display_text);
                    sleep_ms(500);
                    display_text[0] = '\0';
                }
                strcat(display_text, &received_code[i]);
            }
            
            
            clear_display();
            received_code[0] = '\0';
            programState = IDLE;

        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }

    
}



int main(void)
{
    stdio_init_all();
    // Uncomment this lines if you want to wait till the serial monitor is connected
    /*while (!stdio_usb_connected()){
        sleep_ms(10);
    }*/
    init_hat_sdk();
    init_sw2();
    sleep_ms(300); // Wait some time so initialization of USB and hat is done.

    TaskHandle_t hSensorTask = NULL;
    TaskHandle_t hTransmitTask = NULL;
    TaskHandle_t hReceiveTask = NULL;
    TaskHandle_t hDisplayTask = NULL;
    TaskHandle_t hClkFxn = NULL;


    if (xTaskCreate(clkFxn, "clkFxn", DEFAULT_STACK_SIZE, NULL, 1, &hClkFxn) != pdPASS) {
        printf("Clock Task creation failed\n");
        return 0;
    };

    if (xTaskCreate(sensorTask, "Sensor", DEFAULT_STACK_SIZE, NULL, 2, &hSensorTask) != pdPASS) {
        printf("Sensor Task creation failed\n");
        return 0;
    };

    if( xTaskCreate(transmitTask, "Transmit", DEFAULT_STACK_SIZE, NULL, 2, &hTransmitTask) != pdPASS) {
        printf("Transmit Task creation failed\n");
        return 0
        ;
    };

    if (xTaskCreate(receiveTask, "Receive", DEFAULT_STACK_SIZE, NULL, 2, &hReceiveTask) != pdPASS) {
        printf("Receive Task creation failed\n");
        return 0;
    };

    if (xTaskCreate(displayTask, "Display", DEFAULT_STACK_SIZE, NULL, 2, &hDisplayTask) != pdPASS) {
        printf("Display Task creation failed\n");
        return 0;
    };



    // Start the scheduler (never returns)
    vTaskStartScheduler();

    // Never reach this line.
    return 0;
}