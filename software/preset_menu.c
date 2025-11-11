#include "preset_menu.h"
#include "lcd_menu.h"
#include "xwax.h"
#include "shared_variables.h"
#include "track.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

// Globals
extern int lcdHandle;
extern bool needsUpdate;
extern MainMenuState mainMenuState;
extern SC_SETTINGS scsettings;

// Preset directory
#define PRESET_DIR "/home/no3z/.scratchtj/presets"
#define PRESET_FILE_FMT "%s/preset_%d.cfg"
#define LAST_PRESET_FILE "%s/last_preset.txt"

// Menu state
typedef enum {
    PRESET_MAIN,
    PRESET_LOAD,
    PRESET_SAVE
} PresetMenuState;

static PresetMenuState presetMenuState = PRESET_MAIN;
static int selectedPresetSlot = 0;

// Preset menu options
const char *presetMainOptions[] = {
    "Load Preset",
    "Save Preset"
};

// Function to create preset directory if it doesn't exist
static void ensure_preset_directory() {
    struct stat st = {0};
    if (stat(PRESET_DIR, &st) == -1) {
        mkdir(PRESET_DIR, 0755);
    }
}

// Display Preset Menu
void display_preset_menu() {
    lcdClear(lcdHandle);
    lcdPosition(lcdHandle, 0, 0);

    if (presetMenuState == PRESET_MAIN) {
        lcdPuts(lcdHandle, "Presets");
        lcdPosition(lcdHandle, 0, 1);
        lcdPuts(lcdHandle, presetMainOptions[selectedPresetSlot]);
    } else if (presetMenuState == PRESET_LOAD) {
        lcdPuts(lcdHandle, "Load Preset");
        lcdPosition(lcdHandle, 0, 1);
        lcdPrintf(lcdHandle, "Slot %d", selectedPresetSlot + 1);
    } else if (presetMenuState == PRESET_SAVE) {
        lcdPuts(lcdHandle, "Save Preset");
        lcdPosition(lcdHandle, 0, 1);
        lcdPrintf(lcdHandle, "Slot %d", selectedPresetSlot + 1);
    }
}

// Handle Preset Menu Navigation
void handle_preset_menu_navigation(struct deck *decks[]) {
    int encoder_movement = rotary_encoder_moved();
    int button_press = rotary_button_pressed();

    if (presetMenuState == PRESET_MAIN) {
        // Main preset menu - choose Load or Save
        if (encoder_movement != 0) {
            selectedPresetSlot = (selectedPresetSlot + encoder_movement + 2) % 2;
            needsUpdate = true;
        }

        if (button_press == 1) { // Short press - enter submenu
            if (selectedPresetSlot == 0) {
                presetMenuState = PRESET_LOAD;
            } else {
                presetMenuState = PRESET_SAVE;
            }
            selectedPresetSlot = 0; // Reset to slot 1
            needsUpdate = true;
        } else if (button_press == 2) { // Long press - back to main
            mainMenuState = MENU_MAIN;
            presetMenuState = PRESET_MAIN;
            needsUpdate = true;
        }
    } else if (presetMenuState == PRESET_LOAD) {
        // Load preset submenu - select slot
        if (encoder_movement != 0) {
            selectedPresetSlot = (selectedPresetSlot + encoder_movement + 3) % 3;
            needsUpdate = true;
        }

        if (button_press == 1) { // Short press - load preset
            load_preset_from_slot(selectedPresetSlot + 1, decks);
            save_last_preset_number(selectedPresetSlot + 1);

            // Show confirmation
            lcdClear(lcdHandle);
            lcdPosition(lcdHandle, 0, 0);
            lcdPuts(lcdHandle, "Loaded!");
            lcdPosition(lcdHandle, 0, 1);
            lcdPrintf(lcdHandle, "Preset %d", selectedPresetSlot + 1);
            delay(1000);

            presetMenuState = PRESET_MAIN;
            selectedPresetSlot = 0;
            needsUpdate = true;
        } else if (button_press == 2) { // Long press - back to main
            presetMenuState = PRESET_MAIN;
            selectedPresetSlot = 0;
            needsUpdate = true;
        }
    } else if (presetMenuState == PRESET_SAVE) {
        // Save preset submenu - select slot
        if (encoder_movement != 0) {
            selectedPresetSlot = (selectedPresetSlot + encoder_movement + 3) % 3;
            needsUpdate = true;
        }

        if (button_press == 1) { // Short press - save preset
            save_preset_to_slot(selectedPresetSlot + 1, decks);
            save_last_preset_number(selectedPresetSlot + 1);

            // Show confirmation
            lcdClear(lcdHandle);
            lcdPosition(lcdHandle, 0, 0);
            lcdPuts(lcdHandle, "Saved!");
            lcdPosition(lcdHandle, 0, 1);
            lcdPrintf(lcdHandle, "Preset %d", selectedPresetSlot + 1);
            delay(1000);

            presetMenuState = PRESET_MAIN;
            selectedPresetSlot = 0;
            needsUpdate = true;
        } else if (button_press == 2) { // Long press - back to main
            presetMenuState = PRESET_MAIN;
            selectedPresetSlot = 0;
            needsUpdate = true;
        }
    }
}

// Save preset to slot
void save_preset_to_slot(int slot, struct deck *decks[]) {
    ensure_preset_directory();

    char filename[512];
    snprintf(filename, sizeof(filename), PRESET_FILE_FMT, PRESET_DIR, slot);

    FILE *f = fopen(filename, "w");
    if (!f) {
        printf("Error: Cannot create preset file: %s\n", filename);
        return;
    }

    fprintf(f, "# ScratchTJ Preset %d\n", slot);
    fprintf(f, "# Auto-generated\n\n");

    // Save deck 0 state
    fprintf(f, "[deck0]\n");
    if (decks[0]->CurrentFolder && decks[0]->CurrentFile) {
        fprintf(f, "folder=%s\n", decks[0]->CurrentFolder->FullPath);
        fprintf(f, "file=%s\n", decks[0]->CurrentFile->FullPath);
    }
    fprintf(f, "\n");

    // Save deck 1 state
    fprintf(f, "[deck1]\n");
    if (decks[1]->CurrentFolder && decks[1]->CurrentFile) {
        fprintf(f, "folder=%s\n", decks[1]->CurrentFolder->FullPath);
        fprintf(f, "file=%s\n", decks[1]->CurrentFile->FullPath);
    }
    fprintf(f, "\n");

    // Save integer settings
    fprintf(f, "[settings_int]\n");
    fprintf(f, "buffersize=%d\n", scsettings.buffersize);
    fprintf(f, "platterspeed=%d\n", scsettings.platterspeed);
    fprintf(f, "slippiness=%d\n", scsettings.slippiness);
    fprintf(f, "brakespeed=%d\n", scsettings.brakespeed);
    fprintf(f, "faderopenpoint=%d\n", scsettings.faderopenpoint);
    fprintf(f, "faderclosepoint=%d\n", scsettings.faderclosepoint);
    fprintf(f, "jogReverse=%d\n", scsettings.jogReverse);
    fprintf(f, "cutbeats=%d\n", scsettings.cutbeats);
    fprintf(f, "pitchrange=%d\n", scsettings.pitchrange);
    fprintf(f, "\n");

    // Save float variables
    int count;
    EditableVariable *vars = get_editable_variables(&count);
    fprintf(f, "[settings_float]\n");
    for (int i = 0; i < count; i++) {
        float value;
        if (get_variable_value(vars[i].name, &value)) {
            fprintf(f, "%s=%.3f\n", vars[i].name, value);
        }
    }

    fclose(f);
    printf("Preset %d saved to %s\n", slot, filename);
}

// Load preset from slot
void load_preset_from_slot(int slot, struct deck *decks[]) {
    char filename[512];
    snprintf(filename, sizeof(filename), PRESET_FILE_FMT, PRESET_DIR, slot);

    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("Error: Cannot open preset file: %s\n", filename);
        return;
    }

    char line[512];
    char section[64] = "";
    char deck0_folder[260] = "";
    char deck0_file[260] = "";
    char deck1_folder[260] = "";
    char deck1_file[260] = "";

    // Parse preset file
    while (fgets(line, sizeof(line), f)) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;

        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\0') continue;

        // Check for section headers
        if (line[0] == '[') {
            sscanf(line, "[%63[^]]]", section);
            continue;
        }

        // Parse key=value pairs
        char key[128], value[384];
        if (sscanf(line, "%127[^=]=%383[^\n]", key, value) == 2) {
            if (strcmp(section, "deck0") == 0) {
                if (strcmp(key, "folder") == 0) {
                    strncpy(deck0_folder, value, sizeof(deck0_folder) - 1);
                } else if (strcmp(key, "file") == 0) {
                    strncpy(deck0_file, value, sizeof(deck0_file) - 1);
                }
            } else if (strcmp(section, "deck1") == 0) {
                if (strcmp(key, "folder") == 0) {
                    strncpy(deck1_folder, value, sizeof(deck1_folder) - 1);
                } else if (strcmp(key, "file") == 0) {
                    strncpy(deck1_file, value, sizeof(deck1_file) - 1);
                }
            } else if (strcmp(section, "settings_int") == 0) {
                int intval = atoi(value);
                if (strcmp(key, "buffersize") == 0) scsettings.buffersize = intval;
                else if (strcmp(key, "platterspeed") == 0) scsettings.platterspeed = intval;
                else if (strcmp(key, "slippiness") == 0) scsettings.slippiness = intval;
                else if (strcmp(key, "brakespeed") == 0) scsettings.brakespeed = intval;
                else if (strcmp(key, "faderopenpoint") == 0) scsettings.faderopenpoint = intval;
                else if (strcmp(key, "faderclosepoint") == 0) scsettings.faderclosepoint = intval;
                else if (strcmp(key, "jogReverse") == 0) scsettings.jogReverse = intval;
                else if (strcmp(key, "cutbeats") == 0) scsettings.cutbeats = intval;
                else if (strcmp(key, "pitchrange") == 0) scsettings.pitchrange = intval;
            } else if (strcmp(section, "settings_float") == 0) {
                float floatval = atof(value);
                set_variable_value(key, floatval);
            }
        }
    }

    fclose(f);

    // Load deck files
    if (strlen(deck0_file) > 0) {
        struct track *track = track_acquire_by_import(decks[0]->importer, deck0_file);
        if (track) {
            player_set_track(&decks[0]->player, track);
            printf("Deck 0 loaded: %s\n", deck0_file);
        }
    }

    if (strlen(deck1_file) > 0) {
        struct track *track = track_acquire_by_import(decks[1]->importer, deck1_file);
        if (track) {
            player_set_track(&decks[1]->player, track);
            printf("Deck 1 loaded: %s\n", deck1_file);
        }
    }

    printf("Preset %d loaded from %s\n", slot, filename);
}

// Load last preset number
int load_last_preset_number() {
    char filename[512];
    snprintf(filename, sizeof(filename), LAST_PRESET_FILE, PRESET_DIR);

    FILE *f = fopen(filename, "r");
    if (!f) {
        return 0; // No last preset
    }

    int preset = 0;
    fscanf(f, "%d", &preset);
    fclose(f);

    return preset;
}

// Save last preset number
void save_last_preset_number(int preset) {
    ensure_preset_directory();

    char filename[512];
    snprintf(filename, sizeof(filename), LAST_PRESET_FILE, PRESET_DIR);

    FILE *f = fopen(filename, "w");
    if (!f) {
        printf("Error: Cannot save last preset number\n");
        return;
    }

    fprintf(f, "%d\n", preset);
    fclose(f);
}
