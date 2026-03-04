#include "deck_menu.h"
#include "lcd_menu.h"
#include "deck.h"
#include "player.h"
#include "cues.h"
#include "alsa_mixer.h"
#include <unistd.h>
#include <string.h>
#include "track.h"
#include "recording.h"

#define RECORD_CAPTURE_DEVICE "plughw:1,0"
#define RECORD_MIXER_CARD "default"

static int nextRecordingNumber = 0;
static RecordingContext recordingContext;

// Capture source enum from ALSA mixer (e.g., Mic, Line In, etc.)
static MixerControl captureSourceControl;
static bool captureSourceFound = false;

bool jogPitchModeEnabled;
// Externals
extern bool needsUpdate;
extern MainMenuState mainMenuState;

// Menu and State Variables
static DeckMenuState currentDeckMenuState = DECK_MENU_MAIN;
static int selectedItem = 0;
static int recordSelectedSource = 0;

// Cue labels for the two CUE screen buttons
#define CUE_LABEL_SEL 0
#define CUE_LABEL_BCK 1
#define CUE_LONG_PRESS_MS 1500

// Main deck menu items
static MenuItem mainDeckMenuItems[] = {
    {"1. Load File", enter_load_file_menu},
    {"2. CUE", enter_cue_menu},
    {"3. Record", enter_record_menu},
    {"4. Settings", enter_settings_menu},
    {"5. Info", enter_deck_info_display},
};

static int mainDeckMenuSize = sizeof(mainDeckMenuItems) / sizeof(MenuItem);

// Load file menu items
static MenuItem loadFileMenuItems[] = {
    {"1. Start/Stop", action_start_stop},
    {"2. Next File", action_next_file},
    {"3. Previous File", action_prev_file},
    {"4. Random File", action_random_file},
    {"5. Next Folder", action_next_folder},
    {"6. Previous Folder", action_prev_folder},
};

static int loadFileMenuSize = sizeof(loadFileMenuItems) / sizeof(MenuItem);

// Settings menu items
static MenuItem settingsMenuItems[] = {
    {"1. Jog Pitch Mode", action_jog_pitch},
    {"2. Toggle Jog Reverse", action_jog_reverse},
};

static int settingsMenuSize = sizeof(settingsMenuItems) / sizeof(MenuItem);

// Function Definitions

char title[32]; // Buffer for the title string
void display_deck_menu(struct deck *d, int deck_no) {
    if (needsUpdate) {
        switch (currentDeckMenuState) {
            case DECK_MENU_MAIN:

                snprintf(title, sizeof(title), "Deck %d Menu", deck_no+1); // Format the title with deck_no

                display_menu(mainDeckMenuItems, mainDeckMenuSize, selectedItem, title);
                break;
            case DECK_MENU_LOAD_FILE:
                display_menu(loadFileMenuItems, loadFileMenuSize, selectedItem, "Load File");
                break;
            case DECK_MENU_CUE:
                display_cue_screen(d, deck_no);
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
                display_record_source_screen();
                break;
            case DECK_MENU_RECORDING:
                display_recording_status(d, deck_no);
                break;
            default:
                break;
        }
        needsUpdate = false;
    }
}

void deck_menu_reset(void) {
    currentDeckMenuState = DECK_MENU_MAIN;
    selectedItem = 0;
}

// Handle Deck Menu Navigation
void handle_deck_menu_navigation(struct deck *d, int deckno) {
    switch (currentDeckMenuState) {
        case DECK_MENU_MAIN:
            handle_menu_navigation(d, deckno, mainDeckMenuItems, mainDeckMenuSize);
            break;
        case DECK_MENU_LOAD_FILE:
            handle_menu_navigation(d, deckno, loadFileMenuItems, loadFileMenuSize);
            break;
        case DECK_MENU_CUE:
            handle_cue_navigation(d, deckno);
            break;
        case DECK_MENU_SETTINGS:
            handle_menu_navigation(d, deckno, settingsMenuItems, settingsMenuSize);
            break;
        case DECK_MENU_ADJUST_VOLUME:
            adjust_volume(d, deckno);
            break;
        case DECK_MENU_INFO:
            if (rotary_button_pressed() == 1 || rotary_button_pressed() == 2) {
                currentDeckMenuState = DECK_MENU_MAIN;
                needsUpdate = true;
            }
            break;
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

// Function to display a menu
void display_menu(MenuItem *menuItems, int menuSize, int selectedItem, const char *title) {
    lcdClear(lcdHandle);
    lcdPosition(lcdHandle, 0, 0);
    lcdPrintf(lcdHandle, "%s", title);
    lcdPosition(lcdHandle, 0, 1);
    lcdPrintf(lcdHandle, "%s", menuItems[selectedItem].label);
}

// Function to handle menu navigation
void handle_menu_navigation(struct deck *d, int deckno, MenuItem *menuItems, int menuSize) {
    int encoder_movement = rotary_encoder_moved();
    int button_press = rotary_button_pressed();

    if (encoder_movement != 0) {
        selectedItem = (selectedItem + encoder_movement + menuSize) % menuSize;
        needsUpdate = true;
    }

    if (button_press == 1) { // Short press to select an option
        menuItems[selectedItem].action(d, deckno);
        needsUpdate = true;
    }

if (button_press == 2) { // Long press to go back
    if (currentDeckMenuState == DECK_MENU_MAIN) {
        // Exit to main menu
        mainMenuState = MENU_MAIN;
        currentDeckMenuState = DECK_MENU_MAIN;
        selectedItem = 0;
        needsUpdate = true;
    } else {
        // Go back to deck main menu
        currentDeckMenuState = DECK_MENU_MAIN;
        selectedItem = 0;
        needsUpdate = true;
    }
}
}

// Menu actions
void enter_load_file_menu(struct deck *d, int deckno) {
    currentDeckMenuState = DECK_MENU_LOAD_FILE;
    selectedItem = 0;
    needsUpdate = true;
}

void enter_adjust_volume(struct deck *d, int deckno) {
    currentDeckMenuState = DECK_MENU_ADJUST_VOLUME;
    needsUpdate = true;
}

void enter_settings_menu(struct deck *d, int deckno) {
    currentDeckMenuState = DECK_MENU_SETTINGS;
    selectedItem = 0;
    needsUpdate = true;
}

void enter_deck_info_display(struct deck *d, int deckno) {
    currentDeckMenuState = DECK_MENU_INFO;
    needsUpdate = true;
}

void enter_cue_menu(struct deck *d, int deckno) {
    currentDeckMenuState = DECK_MENU_CUE;
    needsUpdate = true;
}

static void format_cue_pos(char *buf, size_t len, double pos) {
    if (pos == CUE_UNSET) {
        snprintf(buf, len, "--:--.---");
    } else {
        int total_ms = (int)(pos * 1000);
        int mins = total_ms / 60000;
        int secs = (total_ms % 60000) / 1000;
        int ms = total_ms % 1000;
        snprintf(buf, len, "%d:%02d.%03d", mins, secs, ms);
    }
}

void display_cue_screen(struct deck *d, int deck_no) {
    double sel_pos = cues_get(&d->cues, CUE_LABEL_SEL);
    double bck_pos = cues_get(&d->cues, CUE_LABEL_BCK);
    char sel_str[12], bck_str[12];
    format_cue_pos(sel_str, sizeof(sel_str), sel_pos);
    format_cue_pos(bck_str, sizeof(bck_str), bck_pos);
    lcdClear(lcdHandle);
    lcdPosition(lcdHandle, 0, 0);
    lcdPrintf(lcdHandle, "CUE1:%s", sel_str);
    lcdPosition(lcdHandle, 0, 1);
    lcdPrintf(lcdHandle, "CUE2:%s", bck_str);
}

// CUE-specific button handler with long press detection on both buttons.
// Returns: 0=nothing, 1=sel short, 2=sel long, 3=bck short, 4=bck long, 5=exit
static int cue_button_pressed() {
    static unsigned long sel_press_start = 0;
    static unsigned long bck_press_start = 0;
    static bool sel_long_handled = false;
    static bool bck_long_handled = false;
    unsigned long now = millis();
    // GPIO 27 = select button (BUTTON_LONG in code, but returns 1=select in menus)
    bool sel_state = digitalRead(27) == LOW;
    // GPIO 17 = back button (BUTTON_SHORT in code, but returns 2=back in menus)
    bool bck_state = digitalRead(17) == LOW;

    // Select button (GPIO 27) - long press detection
    if (sel_state) {
        if (sel_press_start == 0) sel_press_start = now;
        if (now - sel_press_start >= CUE_LONG_PRESS_MS && !sel_long_handled) {
            sel_long_handled = true;
            return 2; // sel long press
        }
    } else {
        if (sel_press_start > 0 && !sel_long_handled) {
            sel_press_start = 0;
            return 1; // sel short press (released before threshold)
        }
        sel_press_start = 0;
        sel_long_handled = false;
    }

    // Back button (GPIO 17) - long press detection
    if (bck_state) {
        if (bck_press_start == 0) bck_press_start = now;
        if (now - bck_press_start >= CUE_LONG_PRESS_MS && !bck_long_handled) {
            bck_long_handled = true;
            return 4; // bck long press
        }
    } else {
        if (bck_press_start > 0 && !bck_long_handled) {
            bck_press_start = 0;
            return 3; // bck short press (released before threshold)
        }
        bck_press_start = 0;
        bck_long_handled = false;
    }

    return 0;
}

// Each button has its own cue point.
// Short press = go to that button's cue. Long press = set it.
// Encoder turn or rotary button = exit.
void handle_cue_navigation(struct deck *d, int deckno) {
    int encoder_movement = rotary_encoder_moved();
    int btn = cue_button_pressed();

    if (encoder_movement != 0) {
        currentDeckMenuState = DECK_MENU_MAIN;
        selectedItem = 0;
        needsUpdate = true;
        return;
    }

    if (btn == 1) {
        // SEL short press: go to SEL cue
        double pos = cues_get(&d->cues, CUE_LABEL_SEL);
        if (pos != CUE_UNSET) {
            player_seek_to(&d->player, pos);
            printf("Deck %d: Go to CUE1 at %.2f\n", deckno + 1, pos);
        }
        needsUpdate = true;
    } else if (btn == 2) {
        // SEL long press: set SEL cue
        double pos = player_get_elapsed(&d->player);
        cues_set(&d->cues, CUE_LABEL_SEL, pos);
        cues_save_to_file(&d->cues, d->player.track->path);
        printf("Deck %d: Set CUE1 at %.2f\n", deckno + 1, pos);
        needsUpdate = true;
    } else if (btn == 3) {
        // BCK short press: go to BCK cue
        double pos = cues_get(&d->cues, CUE_LABEL_BCK);
        if (pos != CUE_UNSET) {
            player_seek_to(&d->player, pos);
            printf("Deck %d: Go to CUE2 at %.2f\n", deckno + 1, pos);
        }
        needsUpdate = true;
    } else if (btn == 4) {
        // BCK long press: set BCK cue
        double pos = player_get_elapsed(&d->player);
        cues_set(&d->cues, CUE_LABEL_BCK, pos);
        cues_save_to_file(&d->cues, d->player.track->path);
        printf("Deck %d: Set CUE2 at %.2f\n", deckno + 1, pos);
        needsUpdate = true;
    }
}

void adjust_volume(struct deck *d, int deckno) {
    int movement = rotary_encoder_moved();
    int button_press = rotary_button_pressed();
    double new_volume = d->player.setVolume*100;

    if (movement != 0) {
        // Adjust volume with sensitivity and ensure it stays within [0, 100]
        new_volume = new_volume + movement; // Adjust step size as needed

        if (new_volume < 0) new_volume = 0;
        if (new_volume > 127) new_volume = 127;

        d->player.setVolume = new_volume/100;
        player_set_volume(&d->player, new_volume);

        // Call the trigger_io_event function with the updated volume
        trigger_io_event(ACTION_VOLUME, deckno, (unsigned char)new_volume);

        // Update the display to reflect the new volume
        lcdClear(lcdHandle);
        lcdPosition(lcdHandle, 0, 0);
        lcdPrintf(lcdHandle, "Volume: %d", (int)new_volume);
        lcdPosition(lcdHandle, 0, 1);
        lcdPrintf(lcdHandle, "Movement: %d", movement);  
        needsUpdate = false; // Trigger the UI update
    }

    // Handle button press
    if (button_press == 1 || button_press == 2) {
        // Return to the main menu on button press
        currentDeckMenuState = DECK_MENU_MAIN;
        needsUpdate = true;
    }
}

// Actions for load file menu
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

// --- Record menu (top-level deck menu item) ---
// Scans ALSA mixer for capture source enum (Mic, Line In, etc.),
// sets capture volume to 80%, starts passthrough so you can hear the input.

static void find_capture_source_enum(void) {
    MixerControl controls[20];
    int count = get_mixer_controls(RECORD_MIXER_CARD, controls, 20);
    captureSourceFound = false;

    for (int i = 0; i < count; i++) {
        if (controls[i].isEnum) {
            // Use the first enum control (typically "Capture Source" or "Input Source")
            memcpy(&captureSourceControl, &controls[i], sizeof(MixerControl));
            captureSourceFound = true;
            printf("Found capture source enum: %s (%d items)\n",
                   captureSourceControl.name, captureSourceControl.enumItemCount);
            break;
        }
    }
}

static void set_capture_volume_80(void) {
    MixerControl controls[20];
    int count = get_mixer_controls(RECORD_MIXER_CARD, controls, 20);

    for (int i = 0; i < count; i++) {
        if (controls[i].isVolume) {
            // Look for capture volume controls (names containing "Capture", "Mic", "Line", "Rec")
            if (strstr(controls[i].name, "Capture") || strstr(controls[i].name, "Mic") ||
                strstr(controls[i].name, "Rec") || strstr(controls[i].name, "Line")) {
                long vol80 = controls[i].min + (long)((controls[i].max - controls[i].min) * 0.8);
                set_mixer_control(RECORD_MIXER_CARD, controls[i].name, vol80);
                printf("Set %s to 80%% (%ld)\n", controls[i].name, vol80);
            }
        }
    }
}

void enter_record_menu(struct deck *d, int deckno) {
    find_capture_source_enum();
    if (!captureSourceFound) {
        printf("No capture source enum found in mixer.\n");
        currentDeckMenuState = DECK_MENU_MAIN;
        needsUpdate = true;
        return;
    }
    recordSelectedSource = captureSourceControl.currentEnumIndex;
    set_capture_volume_80();
    currentDeckMenuState = DECK_MENU_RECORD_INPUT_SOURCE;
    needsUpdate = true;
    start_passthrough(RECORD_CAPTURE_DEVICE);
}

void handle_record_input_sources_navigation(struct deck *d, int deckno) {
    int encoder_movement = rotary_encoder_moved();
    int button_press = rotary_button_pressed();

    if (encoder_movement != 0 && captureSourceFound) {
        recordSelectedSource = (recordSelectedSource + encoder_movement +
                                captureSourceControl.enumItemCount) %
                               captureSourceControl.enumItemCount;
        // Switch the mixer input source
        set_mixer_control_enum(RECORD_MIXER_CARD, captureSourceControl.name, recordSelectedSource);
        captureSourceControl.currentEnumIndex = recordSelectedSource;
        needsUpdate = true;
    }

    if (button_press == 1) {
        // Select: stop passthrough, start recording
        stop_passthrough();

        char filename[256];
        snprintf(filename, sizeof(filename), "/tmp/rec%06d.raw", nextRecordingNumber++);
        printf("Deck %d: Recording from %s (source: %s)...\n", deckno + 1,
               RECORD_CAPTURE_DEVICE, captureSourceControl.enumItems[recordSelectedSource]);

        if (start_recording(&recordingContext, RECORD_CAPTURE_DEVICE, filename) == 0) {
            currentDeckMenuState = DECK_MENU_RECORDING;
        } else {
            printf("Failed to start recording.\n");
            start_passthrough(RECORD_CAPTURE_DEVICE);
        }
        needsUpdate = true;
    }

    if (button_press == 2) {
        // Back: stop passthrough, return to deck menu
        stop_passthrough();
        currentDeckMenuState = DECK_MENU_MAIN;
        selectedItem = 0;
        needsUpdate = true;
    }
}

void handle_recording_navigation(struct deck *d, int deckno) {
    int button_press = rotary_button_pressed();

    if (button_press == 1) {
        // Select: stop recording, return to deck menu
        printf("Deck %d: Stopping recording.\n", deckno + 1);
        stop_recording(d, &recordingContext);
        currentDeckMenuState = DECK_MENU_MAIN;
        selectedItem = 0;
        needsUpdate = true;
    } else if (button_press == 2) {
        // Back: stop recording, go back to source browser to record again
        printf("Deck %d: Stopping recording.\n", deckno + 1);
        stop_recording(d, &recordingContext);
        currentDeckMenuState = DECK_MENU_RECORD_INPUT_SOURCE;
        needsUpdate = true;
        start_passthrough(RECORD_CAPTURE_DEVICE);
    }
}


void action_jog_pitch(struct deck *d, int deckno) {
    jogPitchModeEnabled = !jogPitchModeEnabled; // Toggle the mode
    printf("Deck %d: Jog Pitch Mode %s\n", deckno + 1, jogPitchModeEnabled ? "Enabled" : "Disabled");
    if(jogPitchModeEnabled)
        trigger_io_event_no_param(ACTION_JOGPIT, deckno);
    else
        trigger_io_event_no_param(ACTION_JOGPSTOP, deckno);
    currentDeckMenuState = DECK_MENU_SETTINGS;
    needsUpdate = true;
}


void action_jog_reverse(struct deck *d, int deckno) {
    printf("Deck %d: Triggering Jog Reverse...\n", deckno + 1);
    trigger_io_event_no_param(ACTION_JOGREVERSE, deckno);
    currentDeckMenuState = DECK_MENU_MAIN;
}

// Deck info display
void display_deck_info(struct deck *d, int deckno) {
    lcdClear(lcdHandle);
    lcdPosition(lcdHandle, 0, 0);
    lcdPrintf(lcdHandle, "ON: %d", player_is_active(&d->player));
    lcdPosition(lcdHandle, 6, 0);
    lcdPrintf(lcdHandle, "E: %.2f", player_get_elapsed(&d->player));

    lcdPosition(lcdHandle, 0, 1);
    lcdPrintf(lcdHandle, "%.2f/%.2f", player_get_position(&d->player), player_get_remain(&d->player));
}

void display_record_source_screen(void) {
    lcdClear(lcdHandle);
    lcdPosition(lcdHandle, 0, 0);
    lcdPuts(lcdHandle, "Record Source:");
    lcdPosition(lcdHandle, 0, 1);
    if (captureSourceFound && recordSelectedSource < captureSourceControl.enumItemCount) {
        lcdPrintf(lcdHandle, "%s", captureSourceControl.enumItems[recordSelectedSource]);
    } else {
        lcdPuts(lcdHandle, "None");
    }
}

void display_recording_status(struct deck *d, int deck_no) {
    lcdClear(lcdHandle);
    lcdPosition(lcdHandle, 0, 0);
    lcdPuts(lcdHandle, "REC:");
    if (captureSourceFound && recordSelectedSource < captureSourceControl.enumItemCount) {
        lcdPrintf(lcdHandle, "%s", captureSourceControl.enumItems[recordSelectedSource]);
    }
    lcdPosition(lcdHandle, 0, 1);
    lcdPuts(lcdHandle, "SEL=stop BCK=exit");
}