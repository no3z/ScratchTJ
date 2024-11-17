#include "deck_menu.h"
#include "lcd_menu.h"
#include "deck.h"
#include "player.h"
#include <unistd.h>
#include <string.h>
#include "track.h"

// Externals
extern bool needsUpdate;
extern MainMenuState mainMenuState;

// Menu and State Variables
static DeckMenuState currentDeckMenuState = DECK_MENU_MAIN;
static int selectedItem = 0;

// Main deck menu items
static MenuItem mainDeckMenuItems[] = {
    {"1. Load File", enter_load_file_menu},
    {"2. Set Volume", enter_adjust_volume},
    {"3. Settings", enter_settings_menu},
    {"4. Info", enter_deck_info_display},
};

static int mainDeckMenuSize = sizeof(mainDeckMenuItems) / sizeof(MenuItem);

// Load file menu items
static MenuItem loadFileMenuItems[] = {
    {"1. Start/Stop", action_start_stop},
    {"2. Next File", action_next_file},
    {"3. Previous File", action_prev_file},
    {"4. Random File", action_random_file},
    {"5. Next Folder", action_next_folder},
    {"6. Previous Folder", action_prev_folder},
    {"7. Record", action_record},
};

static int loadFileMenuSize = sizeof(loadFileMenuItems) / sizeof(MenuItem);

// Settings menu items
static MenuItem settingsMenuItems[] = {
    {"1. Jog Pitch Mode", action_jog_pitch},
    {"2. Stop Pitch Mode", action_jog_stop},
    {"3. Toggle Jog Reverse", action_jog_reverse},
};

static int settingsMenuSize = sizeof(settingsMenuItems) / sizeof(MenuItem);

// Function Definitions

// Display Deck Menu
void display_deck_menu(struct deck *d, int deck_no) {
    if (needsUpdate) {
        switch (currentDeckMenuState) {
            case DECK_MENU_MAIN:
                display_menu(mainDeckMenuItems, mainDeckMenuSize, selectedItem, "Deck Menu");
                break;
            case DECK_MENU_LOAD_FILE:
                display_menu(loadFileMenuItems, loadFileMenuSize, selectedItem, "Load File");
                break;
            case DECK_MENU_SETTINGS:
                display_menu(settingsMenuItems, settingsMenuSize, selectedItem, "Settings");
                break;
            case DECK_MENU_ADJUST_VOLUME:
                lcdClear(lcdHandle);
                lcdPosition(lcdHandle, 0, 0);
                lcdPrintf(lcdHandle, "Volume: %.2f", d->player.setVolume);
                break;
            case DECK_MENU_INFO:
                display_deck_info(d, deck_no);
                break;
            default:
                break;
        }
        needsUpdate = false;
    }
}

// Handle Deck Menu Navigation
void handle_deck_menu_navigation(struct deck *d, int deckno) {
    switch (currentDeckMenuState) {
        case DECK_MENU_MAIN:
            handle_menu_navigation(d, deckno, mainDeckMenuItems, mainDeckMenuSize);
            break;
        case DECK_MENU_LOAD_FILE:
            handle_menu_navigation(d, deckno, loadFileMenuItems, loadFileMenuSize);
            break;
        case DECK_MENU_SETTINGS:
            handle_menu_navigation(d, deckno, settingsMenuItems, settingsMenuSize);
            break;
        case DECK_MENU_ADJUST_VOLUME:
            adjust_volume(d, deckno);
            break;
        case DECK_MENU_INFO:
            if (rotary_button_pressed() == 1 || rotary_button_pressed() == 2) {
                currentDeckMenuState = DECK_MENU_MAIN;
                needsUpdate = true;
            }
            break;
        default:
            break;
    }
}

// Function to display a menu
void display_menu(MenuItem *menuItems, int menuSize, int selectedItem, const char *title) {
    lcdClear(lcdHandle);
    lcdPosition(lcdHandle, 0, 0);
    lcdPrintf(lcdHandle, "%s", title);
    lcdPosition(lcdHandle, 0, 1);
    lcdPrintf(lcdHandle, "%s", menuItems[selectedItem].label);
}

// Function to handle menu navigation
void handle_menu_navigation(struct deck *d, int deckno, MenuItem *menuItems, int menuSize) {
    int encoder_movement = rotary_encoder_moved();
    int button_press = rotary_button_pressed();

    if (encoder_movement != 0) {
        selectedItem = (selectedItem + encoder_movement + menuSize) % menuSize;
        needsUpdate = true;
    }

    if (button_press == 1) { // Short press to select an option
        menuItems[selectedItem].action(d, deckno);
        needsUpdate = true;
    }

if (button_press == 2) { // Long press to go back
    if (currentDeckMenuState == DECK_MENU_MAIN) {
        // Exit to main menu
        mainMenuState = MENU_MAIN;
        currentDeckMenuState = DECK_MENU_MAIN;
        selectedItem = 0;
        needsUpdate = true;
    } else {
        // Go back to deck main menu
        currentDeckMenuState = DECK_MENU_MAIN;
        selectedItem = 0;
        needsUpdate = true;
    }
}
}

// Menu actions
void enter_load_file_menu(struct deck *d, int deckno) {
    currentDeckMenuState = DECK_MENU_LOAD_FILE;
    selectedItem = 0;
    needsUpdate = true;
}

void enter_adjust_volume(struct deck *d, int deckno) {
    currentDeckMenuState = DECK_MENU_ADJUST_VOLUME;
    needsUpdate = true;
}

void enter_settings_menu(struct deck *d, int deckno) {
    currentDeckMenuState = DECK_MENU_SETTINGS;
    selectedItem = 0;
    needsUpdate = true;
}

void enter_deck_info_display(struct deck *d, int deckno) {
    currentDeckMenuState = DECK_MENU_INFO;
    needsUpdate = true;
}

void adjust_volume(struct deck *d, int deckno) {
    int movement = rotary_encoder_moved();
    int button_press = rotary_button_pressed();

    if (movement != 0) {
        d->player.setVolume += movement * 0.05; // Adjust sensitivity as needed
        if (d->player.setVolume < 0.0) d->player.setVolume = 0.0;
        if (d->player.setVolume > 1.0) d->player.setVolume = 1.0;
        needsUpdate = true;
    }

    if (button_press == 1 || button_press == 2) {
        currentDeckMenuState = DECK_MENU_MAIN;
        needsUpdate = true;
    }
}

// Actions for load file menu
void action_start_stop(struct deck *d, int deckno) {
    trigger_io_event_no_param(ACTION_STARTSTOP, deckno);
    currentDeckMenuState = DECK_MENU_MAIN;
}

void action_next_file(struct deck *d, int deckno) {
    trigger_io_event_no_param(ACTION_NEXTFILE, deckno);
    currentDeckMenuState = DECK_MENU_MAIN;
}

void action_prev_file(struct deck *d, int deckno) {
    trigger_io_event_no_param(ACTION_PREVFILE, deckno);
    currentDeckMenuState = DECK_MENU_MAIN;
}

void action_random_file(struct deck *d, int deckno) {
    trigger_io_event_no_param(ACTION_RANDOMFILE, deckno);
    currentDeckMenuState = DECK_MENU_MAIN;
}

void action_next_folder(struct deck *d, int deckno) {
    trigger_io_event_no_param(ACTION_NEXTFOLDER, deckno);
    currentDeckMenuState = DECK_MENU_MAIN;
}

void action_prev_folder(struct deck *d, int deckno) {
    trigger_io_event_no_param(ACTION_PREVFOLDER, deckno);
    currentDeckMenuState = DECK_MENU_MAIN;
}

void action_record(struct deck *d, int deckno) {
    trigger_io_event_no_param(ACTION_RECORD, 0);
    currentDeckMenuState = DECK_MENU_MAIN;
}

// Actions for settings menu
void action_jog_pitch(struct deck *d, int deckno) {
    printf("Deck %d: Triggering Jog Pitch...\n", deckno + 1);
    trigger_io_event_no_param(ACTION_JOGPIT, deckno);
    currentDeckMenuState = DECK_MENU_MAIN;
}

void action_jog_stop(struct deck *d, int deckno) {
    printf("Deck %d: Triggering Jog Stop...\n", deckno + 1);
    trigger_io_event_no_param(ACTION_JOGPSTOP, deckno);
    currentDeckMenuState = DECK_MENU_MAIN;
}

void action_jog_reverse(struct deck *d, int deckno) {
    printf("Deck %d: Triggering Jog Reverse...\n", deckno + 1);
    trigger_io_event_no_param(ACTION_JOGREVERSE, deckno);
    currentDeckMenuState = DECK_MENU_MAIN;
}

// Deck info display
void display_deck_info(struct deck *d, int deckno) {
    lcdClear(lcdHandle);
    lcdPosition(lcdHandle, 0, 0);
    lcdPrintf(lcdHandle, "ON: %d", player_is_active(&d->player));
    lcdPosition(lcdHandle, 6, 0);
    lcdPrintf(lcdHandle, "E: %.2f", player_get_elapsed(&d->player));

    lcdPosition(lcdHandle, 0, 1);
    lcdPrintf(lcdHandle, "%.2f/%.2f", player_get_position(&d->player), player_get_remain(&d->player));
}
