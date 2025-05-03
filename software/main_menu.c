#include <stdio.h>
#include "lcd_menu.h"
#include "main_menu.h"
#include "deck_menu.h"
#include "controller_menu.h"
#include "info_menu.h"

extern bool needsUpdate;

char *mainMenuOptions[] = {"Deck1 Menu", "Deck2 Menu", "Config Menu", "Info Menu"};
static int selectedItem = 1;
static int menuSize = 4;

extern MainMenuState mainMenuState;

void display_main_menu(struct deck *decks[], int deck_count) {
    lcdClear(lcdHandle);
    lcdPosition(lcdHandle, 0, 0);
    if (jogPitchLongPressActive) {        
        lcdPuts(lcdHandle, "Pitch Mode");     // Short indicator for Jog Pitch
        lcdPosition(lcdHandle, 0, 1);
        lcdPrintf(lcdHandle, "%d", decks[1]->player.note_pitch);
        return;
    }
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
                display_controller_menu(decks[1], 1);
                break;
            case MENU_INFO:
                display_info_menu_actions();
                break;
            default:
                break;
        }
    }
}

void handle_main_menu_navigation(struct deck *decks[], int deck_count) {

    if (jogPitchLongPressActive) {
        return;
    }

    if (mainMenuState == MENU_MAIN) {
        int button_press = rotary_button_pressed();
        int encoder_movement = rotary_encoder_moved();

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
                case 3:
                    mainMenuState = MENU_INFO;
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
                handle_controller_menu_navigation(decks[0],1);
                break;
            case MENU_INFO:
                handle_info_menu_navigation();
                break;
        }        
    }
}
