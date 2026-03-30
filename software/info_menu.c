#include "info_menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lcd_menu.h"
#include <time.h>

extern MainMenuState mainMenuState;

// State for info menu: 0 = Action Selection, 1 = Mem/CPU, 2 = Version
static int infoMenuState = 0;
static int selectedInfoOption = 0;  // 0 for CPU/Mem, 1 for Version

// Function to read CPU usage
float get_cpu_usage() {
    long double a[4], b[4];
    FILE *fp;

    fp = fopen("/proc/stat", "r");
    if (!fp) return -1;
    fscanf(fp, "cpu %Lf %Lf %Lf %Lf", &a[0], &a[1], &a[2], &a[3]);
    fclose(fp);

    usleep(100000); // Sleep for 100ms

    fp = fopen("/proc/stat", "r");
    if (!fp) return -1;
    fscanf(fp, "cpu %Lf %Lf %Lf %Lf", &b[0], &b[1], &b[2], &b[3]);
    fclose(fp);

    return ((b[0] + b[1] + b[2]) - (a[0] + a[1] + a[2])) /
           ((b[0] + b[1] + b[2] + b[3]) - (a[0] + a[1] + a[2] + a[3])) * 100;
}

// Function to read memory usage
int get_memory_usage() {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) return -1;

    int total, free;
    fscanf(fp, "MemTotal: %d kB\nMemFree: %d kB", &total, &free);
    fclose(fp);

    return 100 - ((free * 100) / total); // Percentage used
}

// Display function for CPU/Mem
void display_cpu_mem_usage() {
    int mem_usage = get_memory_usage();
    float cpu_usage = get_cpu_usage();

    oled_clear();
    oled_draw_title_bar("System Info");
    oled_textf(10, 50, FONT_MEDIUM, "Mem: %d%%", mem_usage);
    oled_textf(10, 90, FONT_MEDIUM, "CPU: %.1f%%", cpu_usage);
    oled_flush();
}

// Display function for Version
void display_version() {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    oled_clear();
    oled_draw_title_bar("Version");
    oled_textf(10, 60, FONT_MEDIUM, "ScratchTJ v2");
    oled_textf(10, 100, FONT_MEDIUM, "%04d-%02d-%02d",
               tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    oled_flush();
}

// Info menu options for oled_draw_menu_list
static const char *infoOptions[] = {"CPU/Mem", "Version"};

// Display function for action selection
void display_info_menu_actions() {
    oled_clear();
    oled_draw_title_bar("Info Menu");
    oled_draw_menu_list(infoOptions, 2, selectedInfoOption, 0, MENU_VISIBLE_LINES);
    oled_flush();
}

// Handle navigation within the Info menu
void handle_info_menu_navigation() {
    int encoder_movement = rotary_encoder_moved();
    int button_press = rotary_button_pressed();
    int kb0 = kb0_button_pressed();

    // Navigate options in the action selection menu
    if (infoMenuState == 0) {
        if (encoder_movement != 0) {
            selectedInfoOption = (selectedInfoOption + encoder_movement + 2) % 2;
            display_info_menu_actions();
            needsUpdate = true;
        }

        /* KB0 = select */
        if (kb0 == 1) {
            if (selectedInfoOption == 0) {
                infoMenuState = 1;
            } else {
                infoMenuState = 2;
            }
            needsUpdate = true;
        }
        /* Rotary click = back */
        if (button_press == 1) {
            mainMenuState = MENU_MAIN;
            infoMenuState = 0;
            needsUpdate = true;
        }
    }
    // Display CPU/Mem continuously until button is pressed
    else if (infoMenuState == 1) {
        display_cpu_mem_usage();
        usleep(100000);
        if (button_press == 1 || kb0 == 1) {
            infoMenuState = 0;
            needsUpdate = true;
            display_info_menu_actions();
        }
    }
    else if (infoMenuState == 2) {
        display_version();
        usleep(100000);
        if (button_press == 1 || kb0 == 1) {
            infoMenuState = 0;
            needsUpdate = true;
            display_info_menu_actions();
        }
    }
}
