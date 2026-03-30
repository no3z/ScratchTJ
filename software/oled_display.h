#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "st7789.h"

/* Display dimensions (re-export for menu code) */
#define DISPLAY_WIDTH  ST7789_WIDTH
#define DISPLAY_HEIGHT ST7789_HEIGHT

/* Number of visible menu items below the title bar (11 to leave room for cue bar) */
#define MENU_VISIBLE_LINES 11

/* Color theme */
#define THEME_BG          COLOR_BLACK
#define THEME_TEXT         COLOR_WHITE
#define THEME_TITLE_BG    RGB565(0, 40, 120)
#define THEME_TITLE_FG    COLOR_WHITE
#define THEME_SELECT_BG   RGB565(0, 80, 180)
#define THEME_SELECT_FG   COLOR_WHITE
#define THEME_SCROLLBAR    RGB565(80, 80, 80)
#define THEME_SCROLLTHUMB  COLOR_WHITE
#define THEME_SEPARATOR    RGB565(0, 80, 100)
#define THEME_VALUE_FG     RGB565(0, 255, 100)
#define THEME_RANGE_FG     RGB565(180, 180, 180)

/* Cue bar colors */
#define THEME_CUE_SET     RGB565(0, 200, 0)
#define THEME_CUE_ACTIVE  RGB565(255, 200, 0)
#define THEME_CUE_EMPTY   RGB565(60, 60, 60)

/* Per-cue colors: CUE1=red, CUE2=green, CUE3=yellow, CUE4=blue */
#define THEME_CUE1_COLOR  RGB565(220, 40, 40)
#define THEME_CUE2_COLOR  RGB565(0, 200, 0)
#define THEME_CUE3_COLOR  RGB565(240, 200, 0)
#define THEME_CUE4_COLOR  RGB565(40, 80, 220)

/* Deck accent colors */
#define THEME_DECK1_ACCENT RGB565(0, 200, 255)
#define THEME_DECK2_ACCENT RGB565(255, 140, 0)
#define THEME_DECK2_ACCENT_DIM RGB565(0, 160, 200)
#define THEME_PLAYING      RGB565(0, 220, 0)
#define THEME_STOPPED      RGB565(160, 160, 160)

/* Cue display states */
#define CUE_STATE_EMPTY   0
#define CUE_STATE_SET     1
#define CUE_STATE_ACTIVE  2

/* Initialize/close the display */
int  oled_init(void);
void oled_close(void);

/* Framebuffer operations - compose a frame, then flush once */
void oled_clear(void);
void oled_text(int x, int y, const char *str, int font);
void oled_textf(int x, int y, int font, const char *fmt, ...);
void oled_text_color(int x, int y, const char *str, int font, uint16_t color);
void oled_hline(int x, int y, int width);
void oled_fill_rect(int x, int y, int w, int h, uint16_t color);
void oled_flush(void);

/* High-level menu drawing helpers */
void oled_draw_title_bar(const char *title);
void oled_draw_menu_list(const char **items, int count, int selected,
                         int scroll_offset, int visible_lines);
void oled_draw_value_screen(const char *name, const char *value_str,
                            const char *range_str);
void oled_draw_confirm(const char *line1, const char *line2);
void oled_draw_cue_bar(const int states[4], const double positions[4]);
void oled_draw_progress_bar(int x, int y, int w, int h, float progress, uint16_t color);
void oled_draw_circle(int cx, int cy, int r, uint16_t color);
void oled_draw_line(int x0, int y0, int x1, int y1, uint16_t color);
void oled_draw_center_fader(int x, int y, int w, int h, float value, uint16_t color);
void oled_set_cue_overlay(const int states[4], const double positions[4]);
void oled_format_time(double seconds, char *buf, int bufsize);

/* Scroll offset helper - call after changing selectedItem */
int oled_compute_scroll(int selected, int scroll_offset, int visible_lines);

#endif
