#ifndef PRESET_MENU_H
#define PRESET_MENU_H

#include "deck.h"

// Preset menu functions
void display_preset_menu();
void handle_preset_menu_navigation(struct deck *decks[]);

// Preset save/load functions
void save_preset_to_slot(int slot, struct deck *decks[]);
void load_preset_from_slot(int slot, struct deck *decks[]);
int load_last_preset_number();
void save_last_preset_number(int preset);

#endif // PRESET_MENU_H
