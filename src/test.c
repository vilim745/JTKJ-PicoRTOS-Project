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

#define DEFAULT_STACK_SIZE 2048 // Default stack size for the tasks.

#define TX_SIZE 128
#define RX_SIZE 256
#define LETTER_SIZE 8
#define DISPLAY_SIZE 50 // Letter + ' ' + code + '\0'
#define DEBUG_SIZE (RX_SIZE + 48) // longest message + trail + '\n' + '\0'

#define MSG_END ("  \n")
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

// Morse code from sensor to be transmitted
char transmit_code[TX_SIZE] = "";

// Morse code received from computer
char received_code[RX_SIZE] = "";

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
static void debug_print(const char *trail, const char *message);


/* ---------- FUNCTIONS --------------*/

// Turns a morse symbol to a letter
static char morseToText(const char *symbol) {
    
    for (uint8_t i=0; i<ALPHABET; i++) {
        if (strcmp(symbol, morseSymbols[i]) == 0) { // strcmp returns 0 if they're equal
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

// Tries to add a symbol to transmit_code, if there is space. 
static void addSymbol(char symbol) { 
    uint8_t count = strlen(transmit_code);

    if (count < TX_SIZE - 4) {
        transmit_code[count] = symbol;
        transmit_code[count + 1] = '\0';

        debug_print("Morse code: ", transmit_code);
    } else {
        debug_print(NULL,"Morse code buffer full\n");
    }
}

// Checks the sensor movement and adds the corresponding symbol using addSymbol.
static void sensorMovement(float gx, float gz) {

    if (gx > GX_THRESHOLD) { // Up movement

        if (strlen(transmit_code) > 0) {  

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

// Function to print debug messages into debug port 0 with USB serial if connected
static void debug_print(const char *trail, const char *message) {
    if (!usb_serial_connected()) {
        return;
    }

    char msg[DEBUG_SIZE];
    snprintf(msg, sizeof(msg), "%s%s\n", trail, message);

    usb_serial_print(msg);
    usb_serial_flush();
}

// Called automatically when data arrives from USB, runs inside tud_task (usbTask)
void tud_cdc_rx_cb(uint8_t itf) {

    uint8_t buf[CFG_TUD_CDC_RX_BUFSIZE + 1];
    static size_t index = 0;

    uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf));

    if (itf == CDC_ITF_DATA) {
        for (uint32_t i = 0; i < count; i++) {
            if (buf[i] == '\n') { // checks for newline character to determine end of message
                received_code[index] = '\0';
                index = 0;
                INCOMING_MSG = true; // Tell clock task new message is received
            } else if (index < RX_SIZE - 1) {
                received_code[index++] = (char)buf[i]; // Writes into received_code array
            }
        }
    }
}

/* ---------- TASKS --------------*/

// Task checks for button press and incoming message when in IDLE state
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
                INCOMING_MSG = false;
                programState = UPDATE_OLED;
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

    while (1) {
        if (programState == READ_SENSOR) {

            // Reads sensor data, detects movement commands and checks if button pressed.
            if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0) {

                if (gpio_get(SW2_PIN) == 1) {
                    // If button pressed, toggle led, end the message and change program state
                    debug_print(NULL, "\nButton pressed, stopping sensor data collection\n");
                    toggle_led();
                    strcat(transmit_code, MSG_END); // end of message newline and two spaces
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

    while (1) {
        if (programState == MSG_READY) {
            if (tud_cdc_n_connected(CDC_ITF_DATA)){
                debug_print("Transmitted morse code: ", transmit_code);

                tud_cdc_n_write(CDC_ITF_DATA, transmit_code, strlen(transmit_code));
                tud_cdc_n_write_flush(CDC_ITF_DATA);
                playBuzzer(TRANSMIT_SYMBOL); // Transmit sound feedback
            }
            transmit_code[0] = '\0';
            programState = IDLE;      
        } 
    vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// Displays the received message on the screen one translated letter at a time
void displayTask(void *pvParameters) {  
    (void)pvParameters;

    while (1) {
        if (programState == UPDATE_OLED) {
            char text[DISPLAY_SIZE];
            char morseCharacter[LETTER_SIZE] = "";
            char morseString[LETTER_SIZE * 5 + 5] = "";
            char textString[LETTER_SIZE * 5 + 5] = "";

            debug_print("Received morse code: ", received_code);
        
            for (uint8_t i = 0; received_code[i] != '\0'; i++) {

                playBuzzer(received_code[i]); // Buzzer feedback for dot and dash
            
                if (received_code[i] == SPACE) { // Checks for end of letter, for example ".- " has a space, so the part before the space gets displayed
                    if ((received_code[i-1] == SPACE) || (sizeof(morseString) + sizeof(textString) > DISPLAY_SIZE - 4)) {
                        morseString[0] = '\0';
                        textString[0] = '\0';
                    }
                    if (morseCharacter[0] != '\0') { // if display_text isn't empty?
                        char textCharStr[2] = "";
                        snprintf(textCharStr,sizeof(textCharStr), "%c ", morseToText(morseCharacter))
                        strcat(textString, textCharStr);
                        snprintf(text, sizeof(text), "%s\n%s", morseString, textString); // Builds for example "A .-" from the letter and its code
                        write_text_xy(0, 0, text); // Shows the above buffer text on the display, int values are for the pixel positioning. Scale for write_text_xy is changed to '2' inside sdk.c
                        vTaskDelay(pdMS_TO_TICKS(100));
                        clear_display();
                        morseCharacter[0] = '\0'; // Resets to an empty string
                    }
                } else if (strlen(morseCharacter) < sizeof(morseCharacter) - 1) { // Not a space and display_text has room
                        strncat(morseCharacter, &received_code[i], 1); // Adds the morse code character to display_text 
                        strncat(morseString, morseCharacter, 1);
                        strcat(morseString, " ");
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
