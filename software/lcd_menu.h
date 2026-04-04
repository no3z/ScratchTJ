#ifndef LCD_MENU_H
#define LCD_MENU_H

#include "gpio_direct.h"
#include "oled_display.h"
#include "deck_menu.h"
#include "controller_menu.h"
#include "info_menu.h"
#include "sc_midimap.h"

/* EC11 encoder pins (on TFT board) */
#define ROTARY_CLK 23
#define ROTARY_DT  22
#define ROTARY_SW  27
#define ROTARY_KO  17

/* Main menu states */
typedef enum {
    MENU_HOME,
    MENU_MAIN,
    MENU_DECK1,
    MENU_DECK2,
    MENU_CONTROLLER,
    MENU_INFO,
    MENU_RECORD
} MainMenuState;

extern bool needsUpdate;
extern MainMenuState mainMenuState;

/* Menu control functions */
void lcd_menu_init(struct deck *decks[], int deck_count);
int rotary_encoder_moved();
int rotary_button_pressed();   /* 1=short (back), 2=long (mode toggle) */
int kb0_button_pressed();      /* 1=short (select/forward) */
void poll_rotary_encoder(void);

void trigger_io_event(unsigned char action, unsigned char deckNo, unsigned char param);
void trigger_io_event_no_param(unsigned char action, unsigned char deckNo);

void* rotary_encoder_thread(void* arg);

#endif
