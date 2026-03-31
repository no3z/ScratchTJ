#include "deck_menu.h"
#include "lcd_menu.h"
#include "deck.h"
#include "player.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "track.h"
#include "cues.h"
#include "recording.h"
#include "shared_variables.h"

#define MAX_INPUT_SOURCES 10

InputSource inputSources[MAX_INPUT_SOURCES];
int inputSourceCount = 0;
static int nextRecordingNumber = 0;

RecordingContext recordingContext;
static unsigned long recordingStartTime = 0;

bool jogPitchModeEnabled;

extern bool needsUpdate;
extern MainMenuState mainMenuState;

/* Forward declarations */
static void display_input_sources_menu(InputSource *sources, int sourceCount,
                                       int selected, const char *title);
static void display_recording_status(struct deck *d, int deck_no);
static void handle_record_input_sources_navigation(struct deck *d, int deckno);
static void handle_recording_navigation(struct deck *d, int deckno);
static void action_select_input_source(struct deck *d, int deckno);
void action_platter_speed(struct deck *d, int deckno);

/* Menu and state variables */
static DeckMenuState currentDeckMenuState = DECK_MENU_MAIN;
static int selectedItem = 0;
static int scrollOffset = 0;

/* Main deck menu items */
static MenuItem mainDeckMenuItems[] = {
    {"Load File", enter_load_file_menu},
    {"Settings", enter_settings_menu},
    {"Info", enter_deck_info_display},
};
static int mainDeckMenuSize = sizeof(mainDeckMenuItems) / sizeof(MenuItem);

/* Load file menu items */
static MenuItem loadFileMenuItems[] = {
    {"Start/Stop", action_start_stop},
    {"Next File", action_next_file},
    {"Previous File", action_prev_file},
    {"Random File", action_random_file},
    {"Next Folder", action_next_folder},
    {"Previous Folder", action_prev_folder},
    {"Record", action_record},
};
static int loadFileMenuSize = sizeof(loadFileMenuItems) / sizeof(MenuItem);

/* Settings menu items */
static MenuItem settingsMenuItems[] = {
    {"Jog Pitch Mode", action_jog_pitch},
    {"Toggle Jog Reverse", action_jog_reverse},
    {"Platter Speed", action_platter_speed},
};
static int settingsMenuSize = sizeof(settingsMenuItems) / sizeof(MenuItem);

/* Helper: extract labels from MenuItem array for oled_draw_menu_list */
static const char *labelBuf[16];
static const char **extract_labels(MenuItem *items, int count) {
    for (int i = 0; i < count && i < 16; i++)
        labelBuf[i] = items[i].label;
    return labelBuf;
}

/* Display a generic menu with title bar and scrollable list */
void display_menu(MenuItem *menuItems, int menuSize, int selected, const char *title) {
    scrollOffset = oled_compute_scroll(selected, scrollOffset, MENU_VISIBLE_LINES);
    oled_clear();
    oled_draw_title_bar(title);
    oled_draw_menu_list(extract_labels(menuItems, menuSize),
                        menuSize, selected, scrollOffset, MENU_VISIBLE_LINES);
    oled_flush();
}

char title[32];
void display_deck_menu(struct deck *d, int deck_no) {
    if (needsUpdate) {
        switch (currentDeckMenuState) {
            case DECK_MENU_MAIN:
                snprintf(title, sizeof(title), "Deck %d", deck_no + 1);
                display_menu(mainDeckMenuItems, mainDeckMenuSize, selectedItem, title);
                break;
            case DECK_MENU_LOAD_FILE:
                display_menu(loadFileMenuItems, loadFileMenuSize, selectedItem, "Load File");
                break;
            case DECK_MENU_SETTINGS:
                display_menu(settingsMenuItems, settingsMenuSize, selectedItem, "Settings");
                break;
            case DECK_MENU_ADJUST_VOLUME:
                adjust_volume(d, deck_no);
                break;
            case DECK_MENU_INFO:
                display_deck_info(d, deck_no);
                break;
            case DECK_MENU_RECORD_INPUT_SOURCE:
                display_input_sources_menu(inputSources, inputSourceCount, selectedItem, "Select Input");
                break;
            case DECK_MENU_RECORDING:
                display_recording_status(d, deck_no);
                break;
            default:
                break;
        }
        if (currentDeckMenuState == DECK_MENU_RECORDING)
            needsUpdate = true;  /* keep redrawing for live timer */
        else
            needsUpdate = false;
    }
}

void deck_menu_reset(void) {
    currentDeckMenuState = DECK_MENU_MAIN;
    selectedItem = 0;
    scrollOffset = 0;
}

/* Handle Deck Menu Navigation */
void handle_deck_menu_navigation(struct deck *d, int deckno) {
    switch (currentDeckMenuState) {
        case DECK_MENU_MAIN:
            handle_menu_navigation(d, deckno, mainDeckMenuItems, mainDeckMenuSize);
            break;
        case DECK_MENU_LOAD_FILE:
            handle_menu_navigation(d, deckno, loadFileMenuItems, loadFileMenuSize);
            break;
        case DECK_MENU_SETTINGS:
            handle_menu_navigation(d, deckno, settingsMenuItems, settingsMenuSize);
            break;
        case DECK_MENU_ADJUST_VOLUME:
            adjust_volume(d, deckno);
            break;
        case DECK_MENU_INFO: {
            rotary_encoder_moved(); /* consume */
            int bp = rotary_button_pressed();
            int kb = kb0_button_pressed();
            if (bp == 1 || kb == 1) {
                currentDeckMenuState = DECK_MENU_MAIN;
                needsUpdate = true;
            }
            break;
        }
        case DECK_MENU_RECORD_INPUT_SOURCE:
            handle_record_input_sources_navigation(d, deckno);
            break;
        case DECK_MENU_RECORDING:
            handle_recording_navigation(d, deckno);
            break;
        default:
            break;
    }
}

/* Handle menu navigation (generic) */
void handle_menu_navigation(struct deck *d, int deckno, MenuItem *menuItems, int menuSize) {
    int encoder_movement = rotary_encoder_moved();
    int button_press = rotary_button_pressed();
    int kb0 = kb0_button_pressed();

    if (encoder_movement != 0) {
        selectedItem = (selectedItem + encoder_movement + menuSize) % menuSize;
        needsUpdate = true;
    }

    /* KB0 = select */
    if (kb0 == 1) {
        menuItems[selectedItem].action(d, deckno);
        needsUpdate = true;
    }

    /* Rotary click = back */
    if (button_press == 1) {
        if (currentDeckMenuState == DECK_MENU_MAIN) {
            mainMenuState = MENU_MAIN;
        }
        currentDeckMenuState = DECK_MENU_MAIN;
        selectedItem = 0;
        scrollOffset = 0;
        needsUpdate = true;
    }
}

/* Menu actions */
void enter_load_file_menu(struct deck *d, int deckno) {
    currentDeckMenuState = DECK_MENU_LOAD_FILE;
    selectedItem = 0;
    scrollOffset = 0;
    needsUpdate = true;
}

void enter_adjust_volume(struct deck *d, int deckno) {
    currentDeckMenuState = DECK_MENU_ADJUST_VOLUME;
    needsUpdate = true;
}

void enter_settings_menu(struct deck *d, int deckno) {
    currentDeckMenuState = DECK_MENU_SETTINGS;
    selectedItem = 0;
    scrollOffset = 0;
    needsUpdate = true;
}

void enter_deck_info_display(struct deck *d, int deckno) {
    currentDeckMenuState = DECK_MENU_INFO;
    needsUpdate = true;
}

void adjust_volume(struct deck *d, int deckno) {
    static double orig_volume = -1;
    int movement = rotary_encoder_moved();
    int button_press = rotary_button_pressed();
    int kb0 = kb0_button_pressed();
    double new_volume = d->player.setVolume * 100;

    /* Save original on first entry */
    if (orig_volume < 0) orig_volume = new_volume;

    /* EC11 rotary = change value */
    if (movement != 0) {
        new_volume = new_volume + movement;
        if (new_volume < 0) new_volume = 0;
        if (new_volume > 127) new_volume = 127;

        d->player.setVolume = new_volume / 100;
        player_set_volume(&d->player, new_volume);
        trigger_io_event(ACTION_VOLUME, deckno, (unsigned char)new_volume);

        char val_str[16];
        snprintf(val_str, sizeof(val_str), "%d", (int)new_volume);

        oled_clear();
        oled_draw_value_screen("Volume", val_str, "0 - 127");
        oled_flush();
        needsUpdate = false;
    }

    /* KB0 = confirm */
    if (kb0 == 1) {
        orig_volume = -1;
        currentDeckMenuState = DECK_MENU_MAIN;
        needsUpdate = true;
    }
    /* Rotary click = cancel (revert) */
    if (button_press == 1) {
        d->player.setVolume = orig_volume / 100;
        player_set_volume(&d->player, orig_volume);
        trigger_io_event(ACTION_VOLUME, deckno, (unsigned char)orig_volume);
        orig_volume = -1;
        currentDeckMenuState = DECK_MENU_MAIN;
        needsUpdate = true;
    }
}

/* Actions for load file menu */
void action_start_stop(struct deck *d, int deckno) {
    trigger_io_event_no_param(ACTION_STARTSTOP, deckno);
    currentDeckMenuState = DECK_MENU_LOAD_FILE;
}

void action_next_file(struct deck *d, int deckno) {
    trigger_io_event_no_param(ACTION_NEXTFILE, deckno);
    currentDeckMenuState = DECK_MENU_LOAD_FILE;
}

void action_prev_file(struct deck *d, int deckno) {
    trigger_io_event_no_param(ACTION_PREVFILE, deckno);
    currentDeckMenuState = DECK_MENU_LOAD_FILE;
}

void action_random_file(struct deck *d, int deckno) {
    trigger_io_event_no_param(ACTION_RANDOMFILE, deckno);
    currentDeckMenuState = DECK_MENU_LOAD_FILE;
}

void action_next_folder(struct deck *d, int deckno) {
    trigger_io_event_no_param(ACTION_NEXTFOLDER, deckno);
    currentDeckMenuState = DECK_MENU_LOAD_FILE;
}

void action_prev_folder(struct deck *d, int deckno) {
    trigger_io_event_no_param(ACTION_PREVFOLDER, deckno);
    currentDeckMenuState = DECK_MENU_LOAD_FILE;
}

void action_record(struct deck *d, int deckno) {
    inputSourceCount = get_available_input_sources(inputSources, MAX_INPUT_SOURCES);

    if (inputSourceCount == 0) {
        printf("No input sources available.\n");
        currentDeckMenuState = DECK_MENU_LOAD_FILE;
        needsUpdate = true;
        return;
    }

    currentDeckMenuState = DECK_MENU_RECORD_INPUT_SOURCE;
    selectedItem = 0;
    scrollOffset = 0;
    needsUpdate = true;
}

static void action_select_input_source(struct deck *d, int deckno) {
    InputSource *selectedSource = &inputSources[selectedItem];

    char filename[256];
    snprintf(filename, sizeof(filename), "/tmp/rec%06d.raw", nextRecordingNumber++);

    printf("Deck %d: Recording from %s (%s)...\n", deckno + 1,
           selectedSource->name, selectedSource->device);

    if (start_recording(&recordingContext, selectedSource->device, filename) == 0) {
        recordingStartTime = gpio_millis();
        currentDeckMenuState = DECK_MENU_RECORDING;
        needsUpdate = true;
    } else {
        printf("Failed to start recording.\n");
        currentDeckMenuState = DECK_MENU_RECORD_INPUT_SOURCE;
        needsUpdate = true;
    }
}

static void handle_record_input_sources_navigation(struct deck *d, int deckno) {
    int encoder_movement = rotary_encoder_moved();
    int button_press = rotary_button_pressed();
    int kb0 = kb0_button_pressed();

    if (encoder_movement != 0) {
        selectedItem = (selectedItem + encoder_movement + inputSourceCount) % inputSourceCount;
        needsUpdate = true;
    }

    /* KB0 = select input source */
    if (kb0 == 1) {
        action_select_input_source(d, deckno);
        needsUpdate = true;
    }

    /* Rotary click = back */
    if (button_press == 1) {
        if (mainMenuState == MENU_RECORD) {
            mainMenuState = MENU_MAIN;
            currentDeckMenuState = DECK_MENU_MAIN;
        } else {
            currentDeckMenuState = DECK_MENU_LOAD_FILE;
        }
        selectedItem = 0;
        scrollOffset = 0;
        needsUpdate = true;
    }
}

static void handle_recording_navigation(struct deck *d, int deckno) {
    int button_press = rotary_button_pressed();
    int kb0 = kb0_button_pressed();

    /* KB0 = stop recording, stay in source select */
    if (kb0 == 1) {
        printf("Deck %d: Stopping recording.\n", deckno + 1);
        stop_recording(d, &recordingContext);
        currentDeckMenuState = DECK_MENU_RECORD_INPUT_SOURCE;
        needsUpdate = true;
    }
    /* Rotary click = stop and go back */
    if (button_press == 1) {
        stop_recording(d, &recordingContext);
        if (mainMenuState == MENU_RECORD) {
            mainMenuState = MENU_MAIN;
            currentDeckMenuState = DECK_MENU_MAIN;
        } else {
            currentDeckMenuState = DECK_MENU_LOAD_FILE;
        }
        selectedItem = 0;
        scrollOffset = 0;
        needsUpdate = true;
    }
}

void action_jog_pitch(struct deck *d, int deckno) {
    jogPitchModeEnabled = !jogPitchModeEnabled;
    printf("Deck %d: Jog Pitch Mode %s\n", deckno + 1,
           jogPitchModeEnabled ? "Enabled" : "Disabled");
    if (jogPitchModeEnabled)
        trigger_io_event_no_param(ACTION_JOGPIT, deckno);
    else
        trigger_io_event_no_param(ACTION_JOGPSTOP, deckno);
    currentDeckMenuState = DECK_MENU_SETTINGS;
    needsUpdate = true;
}

void action_jog_reverse(struct deck *d, int deckno) {
    printf("Deck %d: Triggering Jog Reverse...\n", deckno + 1);
    trigger_io_event_no_param(ACTION_JOGREVERSE, deckno);
    currentDeckMenuState = DECK_MENU_SETTINGS;
}

void action_platter_speed(struct deck *d, int deckno) {
    float value;
    if (!get_variable_value("platterspeed", &value))
        value = 9100.0f;
    float orig_value = value;

    needsUpdate = true;
    bool adjusting = true;

    /* Find the variable pointer once */
    float *ptr = NULL;
    int count = 0;
    EditableVariable *vars = get_editable_variables(&count);
    for (int i = 0; i < count; i++) {
        if (strcmp(vars[i].name, "platterspeed") == 0) {
            ptr = vars[i].valuePtr;
            break;
        }
    }

    while (adjusting) {
        int movement = rotary_encoder_moved();
        int bp = rotary_button_pressed();
        int kb0 = kb0_button_pressed();

        /* EC11 rotary = change value */
        if (movement != 0) {
            value += movement * 256.0f;
            if (value < 1.0f) value = 1.0f;
            if (value > 32768.0f) value = 32768.0f;
            if (ptr) *ptr = value;
            needsUpdate = true;
        }

        /* KB0 = confirm */
        if (kb0 == 1) {
            adjusting = false;
        }
        /* Rotary click = cancel (revert) */
        if (bp == 1) {
            if (ptr) *ptr = orig_value;
            adjusting = false;
        }

        if (needsUpdate) {
            char val_str[16], range_str[32];
            snprintf(val_str, sizeof(val_str), "%.0f", value);
            snprintf(range_str, sizeof(range_str), "1 - 32768");
            oled_clear();
            oled_draw_value_screen("Platter Speed", val_str, range_str);
            oled_flush();
            needsUpdate = false;
        }
        usleep(5000);
    }
    currentDeckMenuState = DECK_MENU_SETTINGS;
    needsUpdate = true;
}

/* Deck info display - enhanced with filename, progress, cues (Improvement 2) */
void display_deck_info(struct deck *d, int deckno) {
    oled_clear();
    uint16_t accent = (deckno == 0) ? THEME_DECK1_ACCENT : THEME_DECK2_ACCENT;

    char title[24];
    snprintf(title, sizeof(title), "Deck %d Info", deckno + 1);
    oled_draw_title_bar(title);

    int y = 28;
    char buf[48];

    /* Filename */
    const char *fname = "No track";
    const char *folder = "";
    static char folder_buf[64];
    if (d->player.track && d->player.track->path) {
        const char *slash = strrchr(d->player.track->path, '/');
        fname = slash ? slash + 1 : d->player.track->path;
        if (slash) {
            int len = (int)(slash - d->player.track->path);
            if (len >= (int)sizeof(folder_buf)) len = sizeof(folder_buf) - 1;
            memcpy(folder_buf, d->player.track->path, len);
            folder_buf[len] = '\0';
            folder = folder_buf;
        }
    }
    oled_text_color(4, y, fname, FONT_MEDIUM, accent);
    y += 18;
    oled_text_color(4, y, folder, FONT_SMALL, RGB565(120, 120, 120));
    y += 14;

    /* Position + progress bar */
    double elapsed = d->player.track ? player_get_elapsed(&d->player) : 0.0;
    double duration = 0.0;
    if (d->player.track && d->player.track->rate > 0)
        duration = (double)d->player.track->length / d->player.track->rate;
    float progress = (duration > 0) ? (float)(elapsed / duration) : 0.0f;

    char e[16], t[16];
    oled_format_time(elapsed, e, sizeof(e));
    oled_format_time(duration, t, sizeof(t));
    snprintf(buf, sizeof(buf), "Position  %s / %s", e, t);
    oled_text(4, y, buf, FONT_SMALL);
    y += 12;
    oled_draw_progress_bar(4, y, DISPLAY_WIDTH - 8, 8, progress, accent);
    y += 14;

    /* Pitch */
    snprintf(buf, sizeof(buf), "Pitch     %.2fx", d->player.pitch);
    oled_text(4, y, buf, FONT_SMALL);
    y += 14;

    /* Volume */
    snprintf(buf, sizeof(buf), "Volume    %.0f", d->player.setVolume * 100);
    oled_text(4, y, buf, FONT_SMALL);
    y += 14;

    /* Touch (per-deck effective state) */
    snprintf(buf, sizeof(buf), "Touch     %s",
             d->player.capTouch ? "ON" : "OFF");
    oled_text(4, y, buf, FONT_SMALL);
    y += 14;

    /* Motor */
    snprintf(buf, sizeof(buf), "Motor     %.2f", d->player.motor_speed);
    oled_text(4, y, buf, FONT_SMALL);
    y += 18;

    /* Inline cue status */
    char cue_line[60] = "Cues:";
    for (int i = 0; i < 4; i++) {
        double pos = cues_get(&d->cues, i);
        char cs[24];
        if (pos != CUE_UNSET) {
            char ct[16];
            oled_format_time(pos, ct, sizeof(ct));
            snprintf(cs, sizeof(cs), " [%d %s]", i + 1, ct);
        } else {
            snprintf(cs, sizeof(cs), " [%d   ]", i + 1);
        }
        strncat(cue_line, cs, sizeof(cue_line) - strlen(cue_line) - 1);
    }
    oled_text(4, y, cue_line, FONT_SMALL);

    oled_flush();
}

static void display_input_sources_menu(InputSource *sources, int sourceCount,
                                       int selected, const char *title) {
    /* Build label array from source names */
    const char *labels[MAX_INPUT_SOURCES];
    for (int i = 0; i < sourceCount && i < MAX_INPUT_SOURCES; i++)
        labels[i] = sources[i].name;

    scrollOffset = oled_compute_scroll(selected, scrollOffset, MENU_VISIBLE_LINES);
    oled_clear();
    oled_draw_title_bar(title);
    oled_draw_menu_list(labels, sourceCount, selected, scrollOffset, MENU_VISIBLE_LINES);
    oled_flush();
}

static void display_recording_status(struct deck *d, int deck_no) {
    oled_clear();
    oled_draw_title_bar("Recording");

    /* Timer */
    double secs = (gpio_millis() - recordingStartTime) / 1000.0;
    char tbuf[16];
    oled_format_time(secs, tbuf, sizeof(tbuf));
    int tw = st7789_string_width(tbuf, FONT_LARGE);
    oled_text_color((DISPLAY_WIDTH - tw) / 2, 80, tbuf, FONT_LARGE,
                    COLOR_RED);

    /* Blinking dot */
    if (((int)(secs * 2)) & 1)
        oled_fill_rect(DISPLAY_WIDTH / 2 - 4, 120, 8, 8, COLOR_RED);

    int sw = st7789_string_width("KB0=stop  ROT=back", FONT_SMALL);
    oled_text_color((DISPLAY_WIDTH - sw) / 2, 180, "KB0=stop  ROT=back",
                    FONT_SMALL, RGB565(130, 130, 130));
    oled_flush();
}
