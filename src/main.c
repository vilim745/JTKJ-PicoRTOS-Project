/*
Authors: ...
*/

#include <stdio.h>
#include <string.h>

#include <stdlib.h>
#include <math.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <task.h>
#include <tusb.h>

#include "tkjhat/sdk.h"
#include "usbSerialDebug/helper.h"


// Default stack size for the tasks. It can be reduced to 1024 if task is not using lot of memory.
#define DEFAULT_STACK_SIZE 2048
#define MSG_BUFFER 128

#define CDC_ITF_DEBUG 0
#define CDC_ITF_DATA 1



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

// Morse symbols for letters
const char *morseSymbols[26] = {
    ".-",       // A
    "-...",     // B
    "-.-.",     // C
    "-..",      // D
    ".",        // E
    "..-.",     // F
    "--.",      // G
    "....",     // H
    "..",       // I
    ".---",     // J
    "-.-",      // K
    ".-..",     // L
    "--",       // M
    "-.",       // N
    "---",      // O
    ".--.",     // P
    "--.-",     // Q
    ".-.",      // R
    "...",      // S
    "-",        // T
    "..-",      // U
    "...-",     // V
    ".--",      // W
    "-..-",     // X
    "-.--",     // Y
    "--.."      // Z
};

 // Prototypes
static void debug_print(const char *msg);
// Turns morse symbol to letter
char morseToText(const char *symbol);

char morseToText(const char *symbol) {
    for (int i=0; i<26; i++) {
        if (strcmp(symbol, morseSymbols[i]) == 0) {
            return (char) (i + 65); // ASCII code for character (A = 65 ... Z = 90)
        }
    }
    return '?'; // Symbol not recognized
}

// Function to print debug messages to USB serial if connected
static void debug_print(const char *msg) {
    if (!usb_serial_connected()) {
        return;
    }

    usb_serial_print(msg);
    usb_serial_flush();
}

void clkFxn(void *pvParameters) {
    // Clock function to check button press and change program state
    (void)pvParameters;

    while (1) {
        if (programState == IDLE && gpio_get(SW2_PIN) == 1) {
            debug_print("Button pressed, starting to read sensor data\n");
            toggle_led();
            vTaskDelay(pdMS_TO_TICKS(300));
            programState = READ_SENSOR;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void sensorTask(void *pvParameters)
{
    // Collects data from movement-sensor and recognizes movement commands
    (void)pvParameters;
    float ax, ay, az, gx, gy, gz, t;

    init_hat_sdk();
    init_sw2();
    init_led();
    init_buzzer();
    init_display();

    vTaskDelay(pdMS_TO_TICKS(100));
    for (int i = 0; i < 30 && !usb_serial_connected(); i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }


    // Setting up the sensor. 
    if (init_ICM42670() == 0) {
        debug_print("ICM-42670P initialized successfully!\n");
        if (ICM42670_start_with_default_values() != 0) {
            debug_print("ICM-42670P could not initialize accelerometer or gyroscope\n");
        }
    } else {
        debug_print("Failed to initialize ICM-42670P.\n");
    }

    // Start collection data here. Infinite loop. 
    while (1)
    {
        if (programState == READ_SENSOR) {
            // Reads sensor data, detects movement commands and checks if button pressed.

            if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0) {

                if (gpio_get(SW2_PIN) == 1) {
                    debug_print("Button pressed, stopping sensor data collection\n");
                    toggle_led();
                    strcat(morse_code, "  \n"); // end of message
                    programState = MSG_READY;

                } else if (gx > 180.0f) {

                    if (strlen(morse_code) > 0) {
                        if (strlen(morse_code) < MSG_BUFFER - 4) {
                            debug_print("SPACE\n");
                            strcat(morse_code, " ");
                            char msg[MSG_BUFFER];
                            snprintf(msg, sizeof(msg), "Morse code: %s\n", morse_code);
                            debug_print(msg);
                        } else {
                            debug_print("Morse code buffer full, cannot add space\n");
                        }
                    }
                    vTaskDelay(pdMS_TO_TICKS(300));

                } else if (fabs(gz) > 200.0f) {

                    if (gz > 0) {
                        debug_print("DOT (LEFT)\n");
                        if (strlen(morse_code) < MSG_BUFFER - 4) {
                            strcat(morse_code, ".");
                            char msg[MSG_BUFFER];
                            snprintf(msg, sizeof(msg), "Morse code: %s\n", morse_code);
                            debug_print(msg);
                        } else {
                            debug_print("Morse code buffer full, cannot add dot\n");
                        }
                        vTaskDelay(pdMS_TO_TICKS(300));
                    } else {
                        debug_print("DASH (RIGHT)\n");
                        if (strlen(morse_code) < MSG_BUFFER - 4) {
                            strcat(morse_code, "-");
                            char msg[MSG_BUFFER];
                            snprintf(msg, sizeof(msg), "Morse code: %s\n", morse_code);
                            debug_print(msg);
                        } else {
                            debug_print("Morse code buffer full, cannot add dash\n");
                        }
                        vTaskDelay(pdMS_TO_TICKS(300));
                    }
                }
            } else {
                debug_print("Failed to read imu data\n");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void transmitTask(void *pvParameters)
{
    // Sends morse-code to the computer using USB
    char buf[MSG_BUFFER] = ""; 
    (void)pvParameters;


    while (1)
    {
        if (programState == MSG_READY) {
            if (tud_cdc_n_connected(CDC_ITF_DATA)){
                char msg[MSG_BUFFER];
                snprintf(msg, sizeof(msg), "Transmitting morse code: %s", morse_code);
                debug_print(msg);
                // Lähetä tiedot käyttäen tud_cdc_n_write
                tud_cdc_n_write(CDC_ITF_DATA, morse_code, strlen(morse_code));
                tud_cdc_n_write_flush(CDC_ITF_DATA);
                buzzer_play_tone(466, 150);
                buzzer_play_tone(932, 150);

                vTaskDelay(pdMS_TO_TICKS(1000)); // ???
                morse_code[0] = '\0';
            }

            programState = IDLE;      
        } 
    vTaskDelay(pdMS_TO_TICKS(500));
    }
}


void tud_cdc_rx_cb(uint8_t itf)
{
    uint8_t buf[CFG_TUD_CDC_RX_BUFSIZE + 1];
    static size_t index = 0;

    uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf));

    if (itf == CDC_ITF_DATA) {
        for (int i = 0; i < count; i++) {
            if (programState != IDLE) {
                continue;
            }

            if (buf[i] == '\n') {
                received_code[index] = '\0';
                index = 0;

                programState = UPDATE_OLED;
            } else if (index < MSG_BUFFER - 1) {
                received_code[index++] = (char)buf[i];
            }
        }
    }
}

void displayTask(void *pvParameters)
{  
    
    // Displays the received message on the screen
    (void)pvParameters;

    while (1) {
        
        if (programState == UPDATE_OLED) {
            char display_text[MSG_BUFFER] = "";
            char msg[MSG_BUFFER];
            snprintf(msg, sizeof(msg), "Received morse code: %s\n", received_code);
            debug_print(msg);
        
            for (int i = 0; received_code[i] != '\0'; i++) {
                if (received_code[i] == '.') {
                    buzzer_play_tone(440, 100);
                    vTaskDelay(pdMS_TO_TICKS(100));
                } else if (received_code[i] == '-') {
                    buzzer_play_tone(440, 300);
                    vTaskDelay(pdMS_TO_TICKS(100));
                }

                if (received_code[i] == ' ') {
                    if (display_text[0] != '\0') {
                        char text[MSG_BUFFER];
                        snprintf(text, sizeof(text), "%c %s", morseToText(display_text), display_text);
                        write_text_xy(20, 20, text);
                    }
                    vTaskDelay(pdMS_TO_TICKS(500));
                    clear_display();
                    display_text[0] = '\0';
                } else {
                    if (strlen(display_text) < MSG_BUFFER - 1){
                        strncat(display_text, &received_code[i], 1);
                    }
                }
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

    TaskHandle_t hSensorTask = NULL;
    TaskHandle_t hTransmitTask = NULL;
    TaskHandle_t hDisplayTask = NULL;
    TaskHandle_t hClkFxn = NULL;
    TaskHandle_t hUsb = NULL;


    xTaskCreate(usbTask, "usb", 1024, NULL, 3, &hUsb);
    xTaskCreate(clkFxn, "clkFxn", DEFAULT_STACK_SIZE, NULL, 2, &hClkFxn);
    xTaskCreate(sensorTask, "Sensor", DEFAULT_STACK_SIZE, NULL, 2, &hSensorTask);
    xTaskCreate(transmitTask, "Transmit", DEFAULT_STACK_SIZE, NULL, 2, &hTransmitTask);
    xTaskCreate(displayTask, "Display", DEFAULT_STACK_SIZE, NULL, 2, &hDisplayTask);


    #if (configNUMBER_OF_CORES > 1)
        vTaskCoreAffinitySet(hUsb, 1u << 0);
    #endif

    // Init tiny usb
    tusb_init();

    usb_serial_init();
    // Start the scheduler (never returns)
    vTaskStartScheduler();

    // Never reach this line.
    return 0;
}