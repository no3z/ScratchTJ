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
#include "settings.h"
#include "xwax.h"

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
#define HOME_REDRAW_INTERVAL_MS  22
#define AUTO_RETURN_TIMEOUT_MS  10000

static unsigned long menu_millis(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/* ── Screen saver ──────────────────────────────────────────────── */
/* After "Screen off after" minutes without the knob, KB0, platter,
 * fader, touch or cue buttons being used, the panel is put to sleep.
 * Playing the instrument wakes it; so does the knob or KB0, whose
 * first event is then swallowed so it does not also act on the menu.
 *
 * Everything here runs on the menu thread, the only one that talks to
 * the display. The input thread just stamps input_activity_ms. */

static unsigned long screen_blanked_at = 0;

static void screen_wake(unsigned long now) {
    oled_set_blank(false);
    lastActivityTime = now;
    needsUpdate = true;
}

/* Called on every poll of the knob. Returns true while the screen is off */
static bool screen_saver_poll(void) {
    unsigned long now = menu_millis();
    unsigned long input = input_activity_ms;
    unsigned long last = lastActivityTime;

    /* Latest of the menu and the playing controls (wrap-safe) */
    if ((long)(input - last) > 0)
        last = input;

    if (oled_is_blank()) {
        if ((long)(input - screen_blanked_at) > 0)
            screen_wake(now);
        return oled_is_blank();
    }

    if (cfg_screen_off < 1.0f)
        return false;
    /* Keep the screen on while recording, to see that it is */
    if (deck[0].player.recording || deck[1].player.recording)
        return false;

    if (now - last >= (unsigned long)(cfg_screen_off * 60000.0f)) {
        screen_blanked_at = now;
        oled_set_blank(true);
        return true;
    }
    return false;
}

/* A knob or KB0 event arrived: wake the screen if it is off.
 * Returns true if the event was used up waking it */
static bool screen_wake_by_menu(void) {
    if (!oled_is_blank())
        return false;
    screen_wake(menu_millis());
    return true;
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

    /* Update cue bar overlay with active deck cue data */
    if (deck_count >= 2) {
        int ad = active_deck;
        double cue_positions[4];
        for (int i = 0; i < 4; i++)
            cue_positions[i] = cues_get(&decks[ad]->cues, i);
        oled_set_cue_overlay(cue_display_states, cue_positions);
    }

    handle_main_menu_navigation(decks, deck_count);

    /* Auto-return removed: rotary push = back is sufficient,
     * and auto-return caused problems during recording and other
     * functional screens (volume adjust, etc.) */

    /* Periodic redraw for home screen (live data at ~45fps); not
     * while the screen is off, there is nothing to see */
    if (mainMenuState == MENU_HOME && !oled_is_blank() &&
        (now - lastHomeRedrawTime) >= HOME_REDRAW_INTERVAL_MS) {
        lastHomeRedrawTime = now;
        needsUpdate = true;
    }

    if (needsUpdate) {
        display_main_menu(decks, deck_count);
        /* display_deck_menu manages needsUpdate for deck submenus.
         * For HOME/MAIN/other top-level screens, clear it here. */
        if (mainMenuState == MENU_HOME || mainMenuState == MENU_MAIN ||
            mainMenuState == MENU_CONTROLLER || mainMenuState == MENU_INFO)
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

    screen_saver_poll();

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
        return screen_wake_by_menu() ? 0 : 1;  /* CW */
    } else if (accumulator <= -4) {
        accumulator = 0;
        lastActivityTime = menu_millis();
        return screen_wake_by_menu() ? 0 : -1; /* CCW */
    }

    return 0;
}

/* Detects rotary button: 1=short click (back), 2=long press (mode toggle)
 * Faster debounce (30ms vs old 100ms) + long press at 500ms while held */
#define ROTARY_DEBOUNCE_READS  6    /* ~30ms at 200Hz */
#define ROTARY_ANTICHATTER_MS  150
#define ROTARY_LONG_PRESS_MS   500

int rotary_button_pressed() {
    static unsigned long lastRotRelease = 0;
    static int pressedCount = 0;
    static int releasedCount = 0;
    static bool confirmed = false;
    static bool longPressFired = false;
    bool raw = gpio_read(ROTARY_SW) == 0; /* active low */
    unsigned long now = gpio_millis();

    if (raw) {
        pressedCount++;
        releasedCount = 0;
        if (pressedCount >= ROTARY_DEBOUNCE_READS && !confirmed) {
            confirmed = true;
            lastButtonPressTime = now;
            longPressFired = false;
        }
        /* Long press: fire once at threshold while still held */
        if (confirmed && !longPressFired &&
            (now - lastButtonPressTime) >= ROTARY_LONG_PRESS_MS) {
            longPressFired = true;
            lastActivityTime = menu_millis();
            return screen_wake_by_menu() ? 0 : 2;  /* long press = mode toggle */
        }
    } else {
        releasedCount++;
        pressedCount = 0;
        if (confirmed && releasedCount >= 3) {
            confirmed = false;
            if (!longPressFired && (now - lastRotRelease) > ROTARY_ANTICHATTER_MS) {
                lastRotRelease = now;
                lastButtonPressTime = 0;
                lastActivityTime = menu_millis();
                longPressFired = false;
                return screen_wake_by_menu() ? 0 : 1;  /* short click = back */
            }
            longPressFired = false;
        }
        if (releasedCount >= 5) { confirmed = false; longPressFired = false; }
    }
    return 0;
}

/* KB0 button (GPIO17): 1 on release (select/enter), 0 otherwise */
int kb0_button_pressed() {
    static unsigned long kb0PressTime = 0;
    static unsigned long kb0LastRelease = 0;

    bool pressed = gpio_read(ROTARY_KO) == 0;  /* active low */
    unsigned long now = gpio_millis();

    if (pressed) {
        if (kb0PressTime == 0) kb0PressTime = now;
    } else {
        if (kb0PressTime > 0 && (now - kb0PressTime) > 30) {
            /* Debounce: ignore presses shorter than 30ms (bounce) */
            /* Also enforce 100ms cooldown between events */
            if ((now - kb0LastRelease) > 100) {
                kb0PressTime = 0;
                kb0LastRelease = now;
                lastActivityTime = menu_millis();
                return screen_wake_by_menu() ? 0 : 1;  /* click = select */
            }
        }
        kb0PressTime = 0;
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
