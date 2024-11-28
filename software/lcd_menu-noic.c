#include <stdio.h>
#include <wiringPi.h>
#include <lcd.h>
#include "lcd_menu.h"
#include "deck.h"
#include "main_menu.h"

#define ROTARY_DEBOUNCE_DELAY 5
#define LONG_PRESS_DELAY 2000

// LCD Handle and Globals
int lcdHandle;  // Define lcdHandle here
static struct deck **decks;  // Array of deck pointers
static int deck_count;
bool needsUpdate = true;  // Flag to trigger screen update
MainMenuState mainMenuState = MENU_MAIN;

// Timing and State for Rotary Encoder
static unsigned long lastTurnTime = 0;
static unsigned long lastButtonPressTime = 0;
static bool longPressHandled = false;

// Initialize LCD and Rotary Encoder
void lcd_menu_init(struct deck *deck_array[], int count) {
    wiringPiSetupGpio();
    lcdHandle = lcdInit(2, 16, 4, LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7, 0, 0, 0, 0);
    if (lcdHandle < 0) {
        fprintf(stderr, "Error initializing LCD\n");
        exit(1);
    }
    pinMode(ROTARY_CLK, INPUT);
    pinMode(ROTARY_DT, INPUT);
    pinMode(ROTARY_SW, INPUT);
    pullUpDnControl(ROTARY_SW, PUD_UP);
    printf("LCD menu initialized.\n");

    decks = deck_array;  // Assign deck array and count
    deck_count = count;

    display_main_menu(decks, deck_count);
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
        if (encoded == 0b10 && lastEncoded == 0b11) {  // Clockwise
            lastEncoded = encoded;
            printf("MOVE: 1");
            return 1;  // CW
        } else if (encoded == 0b01 && lastEncoded == 0b11) {  // Counterclockwise
            lastEncoded = encoded;
            printf("MOVE: -1");
            return -1; // CCW
        }
        lastEncoded = encoded;
    }
    return 0;  // No movement
}

// Detects rotary button press: 1 for short press, 2 for long press, 0 for no press
int rotary_button_pressed() {
    bool buttonState = digitalRead(ROTARY_SW) == LOW;
    unsigned long currentPressTime = millis();

    if (buttonState) {  // Button is pressed
        if (lastButtonPressTime == 0) lastButtonPressTime = currentPressTime;

        if (currentPressTime - lastButtonPressTime >= LONG_PRESS_DELAY && !longPressHandled) {
            longPressHandled = true;
            printf("LONG-PRESS");
            return 2;  // Long press detected
        }
    } else {  // Button released
        if (lastButtonPressTime > 0 && currentPressTime - lastButtonPressTime < LONG_PRESS_DELAY) {
            lastButtonPressTime = 0;
            longPressHandled = false;
            printf("CLICK");
            return 1;  // Short press detected
        }
        lastButtonPressTime = 0;
        longPressHandled = false;
    }
    return 0;  // No press detected
}

// Poll encoder and update display if needed
void poll_rotary_encoder() {
    handle_main_menu_navigation(decks, deck_count);
    if (needsUpdate) {
        display_main_menu(decks, deck_count);
        needsUpdate = false;  // Reset update flag
    }
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
        usleep(10000); // Sleep to reduce CPU usage, adjust as needed
    }
    return NULL;
}