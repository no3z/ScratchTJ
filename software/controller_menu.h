#ifndef CONTROLLER_MENU_H
#define CONTROLLER_MENU_H

#include "deck.h"

// Enum for menu options
typedef enum {
    CONTROLLER_SOUND_SETTINGS,
    CONTROLLER_PLATTER_SETTINGS,
    CONTROLLER_FADER_SETTINGS,
    CONTROLLER_SCREEN_SETTINGS,
    CONTROLLER_SAVE_CONFIG,
    CONTROLLER_RESET_DEFAULTS,
    CONTROLLER_RESTART,
    CONTROLLER_REBOOT,
    CONTROLLER_MENU_OPTION_COUNT
} ControllerMenuOption;

// Function Prototypes
void display_controller_menu(struct deck *d, int deck_no);
void handle_controller_menu_navigation(struct deck *d, int deckno);
void enter_sound_settings_menu(struct deck *d, int deckno);
void enter_settings_section(const char *section);

#endif // CONTROLLER_MENU_H
