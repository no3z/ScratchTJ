/*
 * ScratchTJ v2 - TFT UI test (Deck 2 focused, -90° rotation)
 *
 * Build:  make test_tft_ui
 * Run:    sudo ./test_tft_ui
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include "gpio_direct.h"
#include "oled_display.h"

static volatile int running = 1;
static void sig_handler(int sig) { (void)sig; running = 0; }

/* ── Mock home screen (Deck 2 hero) ───────────────────────────── */
static void draw_mock_home(float elapsed2, float elapsed1, float fader) {
    oled_clear();

    /* ── Deck 2: hero section (y=0..88) ─────────────────────── */

    /* Title bar */
    oled_fill_rect(0, 0, DISPLAY_WIDTH, 28, RGB565(0, 30, 60));
    oled_fill_rect(0, 0, 4, 28, THEME_DECK2_ACCENT);
    oled_text_color(8, 6, "DECK 2", FONT_MEDIUM, THEME_DECK2_ACCENT);
    oled_text_color(110, 6, ">", FONT_MEDIUM, THEME_PLAYING);
    oled_text_color(180, 6, "1.00x", FONT_MEDIUM, THEME_TEXT);

    /* Filename */
    oled_text_color(8, 32, "funky_break.mp3", FONT_SMALL, THEME_DECK2_ACCENT);

    /* Progress bar + time */
    float prog2 = elapsed2 / 225.0f;
    oled_draw_progress_bar(4, 44, 152, 12, prog2, THEME_DECK2_ACCENT);
    char buf[32], e[8], d[8];
    oled_format_time(elapsed2, e, sizeof(e));
    oled_format_time(225.0, d, sizeof(d));
    snprintf(buf, sizeof(buf), "%s/%s", e, d);
    oled_text_color(162, 46, buf, FONT_SMALL, THEME_TEXT);

    /* Pitch large centered */
    char pbuf[16];
    snprintf(pbuf, sizeof(pbuf), "%.2fx", 1.0 + sinf(elapsed2 * 0.1f) * 0.05);
    int pw = st7789_string_width(pbuf, FONT_LARGE);
    oled_text_color((DISPLAY_WIDTH - pw) / 2, 62, pbuf, FONT_LARGE, THEME_PLAYING);

    oled_hline(0, 90, DISPLAY_WIDTH);

    /* ── Fader + status (y=94..122) ──────────────────────────── */
    oled_text_color(4, 94, "FADER", FONT_SMALL, RGB565(130, 130, 130));
    oled_draw_progress_bar(48, 94, 184, 10, fader, RGB565(200, 200, 200));

    oled_text_color(4, 110, "Touch:", FONT_SMALL, RGB565(130, 130, 130));
    oled_text_color(56, 110, "ON", FONT_SMALL, THEME_PLAYING);
    oled_text_color(100, 110, "Motor:1.00", FONT_SMALL, RGB565(130, 130, 130));

    oled_hline(0, 124, DISPLAY_WIDTH);

    /* ── Deck 1: compact (y=126..146) ────────────────────────── */
    oled_fill_rect(0, 126, DISPLAY_WIDTH, 18, RGB565(0, 20, 40));
    oled_fill_rect(0, 126, 3, 18, THEME_DECK1_ACCENT);
    oled_text_color(6, 128, "Dk1", FONT_SMALL, THEME_DECK1_ACCENT);
    oled_text_color(30, 128, "=", FONT_SMALL, THEME_STOPPED);
    oled_text_color(42, 128, "hiphop_acap.wav", FONT_SMALL, THEME_TEXT);

    char t1[8];
    oled_format_time(elapsed1, t1, sizeof(t1));
    int tw = st7789_string_width(t1, FONT_SMALL);
    oled_text_color(DISPLAY_WIDTH - tw - 4, 128, t1, FONT_SMALL, THEME_TEXT);

    float prog1 = elapsed1 / 130.0f;
    oled_draw_progress_bar(6, 140, DISPLAY_WIDTH - 12, 3, prog1,
                           THEME_DECK1_ACCENT);

    oled_hline(0, 148, DISPLAY_WIDTH);

    /* ── Volumes (y=152..168) ────────────────────────────────── */
    oled_text_color(4, 154, "V1", FONT_SMALL, THEME_DECK1_ACCENT);
    oled_draw_progress_bar(24, 154, 80, 8, 0.80f, THEME_DECK1_ACCENT);
    oled_text_color(120, 154, "V2", FONT_SMALL, THEME_DECK2_ACCENT);
    oled_draw_progress_bar(140, 154, 80, 8, 0.64f, THEME_DECK2_ACCENT);

    /* Cue bar drawn by overlay */
    oled_flush();
}

/* ── Mock deck info ────────────────────────────────────────────── */
static void draw_mock_deck_info(int deck_no, float elapsed) {
    oled_clear();
    uint16_t accent = deck_no == 0 ? THEME_DECK1_ACCENT : THEME_DECK2_ACCENT;

    char title[24];
    snprintf(title, sizeof(title), "Deck %d Info", deck_no + 1);
    oled_draw_title_bar(title);

    int y = 28;
    char buf[48];

    oled_text_color(4, y, "funky_break.mp3", FONT_MEDIUM, accent);
    y += 18;
    oled_text_color(4, y, "/music/breaks/", FONT_SMALL, RGB565(120, 120, 120));
    y += 14;

    float duration = 225.0f;
    char e[8], d[8];
    oled_format_time(elapsed, e, sizeof(e));
    oled_format_time(duration, d, sizeof(d));
    snprintf(buf, sizeof(buf), "Position  %s / %s", e, d);
    oled_text(4, y, buf, FONT_SMALL);
    y += 12;
    oled_draw_progress_bar(4, y, DISPLAY_WIDTH - 8, 8,
                           elapsed / duration, accent);
    y += 14;

    oled_text(4, y, "Pitch     1.00x", FONT_SMALL); y += 14;
    oled_text(4, y, "Volume    80", FONT_SMALL); y += 14;
    oled_text(4, y, "Touch     ON", FONT_SMALL); y += 14;
    oled_text(4, y, "Motor     1.00", FONT_SMALL); y += 18;
    oled_text(4, y, "Cues: [1 0:04] [2 1:32] [3   ] [4   ]", FONT_SMALL);

    oled_flush();
}

/* ── Mock main menu (Dk2 first) ────────────────────────────────── */
static void draw_mock_main_menu(int selected) {
    oled_clear();
    oled_draw_title_bar("Main Menu");
    const char *items[] = {
        "Dk2 > funky_br 1:23",
        "Dk1 = hiphop_a 0:42",
        "Config",
        "Info"
    };
    oled_draw_menu_list(items, 4, selected, 0, MENU_VISIBLE_LINES);
    oled_flush();
}

/* ── Main ──────────────────────────────────────────────────────── */
int main(void) {
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    printf("=== ScratchTJ TFT UI Test (Deck 2 focus, -90 rotation) ===\n\n");

    if (gpio_direct_init() < 0) {
        fprintf(stderr, "GPIO init failed (need sudo?)\n");
        return 1;
    }
    if (oled_init() < 0) {
        fprintf(stderr, "Display init failed\n");
        return 1;
    }

    /* Cue overlay with mock data */
    int cue_states[4] = { CUE_STATE_SET, CUE_STATE_SET,
                          CUE_STATE_EMPTY, CUE_STATE_EMPTY };
    double cue_positions[4] = { 4.2, 92.5, 0.0, 0.0 };
    oled_set_cue_overlay(cue_states, cue_positions);

    printf("Cycling screens (Ctrl+C to exit)...\n");

    float elapsed2 = 83.0f, elapsed1 = 42.0f, fader = 0.3f;
    int screen = 0, frame = 0, menu_sel = 0;

    while (running) {
        switch (screen) {
            case 0:
                draw_mock_home(elapsed2, elapsed1, fader);
                elapsed2 += 0.1f;
                if (elapsed2 > 225.0f) elapsed2 = 0.0f;
                elapsed1 += 0.05f;
                fader += 0.005f;
                if (fader > 1.0f) fader = 0.0f;
                break;
            case 1:
                draw_mock_main_menu(menu_sel);
                break;
            case 2:
                draw_mock_deck_info(1, elapsed2);
                elapsed2 += 0.1f;
                break;
            case 3:
                draw_mock_deck_info(0, elapsed1);
                elapsed1 += 0.1f;
                break;
        }

        frame++;
        if (frame % 50 == 0) {
            screen = (screen + 1) % 4;
            menu_sel = (menu_sel + 1) % 4;
            printf("  Screen %d/4\n", screen + 1);
        }

        usleep(100000);
    }

    printf("\nDone.\n");
    oled_clear();
    oled_flush();
    oled_close();
    return 0;
}
