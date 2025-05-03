#ifndef LCD_MENU_H
#define LCD_MENU_H


#include <wiringPi.h>
#include "deck_menu.h"
#include "controller_menu.h"
#include "info_menu.h"
#include "sc_midimap.h" // Ensure this header is included


#define ROTARY_CLK 24
#define ROTARY_DT 25
#define ROTARY_SW 23

// Enum for main menu states
typedef enum {
    MENU_MAIN,
    MENU_DECK1,
    MENU_DECK2,
    MENU_CONTROLLER,
    MENU_INFO
} MainMenuState;


extern int lcdHandle;  // Accessible across all menu files
extern bool needsUpdate;
extern MainMenuState mainMenuState;
extern bool jogPitchLongPressActive;

// Menu Control Functions
void lcd_menu_init(struct deck *decks[], int deck_count);
int rotary_encoder_moved();
int rotary_button_pressed();
void poll_rotary_encoder(void);


void trigger_io_event(unsigned char action, unsigned char deckNo, unsigned char param);
void trigger_io_event_no_param(unsigned char action, unsigned char deckNo);

void* rotary_encoder_thread(void* arg);

void lcd_set_cursor(int line, int position);
void lcdClear(int handle);
void lcdPuts(int handle, const char *string);
void lcdPrintf(int handle, const char *format, ...);
void lcdPosition(int handle, int col, int row);
#endif
