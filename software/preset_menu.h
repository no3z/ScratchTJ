#ifndef PRESET_MENU_H
#define PRESET_MENU_H

#include "deck.h"

// Preset submenu (blocking loop, called from controller menu)
void enter_preset_submenu(struct deck *decks[]);

// Preset save/load functions
void save_preset_to_slot(int slot, struct deck *decks[]);
void load_preset_from_slot(int slot, struct deck *decks[]);
int load_last_preset_number();
void save_last_preset_number(int preset);

// Reset all settings to defaults
void reset_to_defaults(void);

#endif // PRESET_MENU_H
