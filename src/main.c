/*
Authors: Vili Mannermaa 2507250, Jasu Känsälä 2410201, Elias Mela 2507227
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
#define ALPHABET 26

#define CDC_ITF_DEBUG 0 // Not used here directly, only for clarification
#define CDC_ITF_DATA 1 // Data port

enum state
{
    IDLE = 1,
    READ_SENSOR,
    MSG_READY,
    UPDATE_OLED
};


volatile enum state programState = IDLE;

bool INCOMING_MSG = 0;

// Array for morse code from sensor
char morse_code[MSG_BUFFER] = "";

// Array for morse code received from computer
char received_code[MSG_BUFFER] = "";



char morseToText(const char *symbol);
static void playBuzzer(char symbol);
static void init_all(void);
static void addSymbol(char symbol);
static void sensorMovement(float gx, float gz);
static void debug_print(const char *msg);


// Morse symbols for letters
const char *morseSymbols[ALPHABET] = {
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

// Turns morse symbols to letters
char morseToText(const char *symbol) {

    for (int i=0; i<ALPHABET; i++) {
        if (strcmp(symbol, morseSymbols[i]) == 0) {
            return (char) (i + 'A'); // ASCII code for character (A = 65 ... Z = 90)
        }
    }
    return '?'; // Symbol not recognized
}

static void playBuzzer(char symbol) {
    if (symbol == '.') {
        // Play the dot tone
        buzzer_play_tone(440, 100);
        vTaskDelay(pdMS_TO_TICKS(150));

    } else if (symbol == '-') {
        // Play the dash tone
        buzzer_play_tone(440, 300);
        vTaskDelay(pdMS_TO_TICKS(150));

    } else if (symbol == 'T') {
        // Play the tone for sending a message
        buzzer_play_tone(460, 150);
        buzzer_play_tone(950, 150);
    }
}

static void init_all(void) {
    init_hat_sdk();
    init_sw2();
    init_led();
    init_buzzer();
    init_display();
    tusb_init();
    usb_serial_init();
}

static void addSymbol(char symbol) { 
    int count = strlen(morse_code);

    if (count < MSG_BUFFER - 4) {
        morse_code[count] = symbol;
        morse_code[count + 1] = '\0';

        char msg[MSG_BUFFER];
        snprintf(msg, sizeof(msg), "Morse code: %s \n", morse_code);
        debug_print(msg);
    } else {
        debug_print("Morse code buffer full \n");
    }
}

static void sensorMovement(float gx, float gz) {

    if (gx > 180.0f) { // Up movement

        if (strlen(morse_code) > 0) {  
            debug_print("SPACE \n");
            addSymbol(' ');    // Add space to morse
            vTaskDelay(pdMS_TO_TICKS(300));
        }

    } else if (fabs(gz) > 200.0f) { // sideways movement

        if (gz > 0) { // left
            debug_print("DOT (LEFT)\n");
            addSymbol('.');

        } else { // right
            debug_print("DASH (RIGHT)\n");
            addSymbol('-');
            
        }
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

// Function to print debug messages to USB serial if connected
static void debug_print(const char *msg) {
    if (!usb_serial_connected()) {
        return;
    }

    usb_serial_print(msg);
    usb_serial_flush();
}

// Clock function to check button press and change program state
void clkFxn(void *pvParameters) {
    
    (void)pvParameters;

    while (1) {
        if (programState == IDLE) {
            if (gpio_get(SW2_PIN) == 1) {
                debug_print("Button pressed, starting to read sensor data\n");
                toggle_led();
                vTaskDelay(pdMS_TO_TICKS(300));
                programState = READ_SENSOR;
            } else if (INCOMING_MSG) {
                programState = UPDATE_OLED;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}


// Collects data from movement-sensor and recognizes movement commands
void sensorTask(void *pvParameters)
{
    (void)pvParameters;

    float ax, ay, az, gx, gy, gz, t;
    

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
                    // If button pressed, toggle led, end the message and change program state
                    debug_print("Button pressed, stopping sensor data collection\n");
                    toggle_led();
                    strcat(morse_code, "  \n"); // end of message
                    programState = MSG_READY;

                } else {
                    sensorMovement(gx, gz);
                }
            } else {
                debug_print("Failed to read imu data\n");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}


// Sends morse-code to the computer using USB
// Stdio usb support needs to be disabled and usb-serial-debug linked and added as subdirectory
void transmitTask(void *pvParameters) {

    (void)pvParameters;

    char buf[MSG_BUFFER] = ""; 

    while (1)
    {
        if (programState == MSG_READY) {
            if (tud_cdc_n_connected(CDC_ITF_DATA)){
                char msg[MSG_BUFFER];
                snprintf(msg, sizeof(msg), "Transmitting morse code: %s", morse_code);
                debug_print(msg);

                tud_cdc_n_write(CDC_ITF_DATA, morse_code, strlen(morse_code));
                tud_cdc_n_write_flush(CDC_ITF_DATA);
                playBuzzer(TRANSMIT_SOUND); // Transmit sound feedback

            }
            morse_code[0] = '\0';
            programState = IDLE;      
        } 
    vTaskDelay(pdMS_TO_TICKS(500));
    }
}


// Receive data using USB, invoked on its own
void tud_cdc_rx_cb(uint8_t itf) {

    uint8_t buf[CFG_TUD_CDC_RX_BUFSIZE + 1];
    static size_t index = 0;

    uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf));

    if (itf == CDC_ITF_DATA) {
        for (int i = 0; i < count; i++) {
            if (buf[i] == '\n') { // checks for newline character to determine end of message
                received_code[index] = '\0';
                index = 0;

                INCOMING_MSG = 1;
            } else if (index < MSG_BUFFER - 1) {
                received_code[index++] = (char)buf[i]; // Writes into received_code array
            }
        }
    }
}

// Displays the received message on the screen
void displayTask(void *pvParameters) {  

    (void)pvParameters;

    while (1) {
        
        if (programState == UPDATE_OLED) {
            char display_text[MSG_BUFFER] = "";

            char msg[MSG_BUFFER];
            snprintf(msg, sizeof(msg), "Received morse code: %s\n", received_code);
            debug_print(msg);
        
            for (int i = 0; received_code[i] != '\0'; i++) {

                // Buzzer feedback for dot and dash
                playBuzzer(received_code[i]);
            
                if (received_code[i] == ' ') { // Checks for space character to determine end of morse code letter
                    if (display_text[0] != '\0') {
                        char text[MSG_BUFFER];   
                        snprintf(text, sizeof(text), "%c %s", morseToText(display_text), display_text);
                        write_text_xy(20, 20, text);
                    }
                    vTaskDelay(pdMS_TO_TICKS(100));
                    clear_display();
                    display_text[0] = '\0';
                } else {
                    if (strlen(display_text) < MSG_BUFFER - 1){
                        strncat(display_text, &received_code[i], 1); // Adds the morse code character to display_text
                    }
                }
            }

            clear_display();
            received_code[0] = '\0';
            programState = IDLE;
        }

        vTaskDelay(pdMS_TO_TICKS(150));
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


int main(void) {

    TaskHandle_t hSensorTask = NULL;
    TaskHandle_t hTransmitTask = NULL;
    TaskHandle_t hDisplayTask = NULL;
    TaskHandle_t hClkFxn = NULL;
    TaskHandle_t hUsb = NULL;


    xTaskCreate(usbTask, "usb", DEFAULT_STACK_SIZE, NULL, 3, &hUsb);
    xTaskCreate(clkFxn, "clkFxn", DEFAULT_STACK_SIZE, NULL, 2, &hClkFxn);
    xTaskCreate(sensorTask, "Sensor", DEFAULT_STACK_SIZE, NULL, 2, &hSensorTask);
    xTaskCreate(transmitTask, "Transmit", DEFAULT_STACK_SIZE, NULL, 2, &hTransmitTask);
    xTaskCreate(displayTask, "Display", DEFAULT_STACK_SIZE, NULL, 2, &hDisplayTask);


    #if (configNUMBER_OF_CORES > 1)
        vTaskCoreAffinitySet(hUsb, 1u << 0);
    #endif

    // Init all peripherals and USB serial debug
    init_all();
    // Start the scheduler (never returns)
    vTaskStartScheduler();

    // Never reach this line.
    return 0;
}
