#include "info_menu.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lcd_menu.h"
#include <time.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

extern MainMenuState mainMenuState;

// State for info menu: 0 = Action Selection, 1 = Mem/CPU, 2 = Version, 3 = Network
static int infoMenuState = 0;
static int selectedInfoOption = 0;  // index into infoOptions

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

/* Is something accepting connections on a local TCP port? */
static bool local_port_open(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;

    /* Loopback answers at once either way; the timeout is a safety net */
    struct timeval tv = { .tv_sec = 0, .tv_usec = 200000 };
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    bool open = (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    close(fd);
    return open;
}

#define FTP_PORT 21

/* Display function for Network: the address to type into an FTP client
 * to manage the beats and samples folders over the network */
void display_network_info() {
    struct ifaddrs *ifs = NULL, *i;
    int y = 34, shown = 0;

    oled_clear();
    oled_draw_title_bar("Network");

    if (getifaddrs(&ifs) == 0) {
        for (i = ifs; i && shown < 2; i = i->ifa_next) {
            char ip[INET_ADDRSTRLEN];

            if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET)
                continue;
            if ((i->ifa_flags & IFF_LOOPBACK) || !(i->ifa_flags & IFF_UP))
                continue;

            inet_ntop(AF_INET, &((struct sockaddr_in *)i->ifa_addr)->sin_addr, ip, sizeof(ip));

            oled_text_color(8, y, i->ifa_name, FONT_SMALL, THEME_RANGE_FG);
            oled_text_color(8, y + 12, ip, FONT_MEDIUM, THEME_VALUE_FG);
            y += 40;
            shown++;
        }
        freeifaddrs(ifs);
    }

    if (shown == 0) {
        oled_text_color(8, y + 12, "No network", FONT_MEDIUM, THEME_STOPPED);
        y += 40;
    }

    /* Probing opens a real connection to the FTP server, so do it now
     * and then rather than on every redraw */
    static bool ftp_up = false;
    static unsigned int redraws = 0;
    if (redraws++ % 20 == 0)
        ftp_up = local_port_open(FTP_PORT);

    y += 6;
    if (ftp_up) {
        oled_text_color(8, y, "FTP: beats + samples", FONT_SMALL, THEME_TEXT);
        oled_textf(8, y + 14, FONT_SMALL, "ftp://<address>  port %d", FTP_PORT);
        oled_text_color(8, y + 28, "user: no3z", FONT_SMALL, THEME_TEXT);
        oled_text_color(8, y + 46, "Restart to see new files", FONT_SMALL, THEME_RANGE_FG);
    } else {
        oled_text_color(8, y, "FTP server not running", FONT_SMALL, THEME_STOPPED);
    }

    oled_flush();
}

// Info menu options for oled_draw_menu_list
static const char *infoOptions[] = {"CPU/Mem", "Version", "Network / FTP"};
#define INFO_OPTION_COUNT 3

// Display function for action selection
void display_info_menu_actions() {
    oled_clear();
    oled_draw_title_bar("Info Menu");
    oled_draw_menu_list(infoOptions, INFO_OPTION_COUNT, selectedInfoOption, 0, MENU_VISIBLE_LINES);
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
            selectedInfoOption = ((selectedInfoOption + encoder_movement) % INFO_OPTION_COUNT +
                                  INFO_OPTION_COUNT) % INFO_OPTION_COUNT;
            display_info_menu_actions();
            needsUpdate = true;
        }

        /* KB0 = select */
        if (kb0 == 1) {
            infoMenuState = selectedInfoOption + 1;
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
    else if (infoMenuState == 3) {
        display_network_info();
        usleep(250000);
        if (button_press == 1 || kb0 == 1) {
            infoMenuState = 0;
            needsUpdate = true;
            display_info_menu_actions();
        }
    }
}
