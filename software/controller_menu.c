#include "controller_menu.h"
#include "lcd_menu.h"
#include "alsa_mixer.h"
#include <stdio.h>
#include "sc_input.h"
#include "xwax.h"
#include "shared_variables.h"

// Constants
#define MAX_MIXER_CONTROLS 20

// Globals
extern int lcdHandle;    // LCD handle defined elsewhere
extern bool needsUpdate; // Global flag for display updates
extern MainMenuState mainMenuState;

static ControllerMenuOption selectedOption = CONTROLLER_SOUND_SETTINGS;
static MixerControl mixerControls[MAX_MIXER_CONTROLS];
static int mixerControlCount = 0;

// Controller menu options
const char *controllerOptions[] = {
    "Sound Settings",
    "Global Settings"
};

// Function Prototypes
void adjust_mixer_control(int selectedIndex);
void adjust_variable_value(EditableVariable *variable);

// Display Controller Menu
void display_controller_menu(struct deck *d, int deck_no) {
    lcdClear(lcdHandle);
    lcdPosition(lcdHandle, 0, 0);
    lcdPuts(lcdHandle, "Config");

    // Display selected option
    lcdPosition(lcdHandle, 0, 1);
    lcdPrintf(lcdHandle, controllerOptions[selectedOption]);
}

// Handle Controller Menu Navigation
void handle_controller_menu_navigation(struct deck *d, int deckno) {
    int encoder_movement = rotary_encoder_moved();
    int button_press = rotary_button_pressed();

    // Update selected option based on encoder movement
    if (encoder_movement != 0) {
        selectedOption = (ControllerMenuOption)((selectedOption + encoder_movement + CONTROLLER_MENU_OPTION_COUNT) % CONTROLLER_MENU_OPTION_COUNT);
        needsUpdate = true;
    }

    // Handle button press to activate the selected option
    if (button_press == 1) { // Short press to select
        switch (selectedOption) {
            case CONTROLLER_SOUND_SETTINGS:
                enter_sound_settings_menu(d, deckno);
                break;
            case CONTROLLER_GLOBAL_SETTINGS:
                enter_global_settings_menu(d, deckno);
                break;
        }
        needsUpdate = true;
    } else if (button_press == 2) { // Long press to return to main menu
        mainMenuState = MENU_MAIN;
        needsUpdate = true;
    }
}

// Enter Sound Settings Menu
void enter_sound_settings_menu(struct deck *d, int deckno) {
    mixerControlCount = get_mixer_controls("default", mixerControls, MAX_MIXER_CONTROLS);

    if (mixerControlCount == 0) {
        printf("No mixer controls found!\n");
        return;
    }
    needsUpdate = true;
    bool adjusting = true;
    int selectedControl = 0;

    while (adjusting) {
        int movement = rotary_encoder_moved();
        int button_press = rotary_button_pressed();

        if (movement != 0) {
            selectedControl = (selectedControl + movement + mixerControlCount) % mixerControlCount;
            needsUpdate = true;
        }

        if (button_press == 1) { // Adjust selected control
            adjust_mixer_control(selectedControl);
            needsUpdate = true;

        } else if (button_press == 2) { // Exit sound settings
            adjusting = false;
        }

        if (needsUpdate) {
            MixerControl *control = &mixerControls[selectedControl];

            lcdClear(lcdHandle);
            lcdPosition(lcdHandle, 0, 0);


            if (control->isVolume) {
                lcdPrintf(lcdHandle, "%s", control->name);
                lcdPosition(lcdHandle, 0, 1);
                lcdPrintf(lcdHandle, "[%ld/%ld]", control->current, control->max);
            } else if (control->isBoolean) {
                lcdPrintf(lcdHandle, "%s", control->name);
                lcdPosition(lcdHandle, 0, 1);
                lcdPrintf(lcdHandle, "[%s]",  control->current ? "On" : "Off");
            } else if (control->isEnum) {
                lcdPrintf(lcdHandle, "%s", control->name);
                lcdPosition(lcdHandle, 0, 1);
                lcdPrintf(lcdHandle, "[%s]", control->enumItems[control->currentEnumIndex]);
            } else {
                lcdPosition(lcdHandle, 0, 1);
                lcdPrintf(lcdHandle, "%s [Unknown]", control->name);
            }

            needsUpdate = false;
        }
    }
}

// Adjust a specific Mixer Control
void adjust_mixer_control(int selectedIndex) {
    MixerControl *control = &mixerControls[selectedIndex];
    bool adjusting = true;
    needsUpdate = true;
    while (adjusting) {
        int movement = rotary_encoder_moved();
        int button_press = rotary_button_pressed();

        if (control->isVolume) {
            if (movement != 0) {
                control->current += movement;
                if (control->current < control->min) control->current = control->min;
                if (control->current > control->max) control->current = control->max;

                set_mixer_control("default", control->name, control->current);
                needsUpdate = true;
            }
        } else if (control->isBoolean) {
            if (button_press == 1) {
                control->current = !control->current;
                set_mixer_control_boolean("default", control->name, control->current);
                needsUpdate = true;
            }
        } else if (control->isEnum) {
            if (movement != 0) {
                control->currentEnumIndex = (control->currentEnumIndex + movement + control->enumItemCount) % control->enumItemCount;
                set_mixer_control_enum("default", control->name, control->currentEnumIndex);
                needsUpdate = true;
            }
        }

        if (button_press == 1 || button_press == 2) { // Exit adjustment
            adjusting = false;
        }

        if (needsUpdate) {
            lcdClear(lcdHandle);
            lcdPosition(lcdHandle, 0, 0);
            lcdPrintf(lcdHandle, "%s:", control->name);
            lcdPosition(lcdHandle, 0, 1);

            if (control->isVolume) {
                lcdPrintf(lcdHandle, "%ld/%ld", control->current, control->max);
            } else if (control->isBoolean) {
                lcdPrintf(lcdHandle, "%s", control->current ? "On" : "Off");
            } else if (control->isEnum) {
                lcdPrintf(lcdHandle, "%s", control->enumItems[control->currentEnumIndex]);
            } else {
                lcdPrintf(lcdHandle, "[Unknown]");
            }

            needsUpdate = false;
        }
    }
}

// Enter Global Settings Menu
void enter_global_settings_menu(struct deck *d, int deckno) {
    int variableIndex = 0;
    int variableCount = 0;
    EditableVariable *variables = get_editable_variables(&variableCount);

    if (variableCount == 0) {
        printf("No global settings available!\n");
        return;
    }
    needsUpdate = true;
    bool adjusting = true;
    while (adjusting) {
        int movement = rotary_encoder_moved();
        int button_press = rotary_button_pressed();

        if (movement != 0) {
            variableIndex = (variableIndex + movement + variableCount) % variableCount;
            needsUpdate = true;
        }

        if (button_press == 1) { // Adjust selected variable
            adjust_variable_value(&variables[variableIndex]);
            needsUpdate = true;
        } else if (button_press == 2) { // Exit global settings
            adjusting = false;
        }

        if (needsUpdate) {
            lcdClear(lcdHandle);
            lcdPosition(lcdHandle, 0, 0);
            lcdPuts(lcdHandle, "Global Settings");
            lcdPosition(lcdHandle, 0, 1);
            lcdPrintf(lcdHandle, "%s", variables[variableIndex].name);
            needsUpdate = false;
        }
    }
}

// Adjust a specific Editable Variable
void adjust_variable_value(EditableVariable *variable) {
    bool adjusting = true;
    float value = *variable->valuePtr;
    needsUpdate = true;
    while (adjusting) {
        int movement = rotary_encoder_moved();
        int button_press = rotary_button_pressed();

        if (movement != 0) {
            value += movement * variable->stepSize;
            if (value < variable->minValue) value = variable->minValue;
            if (value > variable->maxValue) value = variable->maxValue;

            pthread_mutex_lock(&variable->mutex);
            *variable->valuePtr = value;
            pthread_mutex_unlock(&variable->mutex);

            needsUpdate = true;
        }

        if (button_press == 1 || button_press == 2) { // Exit adjustment
            adjusting = false;
        }

        if (needsUpdate) {
            lcdClear(lcdHandle);
            lcdPosition(lcdHandle, 0, 0);
            lcdPrintf(lcdHandle, "%s:", variable->name);
            lcdPosition(lcdHandle, 0, 1);
            lcdPrintf(lcdHandle, "%.2f", value);
            needsUpdate = false;
        }
    }
}
