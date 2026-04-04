#ifndef SC_INPUT_H
#define SC_INPUT_H

#include <stdint.h>
#include <stdbool.h>

/* Function modes for cue button behavior */
typedef enum {
    FUNC_MODE_CUE = 0,       /* default: buttons = cue points */
    FUNC_MODE_SETTINGS = 1   /* buttons = parameter adjusters */
} FunctionMode;

extern volatile FunctionMode current_function_mode;
extern volatile uint8_t settings_buttons_held;  /* bitmask of held cue buttons in SETTINGS */
extern volatile int active_deck;                /* 0 or 1: which deck platter controls */

void SC_Input_Start();
void process_rot();
void process_cue_buttons(uint8_t button_byte);

/* Cue display states (for TFT cue bar) */
extern int cue_display_states[4];

/* Hardware state (for home screen) */
extern bool capIsTouched;
extern unsigned int ADCs[4];

#endif