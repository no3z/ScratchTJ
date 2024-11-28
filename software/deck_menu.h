#ifndef DECK_MENU_H
#define DECK_MENU_H

#include "deck.h"

// Enumeration for deck menu states
typedef enum {
    DECK_MENU_MAIN,
    DECK_MENU_LOAD_FILE,
    DECK_MENU_SETTINGS,
    DECK_MENU_ADJUST_VOLUME,
    DECK_MENU_INFO,
    DECK_MENU_RECORD_INPUT_SOURCE, // State for selecting input source
    DECK_MENU_RECORDING            // State for recording in progress
} DeckMenuState;


// Structure for menu items
typedef struct {
    const char *label;
    void (*action)(struct deck *d, int deckno);
} MenuItem;

// Function declarations

// Main display and navigation functions
void display_deck_menu(struct deck *d, int deck_no);
void handle_deck_menu_navigation(struct deck *d, int deckno);

// Menu navigation and display helpers
void display_menu(MenuItem *menuItems, int menuSize, int selectedItem, const char *title);
void handle_menu_navigation(struct deck *d, int deckno, MenuItem *menuItems, int menuSize);

// Entry functions for sub-menus
void enter_load_file_menu(struct deck *d, int deckno);
void enter_adjust_volume(struct deck *d, int deckno);
void enter_settings_menu(struct deck *d, int deckno);
void enter_deck_info_display(struct deck *d, int deckno);

// Actions for Load File menu
void action_start_stop(struct deck *d, int deckno);
void action_next_file(struct deck *d, int deckno);
void action_prev_file(struct deck *d, int deckno);
void action_random_file(struct deck *d, int deckno);
void action_next_folder(struct deck *d, int deckno);
void action_prev_folder(struct deck *d, int deckno);
void action_record(struct deck *d, int deckno);

// Actions for Settings menu
void action_jog_pitch(struct deck *d, int deckno);
void action_jog_stop(struct deck *d, int deckno);
void action_jog_reverse(struct deck *d, int deckno);

// Volume adjustment function
void adjust_volume(struct deck *d, int deckno);

// Deck information display function
void display_deck_info(struct deck *d, int deckno);

#endif // DECK_MENU_H
