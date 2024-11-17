#ifndef CONTROLLER_MENU_H
#define CONTROLLER_MENU_H

#include "deck.h"

// Enumeration for controller menu options
typedef enum {
    CONTROLLER_MIXER,
    CONTROLLER_FADER,
    CONTROLLER_ROTARY,
    CONTROLLER_EDIT_VARIABLES, // New option
    CONTROLLER_MENU_OPTION_COUNT
} ControllerMenuOption;

// Function declarations
void display_controller_menu(struct deck *d, int deck_no);
void handle_controller_menu_navigation(struct deck *d, int deckno);

#endif // CONTROLLER_MENU_H
