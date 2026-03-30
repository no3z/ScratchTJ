#ifndef SC_INPUT_H
#define SC_INPUT_H

#include <stdint.h>

void SC_Input_Start();
void process_rot();
void process_cue_buttons(uint8_t button_byte);

/* Cue display states (for TFT cue bar) */
extern int cue_display_states[4];

/* Hardware state (for home screen) */
extern bool capIsTouched;
extern unsigned int ADCs[4];

#endif