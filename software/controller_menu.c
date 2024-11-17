#include "controller_menu.h"
#include "lcd_menu.h"
#include <stdio.h>
#include "sc_input.h"
#include "xwax.h"
#include "shared_variables.h" // Include the shared variables header

extern int lcdHandle;   // LCD handle defined elsewhere
extern bool needsUpdate; // Global flag for display updates
extern MainMenuState mainMenuState;
static unsigned char faderOpen1 = 0;
static ControllerMenuOption selectedOption = CONTROLLER_MIXER;
static int menuOptionsCount = CONTROLLER_MENU_OPTION_COUNT;

// Controller menu options
const char *controllerOptions[] = {
    "Mixer",
    "Fader",
    "Rotary",
    "Edit Variables"
};
void enter_edit_variables_mode();
void adjust_variable_value(EditableVariable *variable);

// Display function for controller menu
void display_controller_menu(struct deck *d, int deck_no) {
    lcdClear(lcdHandle);
    lcdPosition(lcdHandle, 0, 0);
    lcdPuts(lcdHandle, "Controller Menu");

    // Display selected option
    lcdPosition(lcdHandle, 0, 1);
    lcdPrintf(lcdHandle, controllerOptions[selectedOption]);
}

void enter_mixer_mode() {
    int volume = 64;  // Start with a mid-range volume
    bool adjusting = true;
    needsUpdate = true;

    while (adjusting) {
        int movement = rotary_encoder_moved();
        if (movement != 0) {
            volume = (volume + movement) % 128;  // Ensure volume stays within 0-127
            printf("Volume: %d\n", volume);
            trigger_io_event(ACTION_VOLUME, 0, volume);  // Assuming deck 0 for simplicity
            needsUpdate = true;
        }

        int button_press = rotary_button_pressed();
        if (button_press == 1) {  // Short press to exit
            adjusting = false;
        }

        if (needsUpdate) {
            lcdClear(lcdHandle);
            lcdPosition(lcdHandle, 0, 0);
            lcdPrintf(lcdHandle, "Volume: %d", volume);
            needsUpdate = false;
        }
    }
}

void apply_fader_logic(struct deck *d, int deckno, double faderValue) {
    // Example: Apply fader value to volume
    d->player.setVolume = faderValue;

    // Implement hysteresis logic if needed
    unsigned int faderCutPoint = faderOpen1 ? scsettings.faderclosepoint : scsettings.faderopenpoint;
    if (faderValue < faderCutPoint / 1024.0) {
        if (scsettings.cutbeats == 1) d->player.setVolume = 0.0;
        faderOpen1 = 0;
    } else {
        faderOpen1 = 1;
    }

    // Update fader target if needed
    d->player.faderTarget = d->player.setVolume;
}


void enter_fader_mode(struct deck *d, int deckno) {
    double faderValue = 0.5;  // Start with a mid-range value
    bool adjusting = true;
    needsUpdate = true;

    while (adjusting) {
        int movement = rotary_encoder_moved();
        if (movement != 0) {
            // Adjust fader value based on encoder movement
            faderValue += movement * 0.1;  // Adjust sensitivity as needed
            if (faderValue < 0.0) faderValue = 0.0;
            if (faderValue > 1.0) faderValue = 1.0;

            printf("Fader: %.2f\n", faderValue);

            // Apply fader logic
            apply_fader_logic(d, deckno, faderValue);

            needsUpdate = true;
        }

        int button_press = rotary_button_pressed();
        if (button_press == 1) {  // Short press to exit
            adjusting = false;
        }

        if (needsUpdate) {
            lcdClear(lcdHandle);
            lcdPosition(lcdHandle, 0, 0);
            lcdPrintf(lcdHandle, "Fader: %.2f", faderValue);
            needsUpdate = false;
        }
    }
}



void process_rot_with_encoder(struct deck *d, int deckno)  {
    bool adjusting = true;
    needsUpdate = true;
    
    while (adjusting) {
        int encoder_movement = rotary_encoder_moved();
        if (encoder_movement != 0) {
            d->newEncoderAngle += encoder_movement*32;  // Adjust based on movement
            // Ensure the angle stays within valid range
            if (d->newEncoderAngle < 0) d->newEncoderAngle = 0;
            if (d->newEncoderAngle > 4095) d->newEncoderAngle = 4095;
            // Call the original process_rot logic with the updated angle
            process_rot();

            needsUpdate = true;                          
        }

        int button_press = rotary_button_pressed();
        if (button_press == 1) {  // Short press to exit
            adjusting = false;
        }

        if (needsUpdate) {
            lcdClear(lcdHandle);
            lcdPosition(lcdHandle, 0, 0);
            lcdPrintf(lcdHandle, "Spin: %.3f", d->newEncoderAngle);
            needsUpdate = false;
        }
    }

    
}

// Navigation handler for controller menu
void handle_controller_menu_navigation(struct deck *d, int deckno) {
    int encoder_movement = rotary_encoder_moved();
    int button_press = rotary_button_pressed();

    // Update selected option based on encoder movement
    if (encoder_movement != 0) {
        selectedOption = (ControllerMenuOption)((selectedOption + encoder_movement + menuOptionsCount) % menuOptionsCount);
        needsUpdate = true;
    }

    // Handle button press to activate the selected option
    if (button_press == 1) {  // Short press to select
        switch (selectedOption) {
            case CONTROLLER_EDIT_VARIABLES:
                printf("Controller: Editing Variables...\n");
                enter_edit_variables_mode();
                break;         
            case CONTROLLER_MIXER:
                printf("Controller: Activating...\n");
                enter_mixer_mode();
                break;
            case CONTROLLER_FADER:
                printf("Controller: Opening Settings...\n");
                enter_fader_mode(d,deckno);
                break;
            case CONTROLLER_ROTARY:
                printf("Controller: Showing Info...\n");
                process_rot_with_encoder(d,deckno);
                break;
        }
        needsUpdate = true;
    } else if (button_press == 2) {  // Long press to return to main menu
        mainMenuState = MENU_MAIN;
        needsUpdate = true;
    }
}


void enter_edit_variables_mode() {
    int variableIndex = 0;
    int variableCount = 0;
    EditableVariable *variables = get_editable_variables(&variableCount);
    bool selectingVariable = true;
    needsUpdate = true;

    while (selectingVariable) {
        int encoder_movement = rotary_encoder_moved();
        int button_press = rotary_button_pressed();

        if (encoder_movement != 0) {
            variableIndex = (variableIndex + encoder_movement + variableCount) % variableCount;
            needsUpdate = true;
        }

        if (button_press == 1) {  // Short press to select variable
            adjust_variable_value(&variables[variableIndex]);
            needsUpdate = true;
        } else if (button_press == 2) {  // Long press to exit
            selectingVariable = false;
            needsUpdate = true;
        }

        if (needsUpdate) {
            lcdClear(lcdHandle);
            lcdPosition(lcdHandle, 0, 0);
            lcdPuts(lcdHandle, "Select Variable:");
            lcdPosition(lcdHandle, 0, 1);
            lcdPrintf(lcdHandle, "%s", variables[variableIndex].name);
            needsUpdate = false;
        }
    }
}

void adjust_variable_value(EditableVariable *variable) {
    bool adjusting = true;
    needsUpdate = true;
    float currentValue;

    // Get the initial value safely
    pthread_mutex_lock(&variable->mutex);
    currentValue = *(variable->valuePtr);
    pthread_mutex_unlock(&variable->mutex);

    while (adjusting) {
        int movement = rotary_encoder_moved();
        int button_press = rotary_button_pressed();

        if (movement != 0) {
            currentValue += movement * variable->stepSize;
            if (currentValue < variable->minValue) currentValue = variable->minValue;
            if (currentValue > variable->maxValue) currentValue = variable->maxValue;

            // Set the new value safely
            pthread_mutex_lock(&variable->mutex);
            *(variable->valuePtr) = currentValue;
            pthread_mutex_unlock(&variable->mutex);

            needsUpdate = true;
        }

        if (button_press == 1) {  // Short press to exit adjustment
            adjusting = false;
            needsUpdate = true;
        } else if (button_press == 2) {  // Long press to exit to variable selection
            adjusting = false;
            needsUpdate = true;
        }

        if (needsUpdate) {
            lcdClear(lcdHandle);
            lcdPosition(lcdHandle, 0, 0);
            lcdPrintf(lcdHandle, "%s:", variable->name);
            lcdPosition(lcdHandle, 0, 1);
            lcdPrintf(lcdHandle, "%.2f", currentValue);
            needsUpdate = false;
        }
    }
}