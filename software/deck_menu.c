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
#include "alsa_mixer.h"

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
static void display_recording_status(struct deck *d, int deck_no);
static void handle_recording_navigation(struct deck *d, int deckno);
static void display_record_setup(struct deck *d, int deck_no);
static void handle_record_setup_navigation(struct deck *d, int deckno);
static void display_browse_folders(struct deck *d);
static void handle_browse_folders_navigation(struct deck *d, int deckno);
static void display_browse_files(struct deck *d);
static void handle_browse_files_navigation(struct deck *d, int deckno);
void adjust_pitch(struct deck *d, int deckno);
void action_randomize_both(struct deck *d, int deckno);

/* File browser state */
static struct Folder *browseFolder = NULL;   /* currently highlighted folder */
static struct File *browseFile = NULL;       /* currently highlighted file */
static int browseFolderIdx = 0;
static int browseFileIdx = 0;
static int browseFolderCount = 0;
static int browseFileCount = 0;
static int browseScrollOffset = 0;

/* Recording setup state */
static int setupSelectedSource = 0;
static int setupSelectedItem = 0;  /* 0=Source, 1=Record */

#define REC_SETUP_ITEMS 2  /* Source + Record */
static const char *rec_setup_control_names[] = {
    "Input Mux",                  /* enum: Line In / Mic */
    "Capture",                    /* capture volume 0-31 */
    "Mic Boost",                  /* volume 0-1 */
    "Output Mixer Line Bypass",   /* pswitch: passthrough */
};
#define REC_SETUP_NUM_CONTROLS 4
static MixerControl rec_mixer[REC_SETUP_NUM_CONTROLS];
void action_platter_speed(struct deck *d, int deckno);

/* Menu and state variables */
static DeckMenuState currentDeckMenuState = DECK_MENU_MAIN;
static int selectedItem = 0;
static int scrollOffset = 0;

/* Main deck menu items */
static MenuItem mainDeckMenuItems[] = {
    {"Load File", enter_load_file_menu},
    {"Start/Stop", action_start_stop},
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
            case DECK_MENU_ADJUST_PITCH:
                adjust_pitch(d, deck_no);
                break;
            case DECK_MENU_INFO:
                display_deck_info(d, deck_no);
                break;
            case DECK_MENU_BROWSE_FOLDERS:
                display_browse_folders(d);
                break;
            case DECK_MENU_BROWSE_FILES:
                display_browse_files(d);
                break;
            case DECK_MENU_RECORD_SETUP:
                display_record_setup(d, deck_no);
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
        case DECK_MENU_ADJUST_PITCH:
            adjust_pitch(d, deckno);
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
        case DECK_MENU_BROWSE_FOLDERS:
            handle_browse_folders_navigation(d, deckno);
            break;
        case DECK_MENU_BROWSE_FILES:
            handle_browse_files_navigation(d, deckno);
            break;
        case DECK_MENU_RECORD_SETUP:
            handle_record_setup_navigation(d, deckno);
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
    /* Enter folder browser -- count folders */
    browseFolder = d->FirstFolder;
    browseFolderIdx = 0;
    browseFolderCount = 0;
    browseScrollOffset = 0;
    struct Folder *f = d->FirstFolder;
    while (f) { browseFolderCount++; f = f->next; }

    fprintf(stderr, "enter_load_file_menu: FirstFolder=%p count=%d\n",
            (void*)d->FirstFolder, browseFolderCount);

    if (browseFolderCount > 0) {
        currentDeckMenuState = DECK_MENU_BROWSE_FOLDERS;
    } else {
        /* No folders, fall back to old menu */
        currentDeckMenuState = DECK_MENU_LOAD_FILE;
    }
    selectedItem = 0;
    scrollOffset = 0;
    needsUpdate = true;
}

void enter_adjust_volume(struct deck *d, int deckno) {
    currentDeckMenuState = DECK_MENU_ADJUST_VOLUME;
    needsUpdate = true;
}

void enter_adjust_pitch(struct deck *d, int deckno) {
    currentDeckMenuState = DECK_MENU_ADJUST_PITCH;
    needsUpdate = true;
}

void adjust_pitch(struct deck *d, int deckno) {
    static double orig_pitch = -1;
    static bool first_entry = true;
    int movement = rotary_encoder_moved();
    int button_press = rotary_button_pressed();
    int kb0 = kb0_button_pressed();
    double pitch = d->player.note_pitch;

    if (orig_pitch < 0) { orig_pitch = pitch; first_entry = true; }

    if (first_entry) {
        char val_str[16];
        snprintf(val_str, sizeof(val_str), "%.2f", pitch);
        oled_clear();
        oled_draw_value_screen("Pitch", val_str, "0.25 - 4.00");
        oled_flush();
        first_entry = false;
        needsUpdate = false;
    }

    if (movement != 0) {
        pitch += movement * 0.01;
        if (pitch < 0.25) pitch = 0.25;
        if (pitch > 4.0) pitch = 4.0;
        d->player.note_pitch = pitch;

        char val_str[16];
        snprintf(val_str, sizeof(val_str), "%.2f", pitch);
        oled_clear();
        oled_draw_value_screen("Pitch", val_str, "0.25 - 4.00");
        oled_flush();
        needsUpdate = false;
    }

    if (kb0 == 1) {
        orig_pitch = -1;
        currentDeckMenuState = DECK_MENU_MAIN;
        needsUpdate = true;
    }
    if (button_press == 1) {
        d->player.note_pitch = orig_pitch;
        orig_pitch = -1;
        currentDeckMenuState = DECK_MENU_MAIN;
        needsUpdate = true;
    }
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
    static bool first_entry = true;
    int movement = rotary_encoder_moved();
    int button_press = rotary_button_pressed();
    int kb0 = kb0_button_pressed();
    int vol_pct = (int)(d->player.setVolume * 100 + 0.5);

    if (orig_volume < 0) { orig_volume = d->player.setVolume; first_entry = true; }

    /* Draw on first entry */
    if (first_entry) {
        char val_str[16];
        snprintf(val_str, sizeof(val_str), "%d%%", vol_pct);
        oled_clear();
        oled_draw_value_screen("Volume", val_str, "0 - 100%");
        oled_flush();
        first_entry = false;
        needsUpdate = false;
    }

    if (movement != 0) {
        vol_pct += movement;
        if (vol_pct < 0) vol_pct = 0;
        if (vol_pct > 100) vol_pct = 100;
        d->player.setVolume = vol_pct / 100.0;

        char val_str[16];
        snprintf(val_str, sizeof(val_str), "%d%%", vol_pct);
        oled_clear();
        oled_draw_value_screen("Volume", val_str, "0 - 100%");
        oled_flush();
        needsUpdate = false;
    }

    if (kb0 == 1) {
        orig_volume = -1;
        currentDeckMenuState = DECK_MENU_MAIN;
        needsUpdate = true;
    }
    if (button_press == 1) {
        d->player.setVolume = orig_volume;
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

void action_randomize_both(struct deck *d, int deckno) {
    extern struct deck deck[2];
    deck_random_file(&deck[0]);
    deck_random_file(&deck[1]);
    currentDeckMenuState = DECK_MENU_MAIN;
    needsUpdate = true;
}

void action_next_folder(struct deck *d, int deckno) {
    trigger_io_event_no_param(ACTION_NEXTFOLDER, deckno);
    currentDeckMenuState = DECK_MENU_LOAD_FILE;
}

void action_prev_folder(struct deck *d, int deckno) {
    trigger_io_event_no_param(ACTION_PREVFOLDER, deckno);
    currentDeckMenuState = DECK_MENU_LOAD_FILE;
}

static void load_mixer_for_source(void) {
    MixerControl all_controls[20];
    int total = get_mixer_controls("default", all_controls, 20);
    for (int c = 0; c < REC_SETUP_NUM_CONTROLS; c++) {
        rec_mixer[c].isVolume = false;
        rec_mixer[c].isBoolean = false;
        rec_mixer[c].isEnum = false;
        snprintf(rec_mixer[c].name, sizeof(rec_mixer[c].name), "%s", rec_setup_control_names[c]);
        for (int j = 0; j < total; j++) {
            if (strcmp(all_controls[j].name, rec_setup_control_names[c]) == 0) {
                rec_mixer[c] = all_controls[j];
                break;
            }
        }
    }
}

void action_record(struct deck *d, int deckno) {
    /* Hardcode AudioInjector — no ALSA scan, instant entry */
    if (inputSourceCount == 0) {
        strncpy(inputSources[0].name, "AudioInjector", sizeof(inputSources[0].name));
        strncpy(inputSources[0].device, "hw:1,0", sizeof(inputSources[0].device));
        inputSourceCount = 1;
        setupSelectedSource = 0;
    }

    setupSelectedItem = 1;  /* start on RECORD button */
    currentDeckMenuState = DECK_MENU_RECORD_SETUP;
    needsUpdate = true;
}

/* action_select_input_source removed — source is now inline in setup screen */

static void action_start_recording(struct deck *d, int deckno) {
    InputSource *source = &inputSources[setupSelectedSource];

    char filename[256];
    snprintf(filename, sizeof(filename), "/tmp/rec%06d.raw", nextRecordingNumber++);

    printf("Deck %d: Recording from %s (%s)...\n", deckno + 1,
           source->name, source->device);

    /* Release the setup-screen peak meter handle before opening capture. */
    close_input_peak_monitor();

    if (start_recording(&recordingContext, source->device, filename) == 0) {
        recordingStartTime = gpio_millis();
        currentDeckMenuState = DECK_MENU_RECORDING;
        needsUpdate = true;
    } else {
        printf("Failed to start recording.\n");
        needsUpdate = true;
    }
}

/* handle_record_input_sources_navigation removed — source is inline in setup */

static void handle_recording_navigation(struct deck *d, int deckno) {
    int button_press = rotary_button_pressed();
    int kb0 = kb0_button_pressed();

    /* KB0 = stop recording, back to setup screen */
    if (kb0 == 1) {
        printf("Deck %d: Stopping recording.\n", deckno + 1);
        stop_recording(d, &recordingContext);
        currentDeckMenuState = DECK_MENU_RECORD_SETUP;
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

/* ── File Browser ─────────────────────────────────────────────── */

/* Helper: extract just the folder name from full path */
static const char *folder_basename(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

/* Helper: extract just the filename from full path */
static const char *file_basename(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void display_browse_folders(struct deck *d) {
    oled_clear();
    oled_draw_title_bar("Folders");

    /* Build label list from folder linked list */
    #define MAX_BROWSE 30
    const char *labels[MAX_BROWSE];
    struct Folder *f = d->FirstFolder;
    int count = 0;
    while (f && count < MAX_BROWSE) {
        labels[count] = folder_basename(f->FullPath);
        count++;
        f = f->next;
    }

    browseScrollOffset = oled_compute_scroll(browseFolderIdx, browseScrollOffset, MENU_VISIBLE_LINES);
    oled_draw_menu_list(labels, count, browseFolderIdx, browseScrollOffset, MENU_VISIBLE_LINES);

    /* Show current folder file count at bottom */
    if (browseFolder) {
        char info[40];
        int fc = 0;
        struct File *fi = browseFolder->FirstFile;
        while (fi) { fc++; fi = fi->next; }
        snprintf(info, sizeof(info), "%d files", fc);
        int iw = st7789_string_width(info, FONT_SMALL);
        oled_text_color(DISPLAY_WIDTH - iw - 4, DISPLAY_HEIGHT - 12, info,
                        FONT_SMALL, RGB565(100, 100, 100));
    }
    oled_flush();
}

static void handle_browse_folders_navigation(struct deck *d, int deckno) {
    int movement = rotary_encoder_moved();
    int button_press = rotary_button_pressed();
    int kb0 = kb0_button_pressed();

    if (movement != 0) {
        browseFolderIdx = (browseFolderIdx + movement + browseFolderCount) % browseFolderCount;
        /* Walk to the folder at this index */
        browseFolder = d->FirstFolder;
        for (int i = 0; i < browseFolderIdx && browseFolder; i++)
            browseFolder = browseFolder->next;
        needsUpdate = true;
    }

    /* KB0 = enter folder → show files */
    if (kb0 == 1 && browseFolder) {
        browseFile = browseFolder->FirstFile;
        browseFileIdx = 0;
        browseFileCount = 0;
        browseScrollOffset = 0;
        struct File *fi = browseFolder->FirstFile;
        while (fi) { browseFileCount++; fi = fi->next; }
        currentDeckMenuState = DECK_MENU_BROWSE_FILES;
        needsUpdate = true;
    }

    /* Rotary click = back to deck menu */
    if (button_press == 1) {
        currentDeckMenuState = DECK_MENU_MAIN;
        selectedItem = 0;
        scrollOffset = 0;
        needsUpdate = true;
    }
}

static void display_browse_files(struct deck *d) {
    oled_clear();
    char title[32];
    snprintf(title, sizeof(title), "%s", folder_basename(browseFolder->FullPath));
    oled_draw_title_bar(title);

    /* Build label list from file linked list */
    const char *labels[MAX_BROWSE];
    struct File *fi = browseFolder->FirstFile;
    int count = 0;
    while (fi && count < MAX_BROWSE) {
        labels[count] = file_basename(fi->FullPath);
        count++;
        fi = fi->next;
    }

    browseScrollOffset = oled_compute_scroll(browseFileIdx, browseScrollOffset, MENU_VISIBLE_LINES);

    /* Draw custom list with playing indicator */
    int y = 26;
    for (int i = browseScrollOffset; i < count && i < browseScrollOffset + MENU_VISIBLE_LINES; i++) {
        bool sel = (i == browseFileIdx);
        bool playing = false;

        /* Check if this file is the currently loaded one */
        struct File *check = browseFolder->FirstFile;
        for (int j = 0; j < i && check; j++) check = check->next;
        if (check && d->CurrentFile && strcmp(check->FullPath, d->CurrentFile->FullPath) == 0)
            playing = true;

        if (sel)
            oled_fill_rect(0, y, DISPLAY_WIDTH, 16, THEME_SELECT_BG);

        /* Playing indicator */
        if (playing)
            oled_text_color(2, y + 1, ">", FONT_SMALL, COLOR_GREEN);

        oled_text_color(12, y + 1, labels[i], FONT_SMALL,
                        sel ? COLOR_WHITE : RGB565(180, 180, 180));
        y += 16;
    }

    /* File count at bottom */
    char info[32];
    snprintf(info, sizeof(info), "%d/%d", browseFileIdx + 1, count);
    int iw = st7789_string_width(info, FONT_SMALL);
    oled_text_color(DISPLAY_WIDTH - iw - 4, DISPLAY_HEIGHT - 12, info,
                    FONT_SMALL, RGB565(100, 100, 100));

    oled_flush();
}

static void handle_browse_files_navigation(struct deck *d, int deckno) {
    int movement = rotary_encoder_moved();
    int button_press = rotary_button_pressed();
    int kb0 = kb0_button_pressed();

    if (movement != 0) {
        browseFileIdx = (browseFileIdx + movement + browseFileCount) % browseFileCount;
        /* Walk to the file at this index */
        browseFile = browseFolder->FirstFile;
        for (int i = 0; i < browseFileIdx && browseFile; i++)
            browseFile = browseFile->next;
        needsUpdate = true;
    }

    /* KB0 = load selected file */
    if (kb0 == 1 && browseFile) {
        d->CurrentFolder = browseFolder;
        d->CurrentFile = browseFile;
        load_track(d, track_acquire_by_import(d->importer, browseFile->FullPath));
        needsUpdate = true;
    }

    /* Rotary click = back to folder list */
    if (button_press == 1) {
        browseScrollOffset = 0;
        currentDeckMenuState = DECK_MENU_BROWSE_FOLDERS;
        needsUpdate = true;
    }
}

/* ── Recording Setup Screen (combined source + controls) ─────── */

static void display_record_setup(struct deck *d, int deck_no) {
    oled_clear();
    oled_draw_title_bar("Record");

    /* Item 0: Source */
    int y = 60;
    {
        bool sel = (setupSelectedItem == 0);
        if (sel) oled_fill_rect(0, y, DISPLAY_WIDTH, 20, THEME_SELECT_BG);
        oled_text_color(8, y + 4, "Source:", FONT_SMALL,
                        sel ? COLOR_WHITE : RGB565(160, 160, 160));
        const char *sname = (inputSourceCount > 0)
            ? inputSources[setupSelectedSource].device : "None";
        int sw = st7789_string_width(sname, FONT_SMALL);
        oled_text_color(DISPLAY_WIDTH - sw - 8, y + 4, sname, FONT_SMALL,
                        THEME_VALUE_FG);
    }
    y += 30;

    /* Item 1: RECORD button — big and centered */
    {
        bool sel = (setupSelectedItem == 1);
        uint16_t bg = sel ? COLOR_RED : RGB565(80, 0, 0);
        uint16_t fg = sel ? COLOR_WHITE : RGB565(160, 160, 160);
        oled_fill_rect(30, y, DISPLAY_WIDTH - 60, 36, bg);
        int tw = st7789_string_width("RECORD", FONT_LARGE);
        oled_text_color((DISPLAY_WIDTH - tw) / 2, y + 6, "RECORD", FONT_LARGE, fg);
    }

    /* Instructions */
    oled_text_color(40, 180, "KB0=select  ROT=back", FONT_SMALL,
                    RGB565(100, 100, 100));

    oled_flush();
    needsUpdate = false;
}

static void handle_record_setup_navigation(struct deck *d, int deckno) {
    int movement = rotary_encoder_moved();
    int button_press = rotary_button_pressed();
    int kb0 = kb0_button_pressed();

    if (movement != 0) {
        setupSelectedItem = (setupSelectedItem + movement + REC_SETUP_ITEMS) % REC_SETUP_ITEMS;
        needsUpdate = true;
    }

    if (kb0 == 1) {
        if (setupSelectedItem == 0) {
            /* Source: cycle */
            setupSelectedSource = (setupSelectedSource + 1) % inputSourceCount;
        } else {
            /* RECORD */
            action_start_recording(d, deckno);
            return;
        }
        needsUpdate = true;
    }

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
