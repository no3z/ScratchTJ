#include "info_menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lcd_menu.h"
#include <time.h>

extern int lcdHandle;  // LCD handle from main or lcd_menu.c
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

    lcdClear(lcdHandle);
    lcdPosition(lcdHandle, 0, 0);
    lcdPrintf(lcdHandle, "Mem: %d%%", mem_usage);
    lcdPosition(lcdHandle, 0, 1);
    lcdPrintf(lcdHandle, "CPU: %.1f%%", cpu_usage);    
}

// Display function for Version
void display_version() {
    lcdClear(lcdHandle);
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    lcdPosition(lcdHandle, 0, 0);
    lcdPrintf(lcdHandle, "Version:");
    lcdPosition(lcdHandle, 0, 1);
    lcdPrintf(lcdHandle, "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
}

// Display function for action selection
void display_info_menu_actions() {
    lcdClear(lcdHandle);
    lcdPosition(lcdHandle, 0, 0);
    lcdPuts(lcdHandle, "Select Info:");
    lcdPosition(lcdHandle, 0, 1);
    if (selectedInfoOption == 0) {
        lcdPuts(lcdHandle, "1. CPU/Mem");
    } else {
        lcdPuts(lcdHandle, "2. Version");
    }
}

// Handle navigation within the Info menu
void handle_info_menu_navigation() {
    int encoder_movement = rotary_encoder_moved();
    int button_press = rotary_button_pressed();
            
    // Navigate options in the action selection menu
    if (infoMenuState == 0) {
        if (encoder_movement != 0) {
            selectedInfoOption = (selectedInfoOption + encoder_movement + 2) % 2;
            display_info_menu_actions();
            needsUpdate = true;
        }
        
        if (button_press == 1) {  // Short press to select an option
            if (selectedInfoOption == 0) {
                infoMenuState = 1;  // Enter CPU/Mem view
            } else {
                infoMenuState = 2;  // Enter Version view
            }
            needsUpdate = true;
        } else if (button_press == 2) {  // Long press to exit to main menu
            mainMenuState = MENU_MAIN;
            infoMenuState = 0;
            needsUpdate = true;
        }
    }
    // Display CPU/Mem continuously until button is pressed
    else if (infoMenuState == 1) {
        display_cpu_mem_usage();       
        usleep(100000);  // Delay 500ms for a smooth update rate
        if (button_press == 1) {  // Short press to return to actions
            infoMenuState = 0;
            needsUpdate = true;
            display_info_menu_actions();
        }
    }
    // Display Version once until button is pressed
    else if (infoMenuState == 2) {
        display_version();
        usleep(100000);  // Delay 500ms for a smooth update rate
        if (button_press == 1) {  // Short press to return to actions
            infoMenuState = 0;
            needsUpdate = true;
            display_info_menu_actions();
        }
    }
}
