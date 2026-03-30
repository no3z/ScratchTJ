#include "controller_menu.h"
#include "lcd_menu.h"
#include "alsa_mixer.h"
#include <stdio.h>
#include <unistd.h>
#include "sc_input.h"
#include "xwax.h"
#include "shared_variables.h"

/* Constants */
#define MAX_MIXER_CONTROLS 20

/* Globals */
extern bool needsUpdate;
extern MainMenuState mainMenuState;

static ControllerMenuOption selectedOption = CONTROLLER_SOUND_SETTINGS;
static MixerControl mixerControls[MAX_MIXER_CONTROLS];
static int mixerControlCount = 0;

/* Controller menu options */
static const char *controllerOptions[] = {
    "Sound Settings",
    "Global Settings"
};

/* Forward declarations */
static void adjust_mixer_control(int selectedIndex);
static void adjust_variable_value(EditableVariable *variable);
static void display_mixer_control(MixerControl *control);

/* Display Controller Menu */
void display_controller_menu(struct deck *d, int deck_no) {
    oled_clear();
    oled_draw_title_bar("Config");
    oled_draw_menu_list(controllerOptions, CONTROLLER_MENU_OPTION_COUNT,
                        selectedOption, 0, MENU_VISIBLE_LINES);
    oled_flush();
}

/* Handle Controller Menu Navigation */
void handle_controller_menu_navigation(struct deck *d, int deckno) {
    int encoder_movement = rotary_encoder_moved();
    int button_press = rotary_button_pressed();
    int kb0 = kb0_button_pressed();

    if (encoder_movement != 0) {
        selectedOption = (ControllerMenuOption)((selectedOption + encoder_movement +
                          CONTROLLER_MENU_OPTION_COUNT) % CONTROLLER_MENU_OPTION_COUNT);
        needsUpdate = true;
    }

    /* KB0 = select */
    if (kb0 == 1) {
        switch (selectedOption) {
            case CONTROLLER_SOUND_SETTINGS:
                enter_sound_settings_menu(d, deckno);
                break;
            case CONTROLLER_GLOBAL_SETTINGS:
                enter_global_settings_menu(d, deckno);
                break;
            default:
                break;
        }
        needsUpdate = true;
    }
    /* Rotary click = back */
    if (button_press == 1) {
        mainMenuState = MENU_MAIN;
        needsUpdate = true;
    }
}

/* Display a mixer control value */
static void display_mixer_control(MixerControl *control) {
    char val_str[32];
    char range_str[32];

    oled_clear();

    if (control->isVolume) {
        snprintf(val_str, sizeof(val_str), "%ld", control->current);
        snprintf(range_str, sizeof(range_str), "%ld - %ld", control->min, control->max);
        oled_draw_value_screen(control->name, val_str, range_str);
    } else if (control->isBoolean) {
        snprintf(val_str, sizeof(val_str), "%s", control->current ? "On" : "Off");
        oled_draw_value_screen(control->name, val_str, "On/Off");
    } else if (control->isEnum) {
        oled_draw_value_screen(control->name,
                               control->enumItems[control->currentEnumIndex], NULL);
    } else {
        oled_draw_value_screen(control->name, "Unknown", NULL);
    }

    oled_flush();
}

/* Enter Sound Settings Menu */
void enter_sound_settings_menu(struct deck *d, int deckno) {
    mixerControlCount = get_mixer_controls("default", mixerControls, MAX_MIXER_CONTROLS);

    if (mixerControlCount == 0) {
        printf("No mixer controls found!\n");
        return;
    }

    bool adjusting = true;
    int selectedControl = 0;
    int scrollOff = 0;
    needsUpdate = true;

    while (adjusting) {
        int movement = rotary_encoder_moved();
        int button_press = rotary_button_pressed();
        int kb0 = kb0_button_pressed();

        if (movement != 0) {
            selectedControl = (selectedControl + movement + mixerControlCount) % mixerControlCount;
            needsUpdate = true;
        }

        if (kb0 == 1) {
            adjust_mixer_control(selectedControl);
            needsUpdate = true;
        }
        if (button_press == 1) {
            adjusting = false;
        }

        if (needsUpdate) {
            const char *labels[MAX_MIXER_CONTROLS];
            for (int i = 0; i < mixerControlCount; i++)
                labels[i] = mixerControls[i].name;

            scrollOff = oled_compute_scroll(selectedControl, scrollOff, MENU_VISIBLE_LINES);
            oled_clear();
            oled_draw_title_bar("Sound Settings");
            oled_draw_menu_list(labels, mixerControlCount, selectedControl,
                                scrollOff, MENU_VISIBLE_LINES);
            oled_flush();
            needsUpdate = false;
        }

        usleep(5000);
    }
}

/* Adjust a specific Mixer Control */
static void adjust_mixer_control(int selectedIndex) {
    MixerControl *control = &mixerControls[selectedIndex];
    bool adjusting = true;
    needsUpdate = true;

    /* Save original values for cancel/revert */
    long orig_current = control->current;
    int orig_enumIndex = control->currentEnumIndex;

    while (adjusting) {
        int movement = rotary_encoder_moved();
        int button_press = rotary_button_pressed();
        int kb0 = kb0_button_pressed();

        /* EC11 rotary = change value */
        if (movement != 0) {
            if (control->isVolume) {
                control->current += movement;
                if (control->current < control->min) control->current = control->min;
                if (control->current > control->max) control->current = control->max;
                set_mixer_control("default", control->name, control->current);
            } else if (control->isBoolean) {
                control->current = !control->current;
                set_mixer_control_boolean("default", control->name, control->current);
            } else if (control->isEnum) {
                control->currentEnumIndex = (control->currentEnumIndex + movement +
                                             control->enumItemCount) % control->enumItemCount;
                set_mixer_control_enum("default", control->name, control->currentEnumIndex);
            }
            needsUpdate = true;
        }

        /* KB0 = confirm */
        if (kb0 == 1) {
            adjusting = false;
        }
        /* Rotary click = cancel (revert) */
        if (button_press == 1) {
            if (control->isVolume) {
                control->current = orig_current;
                set_mixer_control("default", control->name, control->current);
            } else if (control->isBoolean) {
                control->current = orig_current;
                set_mixer_control_boolean("default", control->name, control->current);
            } else if (control->isEnum) {
                control->currentEnumIndex = orig_enumIndex;
                set_mixer_control_enum("default", control->name, control->currentEnumIndex);
            }
            adjusting = false;
        }

        if (needsUpdate) {
            display_mixer_control(control);
            needsUpdate = false;
        }

        usleep(5000);
    }
}

/* Enter Global Settings Menu */
void enter_global_settings_menu(struct deck *d, int deckno) {
    int variableIndex = 0;
    int variableCount = 0;
    int scrollOff = 0;
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
        int kb0 = kb0_button_pressed();

        if (movement != 0) {
            variableIndex = (variableIndex + movement + variableCount) % variableCount;
            needsUpdate = true;
        }

        if (kb0 == 1) {
            adjust_variable_value(&variables[variableIndex]);
            needsUpdate = true;
        }
        if (button_press == 1) {
            adjusting = false;
        }

        if (needsUpdate) {
            const char *labels[32];
            int max = variableCount < 32 ? variableCount : 32;
            for (int i = 0; i < max; i++)
                labels[i] = variables[i].name;

            scrollOff = oled_compute_scroll(variableIndex, scrollOff, MENU_VISIBLE_LINES);
            oled_clear();
            oled_draw_title_bar("Global Settings");
            oled_draw_menu_list(labels, variableCount, variableIndex,
                                scrollOff, MENU_VISIBLE_LINES);
            oled_flush();
            needsUpdate = false;
        }

        usleep(5000);
    }
}

/* Adjust a specific Editable Variable */
static void adjust_variable_value(EditableVariable *variable) {
    bool adjusting = true;
    float value = *variable->valuePtr;
    float orig_value = value;
    needsUpdate = true;

    while (adjusting) {
        int movement = rotary_encoder_moved();
        int button_press = rotary_button_pressed();
        int kb0 = kb0_button_pressed();

        /* EC11 rotary = change value */
        if (movement != 0) {
            value += movement * variable->stepSize;
            if (value < variable->minValue) value = variable->minValue;
            if (value > variable->maxValue) value = variable->maxValue;

            pthread_mutex_lock(&variable->mutex);
            *variable->valuePtr = value;
            pthread_mutex_unlock(&variable->mutex);

            needsUpdate = true;
        }

        /* KB0 = confirm */
        if (kb0 == 1) {
            adjusting = false;
        }
        /* Rotary click = cancel (revert) */
        if (button_press == 1) {
            pthread_mutex_lock(&variable->mutex);
            *variable->valuePtr = orig_value;
            pthread_mutex_unlock(&variable->mutex);
            adjusting = false;
        }

        if (needsUpdate) {
            char val_str[16];
            char range_str[32];
            snprintf(val_str, sizeof(val_str), "%.2f", value);
            snprintf(range_str, sizeof(range_str), "%.1f - %.1f",
                     variable->minValue, variable->maxValue);

            oled_clear();
            oled_draw_value_screen(variable->name, val_str, range_str);
            oled_flush();
            needsUpdate = false;
        }

        usleep(5000);
    }
}
