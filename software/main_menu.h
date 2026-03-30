#ifndef MAIN_MENU_H
#define MAIN_MENU_H

#include "deck.h"

void display_main_menu(struct deck *decks[], int deck_count);
void handle_main_menu_navigation(struct deck *decks[], int deck_count);
void display_home_screen(struct deck *decks[], int deck_count);

#endif
