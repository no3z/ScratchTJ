#include "deck_menu.h"
#include "lcd_menu.h"
#include "deck.h"
#include "player.h"
#include <unistd.h>
#include <string.h>
#include "track.h"
#include "recording.h"

#define MAX_INPUT_SOURCES 10

InputSource inputSources[MAX_INPUT_SOURCES];
int inputSourceCount = 0;
static int nextRecordingNumber = 0;

RecordingContext recordingContext;

bool jogPitchModeEnabled;
// Externals
extern bool needsUpdate;
extern MainMenuState mainMenuState;

// Menu and State Variables
static DeckMenuState currentDeckMenuState = DECK_MENU_MAIN;
static int selectedItem = 0;

// Main deck menu items
static MenuItem mainDeckMenuItems[] = {
    {"1. Load File", enter_load_file_menu},
    // {"2. Set Volume", enter_adjust_volume},
    {"3. Settings", enter_settings_menu},
    {"4. Info", enter_deck_info_display},
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
    {"7. Record", action_record},
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
        needsUpdate = false;
    }
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

void action_record(struct deck *d, int deckno) {
    inputSourceCount = get_available_input_sources(inputSources, MAX_INPUT_SOURCES);

    if (inputSourceCount == 0) {
        // Handle the case when no input sources are found
        printf("No input sources available.\n");
        currentDeckMenuState = DECK_MENU_LOAD_FILE; // Go back to Load File menu
        needsUpdate = true;
        return;
    }

    // Enter the Recording Input Sources menu
    currentDeckMenuState = DECK_MENU_RECORD_INPUT_SOURCE;
    selectedItem = 0;
    needsUpdate = true;
}

void action_select_input_source(struct deck *d, int deckno) {
    // Get the selected input source
    InputSource *selectedSource = &inputSources[selectedItem];

    // Generate the filename
    char filename[256];
    snprintf(filename, sizeof(filename), "/tmp/rec%06d.wav", nextRecordingNumber++);
    // Assuming nextRecordingNumber is a static variable or member variable

    // Start recording with the selected input source
    printf("Deck %d: Recording from %s (%s)...\n", deckno + 1, selectedSource->name, selectedSource->device);

    if (start_recording( &recordingContext, selectedSource->device, filename) == 0) {
        // Recording started successfully
        needsUpdate = true;

        // Enter the Recording state
        currentDeckMenuState = DECK_MENU_RECORDING;
        needsUpdate = true;
    } else {
        // Failed to start recording
        printf("Failed to start recording.\n");
        currentDeckMenuState = DECK_MENU_RECORD_INPUT_SOURCE; // Return to input sources menu
        needsUpdate = true;
    }
}

void handle_record_input_sources_navigation(struct deck *d, int deckno) {
    int encoder_movement = rotary_encoder_moved();
    int button_press = rotary_button_pressed();

    if (encoder_movement != 0) {
        selectedItem = (selectedItem + encoder_movement + inputSourceCount) % inputSourceCount;
        needsUpdate = true;
    }

    if (button_press == 1) { // Short press to select an input source
        action_select_input_source(d, deckno);
        needsUpdate = true;
    }

    if (button_press == 2) { // Long press to go back to Load File Menu
        currentDeckMenuState = DECK_MENU_LOAD_FILE;
        selectedItem = 0;
        needsUpdate = true;
    }
}

void handle_recording_navigation(struct deck *d, int deckno) {
    int button_press = rotary_button_pressed();

    if (button_press == 1) { // Short press to stop recording
        printf("Deck %d: Stopping recording.\n", deckno + 1);
        stop_recording(d,&recordingContext);
        currentDeckMenuState = DECK_MENU_RECORD_INPUT_SOURCE; // Return to input sources menu
        needsUpdate = true;
    } else if (button_press == 2) { // Long press to go back to Load File menu
        stop_recording(d,&recordingContext);
        currentDeckMenuState = DECK_MENU_LOAD_FILE;
        selectedItem = 0;
        needsUpdate = true;
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

void display_input_sources_menu(InputSource *sources, int sourceCount, int selectedItem, const char *title) {
    lcdClear(lcdHandle);
    lcdPosition(lcdHandle, 0, 0);
    lcdPrintf(lcdHandle, "%s", title);
    lcdPosition(lcdHandle, 0, 1);
    lcdPrintf(lcdHandle, "%d. %s", selectedItem + 1, sources[selectedItem].name);
}

void display_recording_status(struct deck *d, int deck_no) {
    lcdClear(lcdHandle);
    lcdPosition(lcdHandle, 0, 0);
    lcdPrintf(lcdHandle, "Recording...");
    // Optionally, display recording time or other info
}