#ifndef INFO_MENU_H
#define INFO_MENU_H


typedef enum {
    MENU_INFO_CPU_MEM,
    MENU_INFO_VERSION
} InfoMenuState;

void display_info_menu_actions();
void toggle_info_menu_state();
float get_cpu_usage();
int get_memory_usage();
void handle_info_menu_navigation();

#endif
