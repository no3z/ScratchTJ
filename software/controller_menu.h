#ifndef CONTROLLER_MENU_H
#define CONTROLLER_MENU_H

#include "deck.h"

// Enum for menu options
typedef enum {
    CONTROLLER_SOUND_SETTINGS,
    CONTROLLER_GLOBAL_SETTINGS,
    CONTROLLER_INFO,
    CONTROLLER_PRESETS,
    CONTROLLER_MENU_OPTION_COUNT
} ControllerMenuOption;

// Function Prototypes
void display_controller_menu(struct deck *decks[], int deck_count);
void handle_controller_menu_navigation(struct deck *decks[], int deck_count);
void enter_sound_settings_menu(struct deck *d, int deckno);
void enter_global_settings_menu(struct deck *d, int deckno);

#endif // CONTROLLER_MENU_H
