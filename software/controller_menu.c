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
#include "settings.h"
#include <math.h>
#include <time.h>

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
    SECTION_PLATTER,
    SECTION_FADER,
    SECTION_SCREEN,
    "Save Config",
    "Reset Defaults",
    "Restart (rescan files)",
    "Reboot Pi"
};

/* Forward declarations */
static void adjust_mixer_control(int selectedIndex);
static bool adjust_variable_value(EditableVariable *variable);
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
            case CONTROLLER_PLATTER_SETTINGS:
                enter_settings_section(SECTION_PLATTER);
                break;
            case CONTROLLER_FADER_SETTINGS:
                enter_settings_section(SECTION_FADER);
                break;
            case CONTROLLER_SCREEN_SETTINGS:
                enter_settings_section(SECTION_SCREEN);
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

#define MAX_SECTION_ITEMS 24

static unsigned long ui_millis(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/* Save the settings, creating the folder if needed */
static bool save_config(void) {
    mkdir("/home/no3z/.scratchtj", 0755);
    return save_variables_to_file(CONFIG_SAVE_PATH);
}

/* One section of the Config menu (Platter, Fader & Volume, Screen):
 * each setting with its value; KB0 opens it, knob push goes back.
 * Anything changed is saved on the way out. */
void enter_settings_section(const char *section) {
    int count = 0;
    EditableVariable *all = get_editable_variables(&count);
    EditableVariable *items[MAX_SECTION_ITEMS];
    int n = 0;

    for (int i = 0; i < count && n < MAX_SECTION_ITEMS; i++) {
        if (all[i].section && strcmp(all[i].section, section) == 0)
            items[n++] = &all[i];
    }
    if (n == 0)
        return;

    int selected = 0, scrollOff = 0;
    bool changed = false;
    bool adjusting = true;
    needsUpdate = true;

    while (adjusting) {
        int movement = rotary_encoder_moved();
        int button_press = rotary_button_pressed();
        int kb0 = kb0_button_pressed();

        if (movement != 0) {
            selected = (selected + movement + n) % n;
            needsUpdate = true;
        }
        if (kb0 == 1) {
            if (adjust_variable_value(items[selected]))
                changed = true;
            needsUpdate = true;
        }
        if (button_press == 1)
            adjusting = false;

        if (needsUpdate) {
            const char *labels[MAX_SECTION_ITEMS];
            const char *values[MAX_SECTION_ITEMS];
            char value_text[MAX_SECTION_ITEMS][16];

            for (int i = 0; i < n; i++) {
                labels[i] = items[i]->label;
                format_variable_value(items[i], *items[i]->valuePtr,
                                      value_text[i], sizeof(value_text[i]));
                values[i] = value_text[i];
            }
            scrollOff = oled_compute_scroll(selected, scrollOff, MENU_VISIBLE_LINES);
            oled_clear();
            oled_draw_title_bar(section);
            oled_draw_value_list(labels, values, n, selected,
                                 scrollOff, MENU_VISIBLE_LINES);
            oled_flush();
            needsUpdate = false;
        }

        usleep(5000);
    }

    if (changed)
        save_config();
}

/* Live reading shown under some settings, to help set them.
 * Returns false if the setting has none */
static bool setting_live_info(EditableVariable *v, float value, char *buf, size_t n) {
    if (strcmp(v->name, "platterspeed") == 0) {
        snprintf(buf, n, "Feels like a %.1f rpm record", value * 60.0f / 16384.0f);
        return true;
    }
    if (strcmp(v->name, "touch_threshold") == 0) {
        unsigned int raw = cap_raw;
        snprintf(buf, n, "Sensor: %u  %s", raw,
                 raw > (unsigned int)value ? "TOUCHED" : "not touched");
        return true;
    }
    if (strcmp(v->name, "faderopenpoint") == 0 ||
        strcmp(v->name, "faderclosepoint") == 0) {
        /* Distance of the fader from each end, out of 1023 */
        snprintf(buf, n, "Fader  L:%u  R:%u", ADCs[0], ADCs[1]);
        return true;
    }
    return false;
}

/* Draw text word-wrapped into lines of up to `cols` characters, from y
 * down to no further than max_y. Returns the y after the last line */
static int draw_wrapped(int x, int y, int cols, int line_h, int max_y,
                        const char *text, uint16_t color) {
    char line[40];
    const char *p = text;

    if (cols > (int)sizeof(line) - 1)
        cols = sizeof(line) - 1;

    while (*p && y + 8 <= max_y) {
        while (*p == ' ') p++;
        int len = strlen(p);
        int take = len;
        if (len > cols) {
            /* Break at the last space that fits */
            take = cols;
            while (take > 0 && p[take] != ' ') take--;
            if (take == 0) take = cols;   /* one very long word */
        }
        memcpy(line, p, take);
        line[take] = '\0';
        st7789_draw_string(x, y, line, color, FONT_SMALL);
        p += take;
        y += line_h;
    }
    return y;
}

/* The edit screen: name, big value, live reading, default and range,
 * then the plain-English help */
static void draw_setting_screen(EditableVariable *v, float value) {
    char buf[48], def[24];

    oled_clear();
    oled_draw_title_bar(v->label);

    /* Value: large if it fits, else medium */
    format_variable_value(v, value, buf, sizeof(buf));
    int font = st7789_string_width(buf, FONT_LARGE) <= DISPLAY_WIDTH - 8 ? FONT_LARGE : FONT_MEDIUM;
    int vw = st7789_string_width(buf, font);
    oled_text_color((DISPLAY_WIDTH - vw) / 2, font == FONT_LARGE ? 32 : 36,
                    buf, font, THEME_VALUE_FG);

    /* Live reading */
    if (setting_live_info(v, value, buf, sizeof(buf))) {
        int w = st7789_string_width(buf, FONT_SMALL);
        oled_text_color((DISPLAY_WIDTH - w) / 2, 62, buf, FONT_SMALL, THEME_DECK1_ACCENT);
    }

    /* Default (and range, for numbers) */
    format_variable_value(v, v->defaultValue, def, sizeof(def));
    if (v->kind == VAR_NUMBER) {
        snprintf(buf, sizeof(buf), "Default %s  (%.*f-%.*f)", def,
                 v->decimals, v->minValue, v->decimals, v->maxValue);
    } else {
        snprintf(buf, sizeof(buf), "Default: %s", def);
    }
    int w = st7789_string_width(buf, FONT_SMALL);
    if (w > DISPLAY_WIDTH - 8) {
        snprintf(buf, sizeof(buf), "Default: %s", def);
        w = st7789_string_width(buf, FONT_SMALL);
    }
    oled_text_color((DISPLAY_WIDTH - w) / 2, 76, buf, FONT_SMALL, THEME_RANGE_FG);

    st7789_hline(4, 89, DISPLAY_WIDTH - 8, THEME_SEPARATOR);

    /* Help, with the fader curve below it for the curve settings */
    bool curve = strcmp(v->name, "Fad Factor") == 0 || strcmp(v->name, "Fad Power") == 0;
    if (v->help)
        draw_wrapped(4, 95, 29, 10, curve ? 145 : 205, v->help, THEME_TEXT);
    if (curve)
        oled_draw_fader_curve(30, 150, 180, 52, cfg_curve_factor, cfg_curve_power);

    oled_text_color(4, 208, "KB0: keep   Knob push: undo", FONT_SMALL,
                    RGB565(110, 110, 110));
}

/* Edit one setting. The knob changes it (it takes effect at once), KB0
 * keeps the new value, a knob push puts the old one back.
 * Returns true if the value was changed and kept. */
static bool adjust_variable_value(EditableVariable *variable) {
    bool adjusting = true;
    bool kept = false;
    float value = *variable->valuePtr;
    float orig_value = value;
    unsigned long last_move = 0, last_live = 0;
    bool live = setting_live_info(variable, value, (char[48]){0}, 48);
    needsUpdate = true;

    while (adjusting) {
        int movement = rotary_encoder_moved();
        int button_press = rotary_button_pressed();
        int kb0 = kb0_button_pressed();
        unsigned long now = ui_millis();

        /* EC11 rotary = change value */
        if (movement != 0) {
            if (variable->kind == VAR_ONOFF) {
                value = value >= 0.5f ? 0.0f : 1.0f;
            } else if (variable->kind == VAR_CHOICE) {
                int lo = (int)variable->minValue, hi = (int)variable->maxValue;
                int span = hi - lo + 1;
                int iv = (int)lroundf(value) - lo;
                iv = ((iv + movement) % span + span) % span;
                value = (float)(iv + lo);
            } else {
                /* Spinning the knob quickly takes bigger steps, so wide
                 * ranges do not need hundreds of clicks */
                float steps = (variable->maxValue - variable->minValue) / variable->stepSize;
                int mult = 1;
                if (steps > 60 && now - last_move < 50) mult = 10;
                else if (steps > 60 && now - last_move < 110) mult = 4;
                last_move = now;

                value += movement * mult * variable->stepSize;
                /* Stay on whole steps, so float rounding never shows */
                value = roundf(value / variable->stepSize) * variable->stepSize;
                if (value < variable->minValue) value = variable->minValue;
                if (value > variable->maxValue) value = variable->maxValue;
            }

            pthread_mutex_lock(&variable->mutex);
            *variable->valuePtr = value;
            pthread_mutex_unlock(&variable->mutex);

            needsUpdate = true;
        }

        /* KB0 = confirm */
        if (kb0 == 1) {
            kept = value != orig_value;
            adjusting = false;
        }
        /* Rotary click = cancel (revert) */
        if (button_press == 1) {
            pthread_mutex_lock(&variable->mutex);
            *variable->valuePtr = orig_value;
            pthread_mutex_unlock(&variable->mutex);
            adjusting = false;
        }

        /* Settings with a live reading refresh it 20 times a second */
        if (live && now - last_live >= 50) {
            last_live = now;
            needsUpdate = true;
        }

        if (adjusting && needsUpdate) {
            draw_setting_screen(variable, value);
            oled_flush();
            needsUpdate = false;
        }

        usleep(5000);
    }
    needsUpdate = true;
    return kept;
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
    if (save_config())
        show_confirm_screen("Config Saved", "Press any button");
    else
        show_confirm_screen("Save Failed!", "Press any button");
}

static bool confirm_action(const char *question);

/* Reset all variables to defaults (the SC1000/SC500 values, or what
 * scsettings.txt says) and keep them */
static void action_reset_defaults(void) {
    if (!confirm_action("Reset all?"))
        return;
    reset_all_variables_to_defaults();
    save_config();
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
    save_config();

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
