#include "oled_display.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

/* Layout constants for 240x240 TFT */
#define TITLE_BAR_HEIGHT  24
#define TITLE_TEXT_Y       4
#define SEPARATOR_Y       (TITLE_BAR_HEIGHT)
#define LIST_START_Y      (TITLE_BAR_HEIGHT + 2)
#define LINE_HEIGHT       16   /* pixels per line (FONT_SMALL = scale 1 * 2 padding) */
#define SCROLLBAR_WIDTH    6
#define SCROLLBAR_X       (DISPLAY_WIDTH - SCROLLBAR_WIDTH - 2)

/* Cue bar at bottom of screen */
#define CUE_BAR_HEIGHT    20
#define CUE_BAR_Y         (DISPLAY_HEIGHT - CUE_BAR_HEIGHT)
#define CUE_BAR_PAD        4
#define CUE_BTN_GAP        4

/* Cue bar overlay state (always drawn by oled_flush) */
static int overlay_cue_states[4] = {0, 0, 0, 0};
static double overlay_cue_positions[4] = {0, 0, 0, 0};
static bool overlay_enabled = false;

int oled_init(void) {
    return st7789_init("/dev/spidev0.0", 24, 25);
}

void oled_close(void) {
    st7789_close();
}

void oled_clear(void) {
    st7789_clear(THEME_BG);
}

void oled_text(int x, int y, const char *str, int font) {
    st7789_draw_string(x, y, str, THEME_TEXT, font);
}

void oled_textf(int x, int y, int font, const char *fmt, ...) {
    char buf[64];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    st7789_draw_string(x, y, buf, THEME_TEXT, font);
}

void oled_text_color(int x, int y, const char *str, int font, uint16_t color) {
    st7789_draw_string(x, y, str, color, font);
}

void oled_hline(int x, int y, int width) {
    st7789_hline(x, y, width, THEME_SEPARATOR);
}

void oled_fill_rect(int x, int y, int w, int h, uint16_t color) {
    st7789_fill_rect(x, y, w, h, color);
}

void oled_flush(void) {
    if (overlay_enabled)
        oled_draw_cue_bar(overlay_cue_states, overlay_cue_positions);
    st7789_flush();
}

void oled_draw_title_bar(const char *title) {
    /* Blue background bar */
    st7789_fill_rect(0, 0, DISPLAY_WIDTH, TITLE_BAR_HEIGHT, THEME_TITLE_BG);
    /* Title text in medium font (2x scale) */
    st7789_draw_string(4, TITLE_TEXT_Y, title, THEME_TITLE_FG, FONT_MEDIUM);
    /* Separator line */
    st7789_hline(0, SEPARATOR_Y, DISPLAY_WIDTH, THEME_SEPARATOR);
}

void oled_draw_menu_list(const char **items, int count, int selected,
                         int scroll_offset, int visible_lines) {
    int max_text_width = DISPLAY_WIDTH - 20; /* leave room for "> " prefix */
    int need_scrollbar = (count > visible_lines);
    if (need_scrollbar)
        max_text_width -= (SCROLLBAR_WIDTH + 4);

    for (int i = 0; i < visible_lines && (scroll_offset + i) < count; i++) {
        int item_idx = scroll_offset + i;
        int y = LIST_START_Y + i * LINE_HEIGHT;

        if (item_idx == selected) {
            /* Highlight background for selected item */
            int bar_w = need_scrollbar ? SCROLLBAR_X - 1 : DISPLAY_WIDTH;
            st7789_fill_rect(0, y, bar_w, LINE_HEIGHT, THEME_SELECT_BG);
            st7789_draw_string(4, y + 2, ">", THEME_SELECT_FG, FONT_SMALL);
            st7789_draw_string(16, y + 2, items[item_idx], THEME_SELECT_FG, FONT_SMALL);
        } else {
            st7789_draw_string(16, y + 2, items[item_idx], THEME_TEXT, FONT_SMALL);
        }
    }

    /* Draw scroll bar if needed */
    if (need_scrollbar) {
        int bar_area_height = visible_lines * LINE_HEIGHT;
        int bar_height = bar_area_height * visible_lines / count;
        if (bar_height < 6) bar_height = 6;
        int bar_max_travel = bar_area_height - bar_height;
        int bar_y = LIST_START_Y;
        if (count > visible_lines)
            bar_y += bar_max_travel * scroll_offset / (count - visible_lines);

        /* Track */
        st7789_vline(SCROLLBAR_X + SCROLLBAR_WIDTH / 2, LIST_START_Y,
                     bar_area_height, THEME_SCROLLBAR);
        /* Thumb */
        st7789_fill_rect(SCROLLBAR_X, bar_y,
                         SCROLLBAR_WIDTH, bar_height, THEME_SCROLLTHUMB);
    }
}

void oled_draw_value_screen(const char *name, const char *value_str,
                            const char *range_str) {
    oled_draw_title_bar(name);

    /* Value centered in large font */
    int vw = st7789_string_width(value_str, FONT_LARGE);
    int vx = (DISPLAY_WIDTH - vw) / 2;
    if (vx < 0) vx = 0;
    st7789_draw_string(vx, 80, value_str, THEME_VALUE_FG, FONT_LARGE);

    /* Range at bottom in small font */
    if (range_str) {
        int rw = st7789_string_width(range_str, FONT_SMALL);
        int rx = (DISPLAY_WIDTH - rw) / 2;
        if (rx < 0) rx = 0;
        st7789_draw_string(rx, 180, range_str, THEME_RANGE_FG, FONT_SMALL);
    }
}

void oled_draw_confirm(const char *line1, const char *line2) {
    /* Line 1 centered in large font */
    int w1 = st7789_string_width(line1, FONT_LARGE);
    int x1 = (DISPLAY_WIDTH - w1) / 2;
    if (x1 < 0) x1 = 0;
    st7789_draw_string(x1, 60, line1, THEME_TEXT, FONT_LARGE);

    /* Line 2 centered in medium font */
    if (line2) {
        int w2 = st7789_string_width(line2, FONT_MEDIUM);
        int x2 = (DISPLAY_WIDTH - w2) / 2;
        if (x2 < 0) x2 = 0;
        st7789_draw_string(x2, 120, line2, THEME_RANGE_FG, FONT_MEDIUM);
    }
}

void oled_format_time(double seconds, char *buf, int bufsize) {
    if (seconds < 0) seconds = 0;
    if (seconds > 5999) seconds = 5999;
    int total_cs = (int)(seconds * 100);
    int mins = total_cs / 6000;
    int secs = (total_cs / 100) % 60;
    int cs = total_cs % 100;
    snprintf(buf, bufsize, "%d:%02d.%02d", mins, secs, cs);
}

void oled_draw_progress_bar(int x, int y, int w, int h, float progress,
                            uint16_t color) {
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    int filled = (int)(w * progress);
    if (filled > 0)
        st7789_fill_rect(x, y, filled, h, color);
    if (filled < w)
        st7789_fill_rect(x + filled, y, w - filled, h, RGB565(40, 40, 40));
}

/* ── Drawing primitives for platter ──────────────────────────────── */

void oled_draw_circle(int cx, int cy, int r, uint16_t color) {
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        st7789_pixel(cx+x, cy+y, color); st7789_pixel(cx-x, cy+y, color);
        st7789_pixel(cx+x, cy-y, color); st7789_pixel(cx-x, cy-y, color);
        st7789_pixel(cx+y, cy+x, color); st7789_pixel(cx-y, cy+x, color);
        st7789_pixel(cx+y, cy-x, color); st7789_pixel(cx-y, cy-x, color);
        y++;
        if (err < 0) err += 2*y + 1;
        else { x--; err += 2*(y - x) + 1; }
    }
}

void oled_draw_line(int x0, int y0, int x1, int y1, uint16_t color) {
    int dx = abs(x1-x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1-y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        st7789_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void oled_draw_center_fader(int x, int y, int w, int h,
                            float value, uint16_t color) {
    /* value 0..1023 mapped so 512=center=zero */
    float norm = (value - 512.0f) / 512.0f;  /* -1..+1 */
    if (norm < -1.0f) norm = -1.0f;
    if (norm >  1.0f) norm =  1.0f;

    /* Background track */
    st7789_fill_rect(x, y, w, h, RGB565(40, 40, 40));

    /* Center tick */
    int cx = x + w / 2;
    st7789_vline(cx, y, h, RGB565(100, 100, 100));

    /* Filled portion from center */
    int half = w / 2;
    if (norm > 0) {
        int filled = (int)(half * norm);
        if (filled > 0)
            st7789_fill_rect(cx + 1, y, filled, h, color);
    } else if (norm < 0) {
        int filled = (int)(half * (-norm));
        if (filled > 0)
            st7789_fill_rect(cx - filled, y, filled, h, color);
    }
}

void oled_set_cue_overlay(const int states[4], const double positions[4]) {
    memcpy(overlay_cue_states, states, sizeof(overlay_cue_states));
    if (positions)
        memcpy(overlay_cue_positions, positions, sizeof(overlay_cue_positions));
    overlay_enabled = true;
}

void oled_draw_cue_bar(const int states[4], const double positions[4]) {
    /* Clear cue bar area */
    st7789_fill_rect(0, CUE_BAR_Y, DISPLAY_WIDTH, CUE_BAR_HEIGHT, THEME_BG);

    int total_gap = CUE_BAR_PAD * 2 + CUE_BTN_GAP * 3;
    int btn_w = (DISPLAY_WIDTH - total_gap) / 4;

    static const uint16_t cue_colors[4] = {
        THEME_CUE1_COLOR, THEME_CUE2_COLOR,
        THEME_CUE3_COLOR, THEME_CUE4_COLOR
    };

    for (int i = 0; i < 4; i++) {
        int x = CUE_BAR_PAD + i * (btn_w + CUE_BTN_GAP);
        uint16_t color;
        if (states[i] == CUE_STATE_ACTIVE)
            color = THEME_CUE_ACTIVE;
        else if (states[i] == CUE_STATE_SET)
            color = cue_colors[i];
        else
            color = THEME_CUE_EMPTY;
        st7789_fill_rect(x, CUE_BAR_Y, btn_w, CUE_BAR_HEIGHT - 2, color);

        /* Show short time (m:ss) if set */
        if (states[i] != CUE_STATE_EMPTY && positions &&
            !isinf(positions[i])) {
            double s = positions[i];
            if (s < 0) s = 0;
            int mins = (int)s / 60;
            int secs = (int)s % 60;
            char tstr[16];
            snprintf(tstr, sizeof(tstr), "%d:%02d", mins, secs);
            int tw = st7789_string_width(tstr, FONT_SMALL);
            int tx = x + (btn_w - tw) / 2;
            st7789_draw_string(tx, CUE_BAR_Y + 6, tstr,
                               THEME_BG, FONT_SMALL);
        }
    }
}

int oled_compute_scroll(int selected, int scroll_offset, int visible_lines) {
    if (selected < scroll_offset)
        scroll_offset = selected;
    if (selected >= scroll_offset + visible_lines)
        scroll_offset = selected - visible_lines + 1;
    return scroll_offset;
}
