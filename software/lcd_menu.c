#include <stdio.h>
#include <wiringPi.h>
#include <pthread.h>
#include "lcd_menu.h"
#include "deck.h"
#include "main_menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>

#define LONG_PRESS_DELAY 2000

// I2C LCD Address (adjust if needed, use `i2cdetect` to find address)
#define I2C_ADDR 0x27

// LCD Commands
#define LCD_CMD 0
#define LCD_CHR 1
#define LCD_BACKLIGHT 0x08
#define ENABLE 0x04
#define LCD_LINE_1 0x80  // First line
#define LCD_LINE_2 0xC0  // Second line

#define BUTTON_SHORT 17  // GPIO pin for short press button
#define BUTTON_LONG 27   // GPIO pin for long press button
#define BUTTON_DEBOUNCE_MS 350  // Debounce time in milliseconds (covers release bounce)

// Globals for I2C LCD and Rotary Encoder
int lcdHandle;  // I2C LCD Handle
static struct deck **decks;
static int deck_count;
bool needsUpdate = true;
MainMenuState mainMenuState = MENU_MAIN;

static unsigned long lastButtonPressTime = 0;
static bool longPressHandled = false;

// Interrupt-driven button state
static volatile bool btn_select_pending = false;
static volatile unsigned long btn_select_time = 0;
static volatile bool btn_back_pending = false;
static volatile unsigned long btn_back_time = 0;

// ISR callbacks: don't check digitalRead — by the time the wiringPi ISR thread
// runs, the pin may have bounced back HIGH. Trust the falling edge + debounce timer.
void button_select_isr(void) {
    unsigned long now = millis();
    if (now - btn_select_time > BUTTON_DEBOUNCE_MS) {
        btn_select_time = now;
        btn_select_pending = true;
    }
}

void button_back_isr(void) {
    unsigned long now = millis();
    if (now - btn_back_time > BUTTON_DEBOUNCE_MS) {
        btn_back_time = now;
        btn_back_pending = true;
    }
}


// Function Prototypes
void lcd_byte(int bits, int mode);
void lcd_init();
void lcd_string(const char *message, int line);
void encoder_isr(void);

void lcd_byte(int bits, int mode) {
    char high_bits = mode | (bits & 0xF0) | LCD_BACKLIGHT;
    char low_bits = mode | ((bits << 4) & 0xF0) | LCD_BACKLIGHT;

    // Write high nibble
    if (write(lcdHandle, &high_bits, 1) != 1) {
        fprintf(stderr, "Error writing high nibble\n");
        exit(1);
    }
    lcd_toggle_enable(high_bits);

    // Write low nibble
    if (write(lcdHandle, &low_bits, 1) != 1) {
        fprintf(stderr, "Error writing low nibble\n");
        exit(1);
    }
    lcd_toggle_enable(low_bits);
}

void lcd_toggle_enable(char bits) {
    usleep(500); // Delay to ensure proper timing
    char enable_bits = bits | ENABLE;
    if (write(lcdHandle, &enable_bits, 1) != 1) {
        fprintf(stderr, "Error toggling ENABLE\n");
        exit(1);
    }
    usleep(500); // Enable pulse width
    enable_bits = bits & ~ENABLE;
    if (write(lcdHandle, &enable_bits, 1) != 1) {
        fprintf(stderr, "Error resetting ENABLE\n");
        exit(1);
    }
    usleep(500); // Delay after toggling
}

void lcd_init() {
    lcd_byte(0x33, LCD_CMD); // Initialize
    lcd_byte(0x32, LCD_CMD); // Set to 4-bit mode
    lcd_byte(0x06, LCD_CMD); // Cursor move direction
    lcd_byte(0x0C, LCD_CMD); // Turn cursor off
    lcd_byte(0x28, LCD_CMD); // 2-line display, 5x8 dots
    lcd_byte(0x01, LCD_CMD); // Clear display
    usleep(2000);            // Delay for clear command
}
// Write a string to the LCD
void lcd_string(const char *message, int line) {
    lcd_byte(line, LCD_CMD);
    for (int i = 0; message[i] != '\0'; i++) {
        lcd_byte(message[i], LCD_CHR);
    }
}


void lcdClear(int handle) {
    lcd_byte(0x01, LCD_CMD); // Clear display
    usleep(2000);            // Delay for clear command
}

void lcdPuts(int handle, const char *string) {    
    //fprintf(stderr, "Printing: %s\n", string);
    for (int i = 0; string[i] != '\0'; i++) {
        // fprintf(stderr, "Char: %c (%02X)\n", string[i], string[i]);
        lcd_byte(string[i], LCD_CHR);
    }
}

void lcdPosition(int handle, int col, int row) {
    int address;

    // Calculate the address based on row and column
    switch (row) {
        case 0: address = 0x80 + col; break; // Line 1
        case 1: address = 0xC0 + col; break; // Line 2
        // case 2: address = 0x94 + col; break; // Line 3 (if applicable)
        // case 3: address = 0xD4 + col; break; // Line 4 (if applicable)
        default:
            fprintf(stderr, "Invalid row: %d\n", row);
            return;
    }

    // Send the calculated address as a command to the LCD
    lcd_byte(address, LCD_CMD);
}

void lcdPrintf(int handle, const char *format, ...) {
    char buffer[64];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    lcdPuts(0,buffer);
}

void lcd_set_cursor(int line, int position) {
    int address = (line == 1 ? LCD_LINE_1 : LCD_LINE_2) + position;
    lcd_byte(address, LCD_CMD);
}


void lcd_menu_init(struct deck *deck_array[], int count) {
    if (wiringPiSetupGpio() == -1) {
        fprintf(stderr, "Couldn't initialize GPIO\n");
        exit(1);
    }

    lcdHandle = open("/dev/i2c-1", O_RDWR);
    if (lcdHandle < 0) {
        fprintf(stderr, "Error opening I2C device\n");
        exit(1);
    }                 

    // Set I2C slave address
    if (ioctl(lcdHandle, I2C_SLAVE, I2C_ADDR) < 0) {
        fprintf(stderr, "Error setting I2C address\n");
        exit(1);
    }

    pinMode(ROTARY_CLK, INPUT);
    pinMode(ROTARY_DT, INPUT);
    pinMode(ROTARY_SW, INPUT);
    pullUpDnControl(ROTARY_SW, PUD_UP);

    // Register interrupts on both encoder pins (fires on every edge)
    if (wiringPiISR(ROTARY_CLK, INT_EDGE_BOTH, &encoder_isr) < 0) {
        fprintf(stderr, "Unable to setup ISR for ROTARY_CLK\n");
    }
    if (wiringPiISR(ROTARY_DT, INT_EDGE_BOTH, &encoder_isr) < 0) {
        fprintf(stderr, "Unable to setup ISR for ROTARY_DT\n");
    }

    pinMode(BUTTON_SHORT, INPUT);
    pinMode(BUTTON_LONG, INPUT);
    pullUpDnControl(BUTTON_SHORT, PUD_UP);
    pullUpDnControl(BUTTON_LONG, PUD_UP);

    // Register interrupts for dedicated buttons (fires on press = falling edge)
    if (wiringPiISR(BUTTON_LONG, INT_EDGE_FALLING, &button_select_isr) < 0) {
        fprintf(stderr, "Unable to setup ISR for select button\n");
    }
    if (wiringPiISR(BUTTON_SHORT, INT_EDGE_FALLING, &button_back_isr) < 0) {
        fprintf(stderr, "Unable to setup ISR for back button\n");
    }

    lcd_init();
    printf("LCD menu initialized.\n");

    decks = deck_array;
    deck_count = count;

    display_main_menu(decks, deck_count);
}


// Poll encoder and update display if needed
void poll_rotary_encoder() {
    handle_main_menu_navigation(decks, deck_count);
    if (needsUpdate) {
        display_main_menu(decks, deck_count);
        needsUpdate = false;
    }
}

// Interrupt-driven rotary encoder using detent-to-detent state machine.
// A step is only reported when the encoder completes a full detent cycle:
//   CW:  11 -> 01 -> 00 -> 10 -> 11
//   CCW: 11 -> 10 -> 00 -> 01 -> 11
// Bounce near a detent can't trigger false steps because we require passing
// through state 00 (both pins LOW) before accepting a new detent arrival.
static pthread_mutex_t enc_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile int enc_last_state = -1;
static volatile int enc_steps = 0;
static volatile int enc_phase = 0;  // 0=at detent, 1=left detent, 2=passed through middle
static volatile int enc_dir = 0;

// Called by wiringPi on any CLK or DT edge change
void encoder_isr(void) {
    pthread_mutex_lock(&enc_mutex);

    int clk = digitalRead(ROTARY_CLK);
    int dt = digitalRead(ROTARY_DT);
    int state = (clk << 1) | dt;

    if (state == enc_last_state) {
        pthread_mutex_unlock(&enc_mutex);
        return;
    }

    int prev = enc_last_state;
    enc_last_state = state;

    // Wait for first detent (11) before tracking
    if (prev < 0) {
        pthread_mutex_unlock(&enc_mutex);
        return;
    }

    if (state == 3) {
        // Arrived at detent — report step only if we passed through middle
        if (enc_phase == 2) {
            enc_steps += enc_dir;
        }
        enc_phase = 0;
        enc_dir = 0;
    } else if (enc_phase == 0 && prev == 3) {
        // Just left detent — record initial direction
        if (state == 1) enc_dir = 1;        // 11->01 = CW
        else if (state == 2) enc_dir = -1;  // 11->10 = CCW
        enc_phase = 1;
    } else if (enc_phase == 1 && state == 0) {
        // Reached middle (00) — movement is committed
        enc_phase = 2;
    }

    pthread_mutex_unlock(&enc_mutex);
}

// Returns encoder direction since last call: -1, 0, or 1.
int rotary_encoder_moved() {
    pthread_mutex_lock(&enc_mutex);
    int steps = enc_steps;
    enc_steps = 0;
    pthread_mutex_unlock(&enc_mutex);
    if (steps > 0) return 1;
    if (steps < 0) return -1;
    return 0;
}

// Detects button press: 1 for select, 2 for back, 0 for no press
// Only checks the dedicated GPIO buttons (not the rotary encoder button, which is now the home button).
int rotary_button_pressed() {
    // Check interrupt-captured button presses (never missed, even during LCD writes)
    if (btn_back_pending) {
        btn_back_pending = false;
        return 2;  // Back
    }
    if (btn_select_pending) {
        btn_select_pending = false;
        return 1;  // Select
    }

    return 0;
}

// Detects rotary encoder button press as a "home" button.
// Returns true once per press (on release), with debouncing.
bool home_button_pressed() {
    bool state = digitalRead(ROTARY_SW) == LOW;
    unsigned long now = millis();

    if (state) {
        if (lastButtonPressTime == 0) lastButtonPressTime = now;
    } else {
        if (lastButtonPressTime > 0) {
            lastButtonPressTime = 0;
            longPressHandled = false;
            return true;
        }
    }
    return false;
}
void trigger_io_event(unsigned char action, unsigned char deckNo, unsigned char param) {
    struct mapping temp_map = {0}; // Temporary mapping structure
    temp_map.Action = action;
    temp_map.DeckNo = deckNo;
    temp_map.Type = MAP_IO; // Assuming it's an I/O event type, not MIDI
    temp_map.Param = param; // Set the volume as a parameter

    IOevent(&temp_map, NULL); // Call IOevent with generated mapping and NULL MidiBuffer
}

void trigger_io_event_no_param(unsigned char action, unsigned char deckNo){
    unsigned char defaultVolume = 64; // Example default value
    trigger_io_event(action, deckNo, defaultVolume);
}
void* rotary_encoder_thread(void* arg) {
    while (true) {
        poll_rotary_encoder();
        usleep(5000); // Poll every 5ms (200Hz) - fast enough to catch all encoder transitions
    }
    return NULL;
}