#include "controller_menu.h"
#include "lcd_menu.h"
#include "alsa_mixer.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include "sc_input.h"
#include "xwax.h"
#include "shared_variables.h"
#include "oled_display.h"

/* Constants */
#define MAX_MIXER_CONTROLS 32

/* Globals */
extern bool needsUpdate;
extern MainMenuState mainMenuState;

static ControllerMenuOption selectedOption = CONTROLLER_SOUND_SETTINGS;
static MixerControl mixerControls[MAX_MIXER_CONTROLS];
static int mixerControlCount = 0;
static bool mixerChanged = false; /* something was adjusted since the last save */

/* Config save path */
#define CONFIG_SAVE_PATH "/home/no3z/.scratchtj/config.cfg"

/* Controller menu options */
static const char *controllerOptions[] = {
    "Sound Settings",
    "Global Settings",
    "Save Config",
    "Reset Defaults",
    "Restart (rescan files)",
    "Reboot Pi"
};

/* Forward declarations */
static void adjust_mixer_control(int selectedIndex);
static void adjust_variable_value(EditableVariable *variable);
static void display_mixer_control(MixerControl *control);
static void action_save_config(void);
static void action_reset_defaults(void);
static void action_restart(void);
static void action_reboot(void);

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
            case CONTROLLER_SAVE_CONFIG:
                action_save_config();
                break;
            case CONTROLLER_RESET_DEFAULTS:
                action_reset_defaults();
                break;
            case CONTROLLER_RESTART:
                action_restart();
                break;
            case CONTROLLER_REBOOT:
                action_reboot();
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
    mixerControlCount = get_mixer_controls(MIXER_CARD, mixerControls, MAX_MIXER_CONTROLS);

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

    /* Keep the settings: without this they were lost at the next boot,
     * when the card comes back up with its stored system defaults */
    if (mixerChanged) {
        mkdir("/home/no3z/.scratchtj", 0755);
        if (mixer_save(MIXER_CARD, MIXER_STATE_PATH) == 0)
            mixerChanged = false;
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
            } else if (control->isBoolean) {
                control->current = !control->current;
            } else if (control->isEnum) {
                control->currentEnumIndex = ((control->currentEnumIndex + movement) %
                                             control->enumItemCount + control->enumItemCount) %
                                            control->enumItemCount;
            }
            apply_mixer_control(MIXER_CARD, control);
            mixerChanged = true;
            needsUpdate = true;
        }

        /* KB0 = confirm */
        if (kb0 == 1) {
            adjusting = false;
        }
        /* Rotary click = cancel (revert) */
        if (button_press == 1) {
            control->current = orig_current;
            control->currentEnumIndex = orig_enumIndex;
            apply_mixer_control(MIXER_CARD, control);
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

            /* Draw fader curve graph for fader-related parameters */
            if (strcmp(variable->name, "Fad Factor") == 0 ||
                strcmp(variable->name, "Fad Power") == 0 ||
                strcmp(variable->name, "fader_sharp") == 0) {
                float ff, fp, fs;
                get_variable_value("Fad Factor", &ff);
                get_variable_value("Fad Power", &fp);
                get_variable_value("fader_sharp", &fs);
                oled_draw_fader_curve(30, 120, 180, 80, ff, fp, fs);
            }

            oled_flush();
            needsUpdate = false;
        }

        usleep(5000);
    }
}

/* Show a confirmation screen, wait for any button, return */
static void show_confirm_screen(const char *line1, const char *line2) {
    oled_clear();
    oled_draw_confirm(line1, line2);
    oled_flush();

    /* Wait for any button press to dismiss */
    for (;;) {
        if (rotary_button_pressed() || kb0_button_pressed())
            break;
        rotary_encoder_moved(); /* drain encoder */
        usleep(10000);
    }
    needsUpdate = true;
}

/* Save current config to disk */
static void action_save_config(void) {
    mkdir("/home/no3z/.scratchtj", 0755);
    if (save_variables_to_file(CONFIG_SAVE_PATH))
        show_confirm_screen("Config Saved", "Press any button");
    else
        show_confirm_screen("Save Failed!", "Press any button");
}

/* Reset all variables to defaults */
static void action_reset_defaults(void) {
    reset_all_variables_to_defaults();
    show_confirm_screen("Defaults Reset", "Press any button");
}

/* Ask before doing something disruptive.
 * Return: true if KB0 was pressed (yes), false on rotary click (cancel) */
static bool confirm_action(const char *question) {
    oled_clear();
    oled_draw_confirm(question, "KB0=yes ROT=no");
    oled_flush();

    for (;;) {
        if (kb0_button_pressed() == 1) {
            needsUpdate = true;
            return true;
        }
        if (rotary_button_pressed() == 1) {
            needsUpdate = true;
            return false;
        }
        rotary_encoder_moved(); /* drain encoder */
        usleep(10000);
    }
}

/* Write out what would otherwise be lost when the program goes away */
static void save_state_before_exit(void) {
    mkdir("/home/no3z/.scratchtj", 0755);
    save_variables_to_file(CONFIG_SAVE_PATH);

    for (int i = 0; i < 2; i++) {
        struct track *tr = deck[i].player.track;
        if (tr && tr->path)
            cues_save_to_file(&deck[i].cues, tr->path);
    }
}

/* Restart the program in place, so that it reads the beats and samples
 * folders again (new files copied over FTP, for instance) */
static void action_restart(void) {
    if (!confirm_action("Restart?"))
        return;

    save_state_before_exit();

    oled_clear();
    oled_draw_confirm("Restarting", "please wait");
    oled_flush();

    xwax_restart_in_place();

    /* Only reached if the exec failed; carry on as before */
    show_confirm_screen("Restart failed", "Press any button");
}

/* Reboot the whole Pi */
static void action_reboot(void) {
    if (!confirm_action("Reboot?"))
        return;

    save_state_before_exit();

    oled_clear();
    oled_draw_confirm("Rebooting", "please wait");
    oled_flush();

    sync();
    if (system("/sbin/reboot") != 0)
        show_confirm_screen("Reboot failed", "Press any button");
}
