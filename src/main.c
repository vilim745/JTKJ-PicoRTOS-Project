
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>


#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <task.h>

#include <tusb.h>
#include "tkjhat/sdk.h"


// Default stack size for the tasks. It can be reduced to 1024 if task is not using lot of memory.
#define DEFAULT_STACK_SIZE 2048
// #define DISPLAY_WIDTH
#define MSG_BUFFER 100

#define CDC_ITF_INTERFACE 0

// Add here necessary states
enum state
{
    IDLE = 1,
    READ_SENSOR,
    MSG_READY,
    NEW_MSG,
    UPDATE_OLED
};


volatile enum state programState = IDLE;

// Array for morse code from sensor
char morse_code[MSG_BUFFER] = "";

// Array for morse code received from computer
char received_code[MSG_BUFFER] = "";

void clkFxn(void *pvParameters) {
    (void)pvParameters;

    while (1) {
        if (programState == IDLE && gpio_get(SW2_PIN) == 1) {
            printf("Button pressed, starting to read sensor data\n");
            toggle_led();
            vTaskDelay(pdMS_TO_TICKS(300));
            programState = READ_SENSOR;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
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
            // Reads sensor data, detects movement commands and checks if button pressed.

            if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0) {

                if (gpio_get(SW2_PIN) == 1) {
                    printf("Button pressed, stopping sensor data collection\n");
                    toggle_led();
                    strcat(morse_code, "  \n")
                    programState = MSG_READY;

                } else if (gx > 180.0f) {

                    if (strlen(morse_code) > 0) {
                        if (strlen(morse_code) < MSG_BUFFER - 2) {
                            printf("SPACE\n");
                            strcat(morse_code, " ");
                            printf("Morse code: %s\n", morse_code);
                        } else {
                            printf("Morse code buffer full, cannot add space\n");
                        }
                    }
                    vTaskDelay(pdMS_TO_TICKS(300));

                } else if (fabs(gz) > 200.0f) {

                    if (gz > 0) {
                        printf("DOT (LEFT)\n");
                        if (strlen(morse_code) < MSG_BUFFER - 2) {
                            strcat(morse_code, ".");
                            printf("Morse code: %s\n", morse_code);
                        } else {
                            printf("Morse code buffer full, cannot add dot\n");
                        }
                        vTaskDelay(pdMS_TO_TICKS(300));
                    } else {
                        printf("DASH (RIGHT)\n");
                        if (strlen(morse_code) < MSG_BUFFER - 2) {
                            strcat(morse_code, "-");
                            printf("Morse code: %s\n", morse_code);
                        } else {
                            printf("Morse code buffer full, cannot add dash\n");
                        }
                        vTaskDelay(pdMS_TO_TICKS(300));
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
    char buf[MSG_BUFFER]; 
    (void)pvParameters;


    while (1)
    {
        if (programState == MSG_READY) {
            printf("Morse code ready: %s\n", morse_code);
            vTaskDelay(pdMS_TO_TICKS(1000));

            if (tud_cdc_n_connected(CDC_ITF_INTERFACE)){
            // Lähetä tiedot käyttäen tud_cdc_n_write
            snprintf(buf, MSG_BUFFER,"Morse code transmitted: %s\n", morse_code);
            tud_cdc_n_write(CDC_ITF_INTERFACE, buf, strlen(buf));
            tud_cdc_n_write_flush(CDC_ITF_INTERFACE);
            
            morse_code[0] = '\0';
            programState = IDLE;
            }
        }       
    vTaskDelay(pdMS_TO_TICKS(500));
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
        if (programState == IDLE) {
            int ch = getchar_timeout_us(10); 
            if (ch >= 0) {
                putchar((char)ch);           // kaiku: lähetä takaisin tismalleen mitä saatiin

                if (ch == '\n') {
                    received_code[index] = '\0';
                    programState = UPDATE_OLED;
                    index = 0;
                    } else if (index < MSG_BUFFER - 1) {
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

    while (1)
    {
        char display_text[MSG_BUFFER] = "";

        if (programState == UPDATE_OLED)
        {
            printf("Received morse code: %s\n", received_code);

            for (int i = 0; received_code[i] != '\0'; i++)
            {
                //BUZZER STARTS
                if (received_code[i] == '.') {
                    buzzer_play_tone(440, 100);
                    vTaskDelay(pdMS_TO_TICKS(200));
                } else if (received_code[i] == '-') {
                    buzzer_play_tone(440, 300);
                    vTaskDelay(pdMS_TO_TICKS(200));
                }
                //BUZZER ENDS
                if (received_code[i] == ' ')
                {
                    write_text(display_text);
                    vTaskDelay(pdMS_TO_TICKS(500));
                    clear_display();
                    display_text[0] = '\0';
                }
                else
                {
                    if (strlen(display_text) < MSG_BUFFER - 1)
                    {
                        strncat(display_text, &received_code[i], 1);
                    }
                }
            }

            // Display the last sequence
            if (strlen(display_text) > 0)
            {
                write_text(display_text);
                vTaskDelay(pdMS_TO_TICKS(500));
            }

            clear_display();
            received_code[0] = '\0';
            programState = IDLE;
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// ---- Task running USB stack ----
static void usbTask(void *arg) {
    (void)arg;
    while (1) {
        tud_task();              // With FreeRTOS wait for events
                                 // Do not add vTaskDelay. 
    }
}


int main(void)
{
    stdio_init_all();

    while (!stdio_usb_connected()){ // wait for usb before starting the program.
        sleep_ms(10);
    }

    printf("Initializing all components\n");
    
    init_buzzer();
    init_hat_sdk();
    init_sw2();
    init_led();
    init_display();



    TaskHandle_t hSensorTask = NULL;
    TaskHandle_t hTransmitTask = NULL;
    TaskHandle_t hReceiveTask = NULL;
    TaskHandle_t hDisplayTask = NULL;
    TaskHandle_t hClkFxn = NULL;
    TaskHandle_t hUsb = NULL;


    if (xTaskCreate(usbTask, "usb", 1024, NULL, 2, &hUsb) != pdPASS) {
        printf("USB Task creation failed\n");
        return 0;
    }

    if (xTaskCreate(clkFxn, "clkFxn", DEFAULT_STACK_SIZE, NULL, 2, &hClkFxn) != pdPASS) {
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

    // Init tiny usb
    tusb_init();

    // Start the scheduler (never returns)
    vTaskStartScheduler();

    // Never reach this line.
    return 0;
}