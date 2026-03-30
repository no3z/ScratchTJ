#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "lcd_menu.h"
#include "main_menu.h"
#include "deck_menu.h"
#include "controller_menu.h"
#include "info_menu.h"
#include "player.h"
#include "track.h"
#include "cues.h"
#include "sc_input.h"
#include "shared_variables.h"

extern bool needsUpdate;
extern MainMenuState mainMenuState;

/* Dynamic menu labels — Deck 2 first */
static char menuLabelBuf[5][40];
static const char *mainMenuOptions[5];
static int selectedItem = 0;
static int menuSize = 5;

/* ── Cue flash state ──────────────────────────────────────────── */
static int prev_cue_states[4] = {0, 0, 0, 0};
static unsigned long cue_flash_time = 0;
static int cue_flash_idx = -1;
static unsigned long cue_trigger_time[4] = {0, 0, 0, 0};

static unsigned long main_millis(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/* ── Helpers ───────────────────────────────────────────────────── */

static const char *get_track_filename(struct deck *d) {
    if (!d->player.track || !d->player.track->path)
        return "No track";
    const char *slash = strrchr(d->player.track->path, '/');
    return slash ? slash + 1 : d->player.track->path;
}

static double get_track_duration(struct deck *d) {
    if (!d->player.track || d->player.track->rate == 0)
        return 0.0;
    return (double)d->player.track->length / d->player.track->rate;
}

/* Build one deck label: "Dk2 > filename 1:23" */
static void build_deck_label(char *buf, int bufsize,
                             struct deck *d, int deck_num) {
    bool playing = player_is_active(&d->player);
    const char *fname = get_track_filename(d);

    if (d->player.track && d->player.track->path) {
        double elapsed = player_get_elapsed(&d->player);
        char t[16];
        oled_format_time(elapsed, t, sizeof(t));
        snprintf(buf, bufsize, "Dk%d %c %.10s %s",
                 deck_num, playing ? '>' : '=', fname, t);
    } else {
        snprintf(buf, bufsize, "Dk%d  No track", deck_num);
    }
}

/* Build main menu labels — Deck 2 first */
static void build_main_menu_labels(struct deck *decks[], int deck_count) {
    if (deck_count >= 2)
        build_deck_label(menuLabelBuf[0], sizeof(menuLabelBuf[0]),
                         decks[1], 2);
    if (deck_count >= 1)
        build_deck_label(menuLabelBuf[1], sizeof(menuLabelBuf[1]),
                         decks[0], 1);
    snprintf(menuLabelBuf[2], sizeof(menuLabelBuf[2]), "Record Dk2");
    snprintf(menuLabelBuf[3], sizeof(menuLabelBuf[3]), "Config");
    snprintf(menuLabelBuf[4], sizeof(menuLabelBuf[4]), "Info");
    for (int i = 0; i < 5; i++)
        mainMenuOptions[i] = menuLabelBuf[i];
}

/* ── Home Screen — Deck 2 focused ─────────────────────────────── */

static void draw_platter(struct deck *d, int cx, int cy, int r,
                         uint16_t accent) {
    uint16_t ring_color;
    if (capIsTouched) {
        /* Touched: double bright ring */
        ring_color = accent;
        oled_draw_circle(cx, cy, r, ring_color);
        oled_draw_circle(cx, cy, r - 1, ring_color);
        oled_draw_circle(cx, cy, r - 2, ring_color);
    } else {
        /* Not touched: single dim ring */
        ring_color = RGB565(80, 80, 80);
        oled_draw_circle(cx, cy, r, ring_color);
        oled_draw_circle(cx, cy, r - 1, ring_color);
    }

    /* Inner hub */
    int hub_r = r / 4;
    oled_draw_circle(cx, cy, hub_r, RGB565(60, 60, 60));

    int needle_inner = hub_r + 2;
    int needle_outer = r - 3;

    /* ── Ghost needle (audio position) — drawn first, underneath ── */
    float ps = 1.0f;
    get_variable_value("platterspeed", &ps);
    double ghost_raw = d->player.position * (double)ps;
    int ghost_angle = ((int)fmod(ghost_raw, 16384.0) + 16384) % 16384;
    float ghost_deg = (ghost_angle * 360.0f) / 16384.0f;
    float ghost_rad = (ghost_deg - 90.0f) * (float)M_PI / 180.0f;
    int gx1 = cx + (int)(needle_inner * cosf(ghost_rad));
    int gy1 = cy + (int)(needle_inner * sinf(ghost_rad));
    int gx2 = cx + (int)(needle_outer * cosf(ghost_rad));
    int gy2 = cy + (int)(needle_outer * sinf(ghost_rad));
    /* 2px thick: draw center line + 1px perpendicular offset */
    oled_draw_line(gx1, gy1, gx2, gy2, THEME_DECK2_ACCENT_DIM);
    float perp_rad = ghost_rad + (float)M_PI / 2.0f;
    int offx = (int)roundf(cosf(perp_rad));
    int offy = (int)roundf(sinf(perp_rad));
    oled_draw_line(gx1 + offx, gy1 + offy, gx2 + offx, gy2 + offy, THEME_DECK2_ACCENT_DIM);

    /* ── Physical needle (encoder position) — drawn on top ── */
    float phys_deg = (d->encoderAngle * 360.0f) / 16384.0f;
    float phys_rad = (phys_deg - 90.0f) * (float)M_PI / 180.0f;
    int px1 = cx + (int)(needle_inner * cosf(phys_rad));
    int py1 = cy + (int)(needle_inner * sinf(phys_rad));
    int px2 = cx + (int)(needle_outer * cosf(phys_rad));
    int py2 = cy + (int)(needle_outer * sinf(phys_rad));
    oled_draw_line(px1, py1, px2, py2, accent);

    /* Elapsed time in center of hub (loop-aware) */
    double hub_duration = get_track_duration(d);
    double hub_elapsed_raw = d->player.track ? player_get_elapsed(&d->player) : 0.0;
    double hub_elapsed = (hub_duration > 0) ? fmod(hub_elapsed_raw, hub_duration) : hub_elapsed_raw;
    char tbuf[16];
    oled_format_time(hub_elapsed, tbuf, sizeof(tbuf));
    int tw = st7789_string_width(tbuf, FONT_SMALL);
    oled_text_color(cx - tw / 2, cy - 5, tbuf, FONT_SMALL, THEME_TEXT);
}

static void draw_deck_compact(struct deck *d, int deck_num,
                              uint16_t accent, int y) {
    char buf[40];
    bool playing = player_is_active(&d->player);

    /* Accent stripe + title */
    oled_fill_rect(0, y, DISPLAY_WIDTH, 18, RGB565(0, 20, 40));
    oled_fill_rect(0, y, 3, 18, accent);

    snprintf(buf, sizeof(buf), "Dk%d", deck_num);
    oled_text_color(6, y + 2, buf, FONT_SMALL, accent);
    oled_text_color(30, y + 2, playing ? ">" : "=", FONT_SMALL,
                    playing ? THEME_PLAYING : THEME_STOPPED);
    oled_text_color(42, y + 2, get_track_filename(d), FONT_SMALL, THEME_TEXT);

    /* Time right-aligned (loop-aware) */
    double compact_duration = get_track_duration(d);
    double compact_elapsed_raw = d->player.track ? player_get_elapsed(&d->player) : 0.0;
    double compact_elapsed = (compact_duration > 0) ? fmod(compact_elapsed_raw, compact_duration) : compact_elapsed_raw;
    char t[8];
    oled_format_time(compact_elapsed, t, sizeof(t));
    int tw = st7789_string_width(t, FONT_SMALL);
    oled_text_color(DISPLAY_WIDTH - tw - 4, y + 2, t, FONT_SMALL, THEME_TEXT);

    /* Mini progress bar below */
    float progress = (compact_duration > 0) ? (float)(compact_elapsed / compact_duration) : 0.0f;
    oled_draw_progress_bar(6, y + 14, DISPLAY_WIDTH - 12, 3, progress, accent);
}

void display_home_screen(struct deck *decks[], int deck_count) {
    oled_clear();

    if (deck_count < 2) return;

    struct deck *d2 = decks[1];
    char buf[40];
    bool playing = player_is_active(&d2->player);

    /* ── Compact title bar (y=0..16) ─────────────────────────── */
    oled_fill_rect(0, 0, DISPLAY_WIDTH, 16, RGB565(0, 20, 40));
    oled_fill_rect(0, 0, 4, 16, THEME_DECK2_ACCENT);

    snprintf(buf, sizeof(buf), "Dk2 %c %.16s",
             playing ? '>' : '=', get_track_filename(d2));
    oled_text_color(8, 2, buf, FONT_SMALL, THEME_TEXT);

    snprintf(buf, sizeof(buf), "%.2fx", d2->player.pitch);
    int pw = st7789_string_width(buf, FONT_SMALL);
    oled_text_color(DISPLAY_WIDTH - pw - 4, 2, buf, FONT_SMALL,
                    THEME_DECK2_ACCENT);

    /* ── Large platter shifted left (y=18..148, cx=76, r=58) ── */
    draw_platter(d2, 76, 83, 58, THEME_DECK2_ACCENT);

    /* ── Info column right of platter (x=145..236) ───────────── */
    {
        uint16_t dim = RGB565(130, 130, 130);
        float ps_val = 1.0f;
        get_variable_value("platterspeed", &ps_val);
        snprintf(buf, sizeof(buf), "Spd: %d", (int)ps_val);
        oled_text_color(145, 24, buf, FONT_SMALL, dim);

        snprintf(buf, sizeof(buf), "Pitch: %.2fx", d2->player.pitch);
        oled_text_color(145, 38, buf, FONT_SMALL, dim);

        snprintf(buf, sizeof(buf), "Motor: %.2f", d2->player.motor_speed);
        oled_text_color(145, 52, buf, FONT_SMALL, dim);

        float slip_val = 200.0f;
        get_variable_value("slippiness", &slip_val);
        snprintf(buf, sizeof(buf), "Slip: %.0f", slip_val);
        oled_text_color(145, 66, buf, FONT_SMALL, dim);
    }

    /* ── Full-width progress bar (y=150..156) — loop-aware ──── */
    double elapsed_raw = d2->player.track ? player_get_elapsed(&d2->player) : 0.0;
    double duration = get_track_duration(d2);
    double elapsed = (duration > 0) ? fmod(elapsed_raw, duration) : elapsed_raw;
    float progress = (duration > 0) ? (float)(elapsed / duration) : 0.0f;
    oled_draw_progress_bar(4, 150, 232, 6, progress, THEME_DECK2_ACCENT);

    /* ── Time + Fader row (y=160..172) ───────────────────────── */
    {
        double remain = duration - elapsed;
        if (remain < 0) remain = 0;

        char e[16], r[16];
        oled_format_time(elapsed, e, sizeof(e));
        oled_format_time(remain, r, sizeof(r));

        /* Elapsed left */
        oled_text_color(4, 160, e, FONT_SMALL, THEME_TEXT);

        /* Center fader */
        oled_draw_center_fader(60, 162, 120, 8,
                               (float)ADCs[0], RGB565(200, 200, 200));

        /* Remaining right */
        char rbuf[20];
        snprintf(rbuf, sizeof(rbuf), "-%s", r);
        int rw = st7789_string_width(rbuf, FONT_SMALL);
        oled_text_color(DISPLAY_WIDTH - rw - 4, 160, rbuf, FONT_SMALL,
                        remain < 30.0 ? RGB565(255, 80, 80) : RGB565(130, 130, 130));
    }

    /* ── Deck 1 compact strip (y=176..194) ───────────────────── */
    draw_deck_compact(decks[0], 1, THEME_DECK1_ACCENT, 176);

    /* ── Cue flash logic ──────────────────────────────────────── */
    {
        unsigned long now = main_millis();
        for (int i = 0; i < 4; i++) {
            /* Detect cue set transition → show banner */
            if (cue_display_states[i] == CUE_STATE_SET &&
                prev_cue_states[i] != CUE_STATE_SET) {
                cue_flash_idx = i;
                cue_flash_time = now;
            }
            /* Detect cue trigger transition → start color flash */
            if (cue_display_states[i] == CUE_STATE_ACTIVE &&
                prev_cue_states[i] != CUE_STATE_ACTIVE) {
                cue_trigger_time[i] = now;
            }
            /* Revert triggered cue back to set after 300ms */
            if (cue_display_states[i] == CUE_STATE_ACTIVE &&
                cue_trigger_time[i] > 0 &&
                (now - cue_trigger_time[i]) >= 300) {
                cue_display_states[i] = CUE_STATE_SET;
                cue_trigger_time[i] = 0;
            }
            prev_cue_states[i] = cue_display_states[i];
        }
        if (cue_flash_idx >= 0 && (now - cue_flash_time) < 500) {
            snprintf(buf, sizeof(buf), "CUE %d SET", cue_flash_idx + 1);
            int bw = st7789_string_width(buf, FONT_MEDIUM);
            oled_text_color((DISPLAY_WIDTH - bw) / 2, 198, buf, FONT_MEDIUM,
                            THEME_CUE_SET);
        } else {
            cue_flash_idx = -1;
        }
    }

    /* Cue bar drawn by overlay in oled_flush() at y=220 */
    oled_flush();
}

/* ── Display dispatch ──────────────────────────────────────────── */

void display_main_menu(struct deck *decks[], int deck_count) {
    if (mainMenuState == MENU_HOME) {
        display_home_screen(decks, deck_count);
    } else if (mainMenuState == MENU_MAIN) {
        oled_clear();
        build_main_menu_labels(decks, deck_count);
        oled_draw_title_bar("Main Menu");
        oled_draw_menu_list(mainMenuOptions, menuSize, selectedItem,
                            0, MENU_VISIBLE_LINES);
        oled_flush();
    } else {
        switch (mainMenuState) {
            case MENU_DECK1:
                display_deck_menu(decks[0], 0);
                break;
            case MENU_DECK2:
                display_deck_menu(decks[1], 1);
                break;
            case MENU_CONTROLLER:
                display_controller_menu(decks[1], 1);
                break;
            case MENU_INFO:
                display_info_menu_actions();
                break;
            case MENU_RECORD:
                display_deck_menu(decks[1], 1);
                break;
            default:
                break;
        }
    }
}

/* ── Navigation dispatch ───────────────────────────────────────── */

void handle_main_menu_navigation(struct deck *decks[], int deck_count) {
    if (mainMenuState == MENU_HOME) {
        int encoder_movement = rotary_encoder_moved();
        int button_press = rotary_button_pressed();
        int kb0 = kb0_button_pressed();

        /* Rotary movement or short press or KB0 short → enter menu */
        if (encoder_movement != 0 || button_press == 1 || kb0 == 1) {
            mainMenuState = MENU_MAIN;
            selectedItem = 0;
            needsUpdate = true;
        }
        return;
    }

    if (mainMenuState == MENU_MAIN) {
        int encoder_movement = rotary_encoder_moved();
        int button_press = rotary_button_pressed();
        int kb0 = kb0_button_pressed();

        if (encoder_movement != 0) {
            selectedItem = (selectedItem + encoder_movement + menuSize) % menuSize;
            needsUpdate = true;
        }

        /* KB0 = select */
        if (kb0 == 1) {
            switch (selectedItem) {
                case 0: mainMenuState = MENU_DECK2; deck_menu_reset(); break;
                case 1: mainMenuState = MENU_DECK1; deck_menu_reset(); break;
                case 2: mainMenuState = MENU_RECORD;
                        action_record(decks[1], 1);
                        break;
                case 3: mainMenuState = MENU_CONTROLLER; break;
                case 4: mainMenuState = MENU_INFO; break;
                default: break;
            }
            selectedItem = 0;
            needsUpdate = true;
        }

        /* Rotary click = back to home */
        if (button_press == 1) {
            mainMenuState = MENU_HOME;
            needsUpdate = true;
        }
    } else {
        /* Submenus handle their own KB0 — don't consume it here */
        switch (mainMenuState) {
            case MENU_DECK1:
                handle_deck_menu_navigation(decks[0], 0);
                break;
            case MENU_DECK2:
                handle_deck_menu_navigation(decks[1], 1);
                break;
            case MENU_CONTROLLER:
                handle_controller_menu_navigation(decks[0], 1);
                break;
            case MENU_INFO:
                handle_info_menu_navigation();
                break;
            case MENU_RECORD:
                handle_deck_menu_navigation(decks[1], 1);
                break;
            default:
                break;
        }
    }
}
