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

/*-------- CONSTANTS---------*/

// Default stack size for the tasks.
#define DEFAULT_STACK_SIZE 2048 

#define MSG_BUFFER 128
#define ALPHABET 26

#define CDC_ITF_DEBUG 0 // Not used here directly, only for clarification
#define CDC_ITF_DATA 1 // Data port

#define DASH '-'
#define DOT '.'
#define SPACE ' '
#define GX_THRESHOLD 180.0f // Threshold for detecting up movement (X-axis)
#define GZ_THRESHOLD 200.0f // Threshold for detecting sideways movement (Z-axis)
#define TRANSMIT_SYMBOL 'T' // used for buzzer feedback when sending a message

/*-------- GLOBAL VARIABLES ---------*/

enum state
{
    IDLE = 1,
    READ_SENSOR,
    MSG_READY,
    UPDATE_OLED
};

volatile enum state programState = IDLE;

// Tells clockTask new message has been received
bool INCOMING_MSG = false; 

// Array for morse code from sensor
char morse_code[MSG_BUFFER] = "";

// Array for morse code received from computer
char received_code[MSG_BUFFER] = "";

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


/*-------- FUNCTION PROTOTYPES ---------*/

static char morseToText(const char *symbol);
static void playBuzzer(char symbol);
static void init_all(void);
static void addSymbol(char symbol);
static void sensorMovement(float gx, float gz);
static void debug_print(char *trail, const char *message);


/* ---------- FUNCTIONS --------------*/

// Turns morse symbols to letters
static char morseToText(const char *symbol) {
    
    for (uint8_t i=0; i<ALPHABET; i++) {
        if (strcmp(symbol, morseSymbols[i]) == 0) {
            return (char) (i + 'A'); // ASCII code for character (A = 65 ... Z = 90)
        }
    }
    return '?'; // Symbol not recognized
}

// Plays buzzer sounds
static void playBuzzer(char symbol) {
    if (symbol == DOT) {
        // Play the dot tone
        buzzer_play_tone(440, 100);
        vTaskDelay(pdMS_TO_TICKS(150));

    } else if (symbol == DASH) {
        // Play the dash tone
        buzzer_play_tone(440, 300);
        vTaskDelay(pdMS_TO_TICKS(150));
        
    } else if (symbol == TRANSMIT_SYMBOL) {
        // Play the tone for sending a message
        buzzer_play_tone(460, 150);
        buzzer_play_tone(950, 150);
    }
}

// Initializes all the inits
static void init_all(void) {
    init_hat_sdk();
    init_sw2();
    init_led();
    init_buzzer();
    init_display();
    tusb_init();
    usb_serial_init();
}

// Tries to add a symbol to morse_code, if there is space. 
static void addSymbol(char symbol) { 
    uint8_t count = strlen(morse_code);

    if (count < MSG_BUFFER - 4) {
        morse_code[count] = symbol;
        morse_code[count + 1] = '\0';

        debug_print("Morse code: ", morse_code);
    } else {
        debug_print(NULL,"Morse code buffer full\n");
    }
}

// Checks the sensor movement and adds the corresponding symbol using addSymbol.
static void sensorMovement(float gx, float gz) {

    if (gx > GX_THRESHOLD) { // Up movement

        if (strlen(morse_code) > 0) {  

            debug_print(NULL, "SPACE");
            addSymbol(SPACE);    // Add space to morse
            vTaskDelay(pdMS_TO_TICKS(300));
        }

    } else if (fabs(gz) > GZ_THRESHOLD) { // sideways movement

        if (gz > 0) { // left

            debug_print(NULL, "DOT (LEFT)");
            addSymbol(DOT);

        } else { // right

            debug_print(NULL, "DASH (RIGHT)");
            addSymbol(DASH);
            
        }
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

// Function to print debug messages to USB serial if connected
static void debug_print(char *trail, const char *message) {
    if (!usb_serial_connected()) {
        return;
    }

    char msg[MSG_BUFFER];
    snprintf(msg, sizeof(msg), "%s%s\n", trail, message);

    usb_serial_print(msg);
    usb_serial_flush();
}

// Called automatically when message is received
void tud_cdc_rx_cb(uint8_t itf) {

    uint8_t buf[CFG_TUD_CDC_RX_BUFSIZE + 1];
    static size_t index = 0;

    uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf));

    if (itf == CDC_ITF_DATA) {
        for (uint8_t i = 0; i < count; i++) {
            if (buf[i] == '\n') { // checks for newline character to determine end of message
                received_code[index] = '\0';
                index = 0;
                INCOMING_MSG = true; // Tell clock task new message is received
            } else if (index < MSG_BUFFER - 1) {
                received_code[index++] = (char)buf[i]; // Writes into received_code array
            }
        }
    }
}

/* ---------- TASKS --------------*/

// Task runs when in IDLE state. Checks for button press and incoming message
void clockTask(void *pvParameters) {
    (void)pvParameters;

    while (1) {
        if (programState == IDLE) {
            if (gpio_get(SW2_PIN) == 1) {
                debug_print(NULL, "Button pressed, starting to read sensor data\n");
                toggle_led();
                vTaskDelay(pdMS_TO_TICKS(300));
                programState = READ_SENSOR;
            } else if (INCOMING_MSG) {
                programState = UPDATE_OLED;
                INCOMING_MSG = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}


// Collects data from movement-sensor and interprets commands
static void sensorTask(void *pvParameters) {
    (void)pvParameters;

    float ax, ay, az, gx, gy, gz, t;

    vTaskDelay(pdMS_TO_TICKS(100));
    for (uint8_t i = 0; i < 30 && !usb_serial_connected(); i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }


    // Setting up the sensor. 
    if (init_ICM42670() == 0) {
        debug_print(NULL, "ICM-42670P initialized successfully!\n");
        if (ICM42670_start_with_default_values() != 0) {
            debug_print(NULL, "ICM-42670P could not initialize accelerometer or gyroscope\n");
        }
    } else {
        debug_print(NULL, "Failed to initialize ICM-42670P.\n");
    }

    // Start collection data here. Infinite loop. 
    while (1)
    {
        if (programState == READ_SENSOR) {
            // Reads sensor data, detects movement commands and checks if button pressed.

            if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0) {

                if (gpio_get(SW2_PIN) == 1) {
                    // If button pressed, toggle led, end the message and change program state
                    debug_print(NULL, "\nButton pressed, stopping sensor data collection\n");
                    toggle_led();
                    strcat(morse_code, "  \n"); // end of message
                    programState = MSG_READY;

                } else {
                    sensorMovement(gx, gz);
                }
            } else {
                debug_print(NULL, "Failed to read imu data\n");
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

                debug_print("Transmitted morse code: ", morse_code);

                tud_cdc_n_write(CDC_ITF_DATA, morse_code, strlen(morse_code));
                tud_cdc_n_write_flush(CDC_ITF_DATA);
                playBuzzer(TRANSMIT_SYMBOL); // Transmit sound feedback

            }
            morse_code[0] = '\0';
            programState = IDLE;      
        } 
    vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// Displays the received message on the screen
void displayTask(void *pvParameters) {  

    (void)pvParameters;

    while (1) {
        
        if (programState == UPDATE_OLED) {
            char display_text[MSG_BUFFER] = "";

            debug_print("Received morse code: ", received_code);
        
            for (uint8_t i = 0; received_code[i] != '\0'; i++) {

                // Buzzer feedback for dot and dash
                playBuzzer(received_code[i]);
            
                if (received_code[i] == SPACE) { // Checks for space character to determine end of morse code letter
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
    TaskHandle_t hclockTask = NULL;
    TaskHandle_t hUsb = NULL;


    xTaskCreate(usbTask, "usb", DEFAULT_STACK_SIZE, NULL, 3, &hUsb);
    xTaskCreate(clockTask, "clockTask", DEFAULT_STACK_SIZE, NULL, 2, &hclockTask);
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
