#include <stdio.h>
#include <wiringPi.h>
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

#define ROTARY_DEBOUNCE_DELAY 5
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
#define BUTTON_DEBOUNCE_DELAY 250  // Debounce time in milliseconds


// Globals for I2C LCD and Rotary Encoder
int lcdHandle;  // I2C LCD Handle
static struct deck **decks;
static int deck_count;
bool needsUpdate = true;
MainMenuState mainMenuState = MENU_MAIN;

static unsigned long lastTurnTime = 0;
static unsigned long lastButtonPressTime = 0;
static bool longPressHandled = false;


// Function Prototypes
void lcd_byte(int bits, int mode);
void lcd_init();
void lcd_string(const char *message, int line);

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
    fprintf(stderr, "Printing: %s\n", string);
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

    pinMode(BUTTON_SHORT, INPUT);
    pinMode(BUTTON_LONG, INPUT);
    pullUpDnControl(BUTTON_SHORT, PUD_UP);  // Enable pull-up resistors
    pullUpDnControl(BUTTON_LONG, PUD_UP);   

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

// Detects rotary encoder movement, returns 1 for CW, -1 for CCW, 0 for no movement
int rotary_encoder_moved() {
    int MSB = digitalRead(ROTARY_CLK);
    int LSB = digitalRead(ROTARY_DT);
    int encoded = (MSB << 1) | LSB;
    unsigned long currentTurnTime = millis();

    if (currentTurnTime - lastTurnTime < ROTARY_DEBOUNCE_DELAY) return 0; // Debounce delay
    lastTurnTime = currentTurnTime;

    static int lastEncoded = 0;
    if (encoded != lastEncoded) {
        if (encoded == 0b10 && lastEncoded == 0b11) {
            lastEncoded = encoded;
            return -1;  // CW
        } else if (encoded == 0b01 && lastEncoded == 0b11) {
            lastEncoded = encoded;
            return 1; // CCW
        }
        lastEncoded = encoded;
    }
    return 0;  // No movement
}


// Detects rotary button press: 1 for short press, 2 for long press, 0 for no press
int rotary_button_pressed() {
    bool rotaryButtonState = digitalRead(ROTARY_SW) == LOW;
    bool shortButtonState = digitalRead(BUTTON_SHORT) == LOW;
    bool longButtonState = digitalRead(BUTTON_LONG) == LOW;
    unsigned long currentPressTime = millis();
    
    static unsigned long lastShortButtonTime = 0;
    static unsigned long lastLongButtonTime = 0;
    static bool shortButtonPressed = false;
    static bool longButtonPressed = false;

    // Check the rotary encoder button first (original functionality)
    if (rotaryButtonState) {
        if (lastButtonPressTime == 0) lastButtonPressTime = currentPressTime;

        if (currentPressTime - lastButtonPressTime >= LONG_PRESS_DELAY && !longPressHandled) {
            longPressHandled = true;
            return 2;  // Long press detected
        }
    } else {
        if (lastButtonPressTime > 0 && currentPressTime - lastButtonPressTime < LONG_PRESS_DELAY) {
            lastButtonPressTime = 0;
            longPressHandled = false;
            return 1;  // Short press detected
        }
        lastButtonPressTime = 0;
        longPressHandled = false;
    }
    
    // Check the dedicated short press button with debouncing
    if (shortButtonState && !shortButtonPressed && 
        (currentPressTime - lastShortButtonTime > BUTTON_DEBOUNCE_DELAY)) {
        shortButtonPressed = true;
        lastShortButtonTime = currentPressTime;
        return 1;  // Emulate a short press
    } else if (!shortButtonState) {
        shortButtonPressed = false;
    }
    
    // Check the dedicated long press button with debouncing
    if (longButtonState && !longButtonPressed && 
        (currentPressTime - lastLongButtonTime > BUTTON_DEBOUNCE_DELAY)) {
        longButtonPressed = true;
        lastLongButtonTime = currentPressTime;
        return 2;  // Emulate a long press
    } else if (!longButtonState) {
        longButtonPressed = false;
    }
    
    return 0;  // No button press detected
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
        usleep(50000); // Sleep to reduce CPU usage, adjust as needed
    }
    return NULL;
}