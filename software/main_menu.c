#include <stdio.h>
#include "lcd_menu.h"
#include "main_menu.h"
#include "deck_menu.h"
#include "controller_menu.h"

extern bool needsUpdate;

char *mainMenuOptions[] = {"Deck1 Menu", "Deck2 Menu", "Config Menu"};
static int selectedItem = 1;
static int menuSize = 3;

extern MainMenuState mainMenuState;

void display_main_menu(struct deck *decks[], int deck_count) {
    lcdClear(lcdHandle);
    lcdPosition(lcdHandle, 0, 0);

    if (mainMenuState == MENU_MAIN) {
        lcdPuts(lcdHandle, "Main Menu");
        lcdPosition(lcdHandle, 0, 1);
        lcdPrintf(lcdHandle, mainMenuOptions[selectedItem]);
    } else {
        switch (mainMenuState) {
            case MENU_DECK1:
                display_deck_menu(decks[0], 0);
                break;
            case MENU_DECK2:
                display_deck_menu(decks[1], 1);
                break;
            case MENU_CONTROLLER:
                display_controller_menu(decks, deck_count);
                break;
            default:
                break;
        }
    }
}

void handle_main_menu_navigation(struct deck *decks[], int deck_count) {

    // Rotary encoder button = home from anywhere
    if (home_button_pressed() && mainMenuState != MENU_MAIN) {
        mainMenuState = MENU_MAIN;
        deck_menu_reset();
        selectedItem = 1;
        needsUpdate = true;
        return;
    }

    if (mainMenuState == MENU_MAIN) {
        int encoder_movement = rotary_encoder_moved();
        int button_press = rotary_button_pressed();
        
        if (encoder_movement != 0) {
            selectedItem = (selectedItem + encoder_movement + menuSize) % menuSize;
            needsUpdate = true;
        }

        if (button_press == 1) {
            switch (selectedItem) {
                case 0:
                    mainMenuState = MENU_DECK1;
                    break;
                case 1:
                    mainMenuState = MENU_DECK2;
                    break;
                case 2:
                    mainMenuState = MENU_CONTROLLER;
                    break;
                default:
                    break;
            }
            selectedItem = 0;
            needsUpdate = true;
        }
    } else {
        // Delegate control to the active submenu
        switch (mainMenuState) {
            case MENU_DECK1:
                handle_deck_menu_navigation(decks[0],0);
                break;
            case MENU_DECK2:
                handle_deck_menu_navigation(decks[1],1);
                break;
            case MENU_CONTROLLER:
                handle_controller_menu_navigation(decks, deck_count);
                break;
        }        
    }
}
