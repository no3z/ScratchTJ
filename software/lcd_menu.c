#include <stdio.h>
#include "lcd_menu.h"
#include "deck.h"
#include "main_menu.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "sc_input.h"
#include "cues.h"

#define ROTARY_DEBOUNCE_DELAY 5

/* Globals */
static struct deck **decks;
static int deck_count;
bool needsUpdate = true;
MainMenuState mainMenuState = MENU_HOME;

static unsigned long lastButtonPressTime = 0;

/* Auto-return and home screen refresh timers */
static unsigned long lastActivityTime = 0;
static unsigned long lastHomeRedrawTime = 0;
#define HOME_REDRAW_INTERVAL_MS  100
#define AUTO_RETURN_TIMEOUT_MS  10000

static unsigned long menu_millis(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

void lcd_menu_init(struct deck *deck_array[], int count) {
    if (gpio_direct_init() < 0) {
        fprintf(stderr, "Couldn't initialize GPIO\n");
        exit(1);
    }

    /* Initialize display */
    if (oled_init() < 0) {
        fprintf(stderr, "Error initializing display\n");
        exit(1);
    }

    /* Setup encoder pins */
    gpio_set_input(ROTARY_CLK);
    gpio_set_input(ROTARY_DT);
    gpio_set_input(ROTARY_SW);
    gpio_set_input(ROTARY_KO);
    gpio_set_pullup(ROTARY_CLK);
    gpio_set_pullup(ROTARY_DT);
    gpio_set_pullup(ROTARY_SW);
    gpio_set_pullup(ROTARY_KO);

    printf("TFT menu initialized.\n");

    decks = deck_array;
    deck_count = count;

    lastActivityTime = menu_millis();
    display_main_menu(decks, deck_count);
}

/* Poll encoder and update display if needed */
void poll_rotary_encoder() {
    unsigned long now = menu_millis();

    /* Update cue bar overlay with deck 2 cue data */
    if (deck_count >= 2) {
        double cue_positions[4];
        for (int i = 0; i < 4; i++)
            cue_positions[i] = cues_get(&decks[1]->cues, i);
        oled_set_cue_overlay(cue_display_states, cue_positions);
    }

    handle_main_menu_navigation(decks, deck_count);

    /* Auto-return to home after inactivity */
    if (mainMenuState != MENU_HOME &&
        (now - lastActivityTime) >= AUTO_RETURN_TIMEOUT_MS) {
        mainMenuState = MENU_HOME;
        needsUpdate = true;
    }

    /* Periodic redraw for home screen (live data at ~10fps) */
    if (mainMenuState == MENU_HOME &&
        (now - lastHomeRedrawTime) >= HOME_REDRAW_INTERVAL_MS) {
        lastHomeRedrawTime = now;
        needsUpdate = true;
    }

    if (needsUpdate) {
        display_main_menu(decks, deck_count);
        needsUpdate = false;
    }
}

/* Detects rotary encoder movement, returns 1 for CW, -1 for CCW, 0 for no movement.
 * Uses a full quadrature state table to track all 4 transitions per detent.
 * Accumulates sub-steps and only reports a move when a full detent (4 edges)
 * is completed, providing reliable debouncing. */
int rotary_encoder_moved() {
    static const int8_t transition_table[4][4] = {
        /*  00   01   10   11  <- current */
        {   0,  -1,   1,   0 }, /* last = 00 */
        {   1,   0,   0,  -1 }, /* last = 01 */
        {  -1,   0,   0,   1 }, /* last = 10 */
        {   0,   1,  -1,   0 }, /* last = 11 */
    };

    static int lastState = -1;
    static int accumulator = 0;

    int clk = gpio_read(ROTARY_CLK);
    int dt = gpio_read(ROTARY_DT);
    int currentState = (clk << 1) | dt;

    if (lastState == -1) {
        lastState = currentState;
        return 0;
    }

    if (currentState == lastState) return 0;

    int direction = transition_table[lastState][currentState];
    lastState = currentState;

    if (direction == 0) return 0;

    accumulator += direction;

    if (accumulator >= 4) {
        accumulator = 0;
        lastActivityTime = menu_millis();
        return 1;  /* CW */
    } else if (accumulator <= -4) {
        accumulator = 0;
        lastActivityTime = menu_millis();
        return -1; /* CCW */
    }

    return 0;
}

/* Detects rotary button click: 1 on release, 0 otherwise (back button) */
int rotary_button_pressed() {
    bool buttonState = gpio_read(ROTARY_SW) == 0; /* active low */

    if (buttonState) {
        if (lastButtonPressTime == 0) lastButtonPressTime = gpio_millis();
    } else {
        if (lastButtonPressTime > 0) {
            lastButtonPressTime = 0;
            lastActivityTime = menu_millis();
            return 1;  /* click = back */
        }
    }
    return 0;
}

/* KB0 button (GPIO17): 1 on release (select/enter), 0 otherwise */
int kb0_button_pressed() {
    static unsigned long kb0PressTime = 0;

    bool pressed = gpio_read(ROTARY_KO) == 0;  /* active low */

    if (pressed) {
        if (kb0PressTime == 0) kb0PressTime = gpio_millis();
    } else {
        if (kb0PressTime > 0) {
            kb0PressTime = 0;
            lastActivityTime = menu_millis();
            return 1;  /* click = select */
        }
    }
    return 0;
}

void trigger_io_event(unsigned char action, unsigned char deckNo, unsigned char param) {
    struct mapping temp_map = {0};
    temp_map.Action = action;
    temp_map.DeckNo = deckNo;
    temp_map.Type = MAP_IO;
    temp_map.Param = param;
    IOevent(&temp_map, NULL);
}

void trigger_io_event_no_param(unsigned char action, unsigned char deckNo) {
    unsigned char defaultVolume = 64;
    trigger_io_event(action, deckNo, defaultVolume);
}

void* rotary_encoder_thread(void* arg) {
    while (true) {
        poll_rotary_encoder();
        usleep(5000); /* Poll every 5ms (200Hz) */
    }
    return NULL;
}
