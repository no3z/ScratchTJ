#include "preset_menu.h"
#include "lcd_menu.h"
#include "xwax.h"
#include "shared_variables.h"
#include "track.h"
#include "sc_input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

// Globals
extern int lcdHandle;
extern bool needsUpdate;
extern SC_SETTINGS scsettings;

// Preset directory
#define PRESET_DIR "/home/no3z/.scratchtj/presets"
#define PRESET_FILE_FMT "%s/preset_%d.cfg"
#define LAST_PRESET_FILE "%s/last_preset.txt"
#define NUM_PRESET_SLOTS 5

// Function to create preset directory if it doesn't exist
static void ensure_preset_directory() {
    struct stat st = {0};
    if (stat(PRESET_DIR, &st) == -1) {
        mkdir(PRESET_DIR, 0755);
    }
}

// Read the saved date from a preset file, returns 1 if found
static int get_preset_date(int slot, char *datebuf, int bufsize) {
    char filename[512];
    snprintf(filename, sizeof(filename), PRESET_FILE_FMT, PRESET_DIR, slot);

    FILE *f = fopen(filename, "r");
    if (!f) return 0;

    char line[512];
    char section[64] = "";
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        if (line[0] == '#' || line[0] == '\0') continue;
        if (line[0] == '[') {
            sscanf(line, "[%63[^]]]", section);
            continue;
        }
        if (strcmp(section, "metadata") == 0) {
            char key[128], value[384];
            if (sscanf(line, "%127[^=]=%383[^\n]", key, value) == 2) {
                if (strcmp(key, "date") == 0) {
                    strncpy(datebuf, value, bufsize - 1);
                    datebuf[bufsize - 1] = '\0';
                    fclose(f);
                    return 1;
                }
            }
        }
    }
    fclose(f);
    return 0;
}

// Check if a preset slot file exists
static int preset_slot_exists(int slot) {
    char filename[512];
    snprintf(filename, sizeof(filename), PRESET_FILE_FMT, PRESET_DIR, slot);
    struct stat st;
    return (stat(filename, &st) == 0);
}

// Blocking submenu: Save
static void enter_save_submenu(struct deck *decks[]) {
    int selected = 0;
    needsUpdate = true;
    bool active = true;

    while (active) {
        int movement = rotary_encoder_moved();
        int button = rotary_button_pressed();

        if (movement != 0) {
            selected = (selected + movement + NUM_PRESET_SLOTS) % NUM_PRESET_SLOTS;
            needsUpdate = true;
        }

        if (button == 1) {
            save_preset_to_slot(selected + 1, decks);
            save_last_preset_number(selected + 1);

            lcdClear(lcdHandle);
            lcdPosition(lcdHandle, 0, 0);
            lcdPuts(lcdHandle, "Saved!");
            lcdPosition(lcdHandle, 0, 1);
            lcdPrintf(lcdHandle, "Preset %d", selected + 1);
            delay(1000);
            active = false;
        } else if (button == 2) {
            active = false;
        }

        if (needsUpdate) {
            char datebuf[32];
            lcdClear(lcdHandle);
            lcdPosition(lcdHandle, 0, 0);
            lcdPuts(lcdHandle, "Save to Slot");
            lcdPosition(lcdHandle, 0, 1);
            if (get_preset_date(selected + 1, datebuf, sizeof(datebuf))) {
                lcdPrintf(lcdHandle, "%d: %s", selected + 1, datebuf);
            } else if (preset_slot_exists(selected + 1)) {
                lcdPrintf(lcdHandle, "%d: Used", selected + 1);
            } else {
                lcdPrintf(lcdHandle, "%d: Empty", selected + 1);
            }
            needsUpdate = false;
        }
    }
}

// Blocking submenu: Load
static void enter_load_submenu(struct deck *decks[]) {
    int selected = 0;
    needsUpdate = true;
    bool active = true;

    while (active) {
        int movement = rotary_encoder_moved();
        int button = rotary_button_pressed();

        if (movement != 0) {
            selected = (selected + movement + NUM_PRESET_SLOTS) % NUM_PRESET_SLOTS;
            needsUpdate = true;
        }

        if (button == 1) {
            if (preset_slot_exists(selected + 1)) {
                load_preset_from_slot(selected + 1, decks);
                save_last_preset_number(selected + 1);

                lcdClear(lcdHandle);
                lcdPosition(lcdHandle, 0, 0);
                lcdPuts(lcdHandle, "Loaded!");
                lcdPosition(lcdHandle, 0, 1);
                lcdPrintf(lcdHandle, "Preset %d", selected + 1);
                delay(1000);
                active = false;
            } else {
                lcdClear(lcdHandle);
                lcdPosition(lcdHandle, 0, 0);
                lcdPuts(lcdHandle, "Slot Empty");
                delay(800);
                needsUpdate = true;
            }
        } else if (button == 2) {
            active = false;
        }

        if (needsUpdate) {
            char datebuf[32];
            lcdClear(lcdHandle);
            lcdPosition(lcdHandle, 0, 0);
            lcdPuts(lcdHandle, "Load Preset");
            lcdPosition(lcdHandle, 0, 1);
            if (get_preset_date(selected + 1, datebuf, sizeof(datebuf))) {
                lcdPrintf(lcdHandle, "%d: %s", selected + 1, datebuf);
            } else if (preset_slot_exists(selected + 1)) {
                lcdPrintf(lcdHandle, "%d: Used", selected + 1);
            } else {
                lcdPrintf(lcdHandle, "%d: Empty", selected + 1);
            }
            needsUpdate = false;
        }
    }
}

// Blocking submenu: Reset confirm
static void enter_reset_submenu(void) {
    needsUpdate = true;
    int selected = 1; // Default to "No"
    bool active = true;

    while (active) {
        int movement = rotary_encoder_moved();
        int button = rotary_button_pressed();

        if (movement != 0) {
            selected = (selected + movement + 2) % 2;
            needsUpdate = true;
        }

        if (button == 1) {
            if (selected == 0) { // Yes
                reset_to_defaults();
                lcdClear(lcdHandle);
                lcdPosition(lcdHandle, 0, 0);
                lcdPuts(lcdHandle, "Reset Done!");
                delay(1000);
            }
            active = false;
        } else if (button == 2) {
            active = false;
        }

        if (needsUpdate) {
            lcdClear(lcdHandle);
            lcdPosition(lcdHandle, 0, 0);
            lcdPuts(lcdHandle, "Reset defaults?");
            lcdPosition(lcdHandle, 0, 1);
            lcdPrintf(lcdHandle, selected == 0 ? "> Yes" : "> No");
            needsUpdate = false;
        }
    }
}

// Main preset submenu (blocking loop)
void enter_preset_submenu(struct deck *decks[]) {
    const char *options[] = {"Save", "Load", "Reset"};
    int optionCount = 3;
    int selected = 0;
    needsUpdate = true;
    bool active = true;

    while (active) {
        int movement = rotary_encoder_moved();
        int button = rotary_button_pressed();

        if (movement != 0) {
            selected = (selected + movement + optionCount) % optionCount;
            needsUpdate = true;
        }

        if (button == 1) {
            switch (selected) {
                case 0: enter_save_submenu(decks); break;
                case 1: enter_load_submenu(decks); break;
                case 2: enter_reset_submenu(); break;
            }
            needsUpdate = true;
        } else if (button == 2) {
            active = false;
        }

        if (needsUpdate) {
            lcdClear(lcdHandle);
            lcdPosition(lcdHandle, 0, 0);
            lcdPuts(lcdHandle, "Presets");
            lcdPosition(lcdHandle, 0, 1);
            lcdPrintf(lcdHandle, "%s", options[selected]);
            needsUpdate = false;
        }
    }
}

// Save preset to slot (with date metadata)
void save_preset_to_slot(int slot, struct deck *decks[]) {
    ensure_preset_directory();

    char filename[512];
    snprintf(filename, sizeof(filename), PRESET_FILE_FMT, PRESET_DIR, slot);

    FILE *f = fopen(filename, "w");
    if (!f) {
        printf("Error: Cannot create preset file: %s\n", filename);
        return;
    }

    // Write metadata with timestamp
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    fprintf(f, "[metadata]\n");
    fprintf(f, "date=%02d/%02d %02d:%02d\n\n",
            tm->tm_mday, tm->tm_mon + 1, tm->tm_hour, tm->tm_min);

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

// Reset all settings to defaults
void reset_to_defaults(void) {
    // Reset integer settings to defaults (from xwax.c)
    scsettings.buffersize = 256;
    scsettings.faderclosepoint = 2;
    scsettings.faderopenpoint = 10;
    scsettings.platterspeed = 2275;
    scsettings.slippiness = 200;
    scsettings.brakespeed = 3000;
    scsettings.pitchrange = 50;
    scsettings.jogReverse = 0;
    scsettings.cutbeats = 0;

    // Reset float variables to their registered defaults
    reset_all_variables_to_defaults();

    printf("All settings reset to defaults\n");
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
