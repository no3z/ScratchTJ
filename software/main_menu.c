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
#include "deck.h"

extern bool needsUpdate;
extern MainMenuState mainMenuState;

/* Dynamic menu labels — Deck 2 first */
static char menuLabelBuf[7][40];
static const char *mainMenuOptions[7];
static int selectedItem = 0;
static int menuSize = 7;

/* ── Function mode overlay state ──────────────────────────────── */
static unsigned long mode_overlay_start_time = 0;
#define MODE_OVERLAY_DURATION_MS 1500

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
    snprintf(menuLabelBuf[2], sizeof(menuLabelBuf[2]), "Randomize");
    snprintf(menuLabelBuf[3], sizeof(menuLabelBuf[3]), "Record Dk2");
    snprintf(menuLabelBuf[4], sizeof(menuLabelBuf[4]), "Btn: %s",
             current_function_mode == FUNC_MODE_SETTINGS ? "SETTINGS" : "CUE");
    snprintf(menuLabelBuf[5], sizeof(menuLabelBuf[5]), "Config");
    snprintf(menuLabelBuf[6], sizeof(menuLabelBuf[6]), "Info");
    for (int i = 0; i < 7; i++)
        mainMenuOptions[i] = menuLabelBuf[i];
}

/* ── Function mode display helpers ─────────────────────────────── */

static void draw_mode_overlay(void) {
    unsigned long now = main_millis();
    if (mode_overlay_start_time == 0) return;
    unsigned long elapsed = now - mode_overlay_start_time;
    if (elapsed >= MODE_OVERLAY_DURATION_MS) {
        mode_overlay_start_time = 0;
        return;
    }

    /* Dark box centered on platter area */
    oled_fill_rect(10, 70, 220, 70, RGB565(10, 10, 30));
    /* Border in deck accent color */
    uint16_t color = (active_deck == 1) ? THEME_DECK2_ACCENT : THEME_DECK1_ACCENT;
    oled_draw_line(10, 70, 229, 70, color);
    oled_draw_line(10, 139, 229, 139, color);

    /* Big label: DECK 1 or DECK 2 */
    char label[16];
    snprintf(label, sizeof(label), "DECK %d", active_deck + 1);
    int tw = st7789_string_width(label, FONT_LARGE);
    oled_text_color((DISPLAY_WIDTH - tw) / 2, 90, label, FONT_LARGE, color);

    /* Subtitle: button mode */
    const char *sub = (current_function_mode == FUNC_MODE_SETTINGS)
                      ? "Buttons: SETTINGS" : "Buttons: CUE";
    int sw = st7789_string_width(sub, FONT_SMALL);
    oled_text_color((DISPLAY_WIDTH - sw) / 2, 118, sub, FONT_SMALL,
                    RGB565(160, 160, 160));
}

static const char *settings_param_labels[4] = {
    "PITCH", "SPEED", "BEND", "VOLUME"
};
static const char *settings_param_names[4] = {
    NULL, "platterspeed", NULL, NULL
};

static void draw_settings_value_overlay(struct deck *d2) {
    if (current_function_mode != FUNC_MODE_SETTINGS || settings_buttons_held == 0)
        return;

    /* Find first held button */
    int btn = -1;
    for (int i = 0; i < 4; i++) {
        if (settings_buttons_held & (1 << i)) { btn = i; break; }
    }
    if (btn < 0) return;

    /* Get current value */
    float val = 0;
    if (btn == 0) {
        val = (float)d2->player.note_pitch;
    } else if (btn == 2) {
        val = (float)d2->player.fader_pitch;  /* bend ±8% */
    } else if (btn == 3) {
        val = (float)d2->player.setVolume;  /* gain multiplier */
    } else if (settings_param_names[btn]) {
        get_variable_value(settings_param_names[btn], &val);
    }

    /* Dark overlay on platter area */
    oled_fill_rect(10, 40, 125, 80, RGB565(5, 5, 20));

    /* Parameter name */
    uint16_t accent = RGB565(0, 180, 255);
    oled_text_color(18, 48, settings_param_labels[btn], FONT_MEDIUM, accent);

    /* Large value */
    char vbuf[16];
    if (btn == 0) snprintf(vbuf, sizeof(vbuf), "%.3f", val);                 /* pitch */
    else if (btn == 1) snprintf(vbuf, sizeof(vbuf), "%.0f", val);             /* platterspeed */
    else if (btn == 2) snprintf(vbuf, sizeof(vbuf), "%+.1f%%", (val - 1.0f) * 100.0f); /* bend % */
    else if (btn == 3) snprintf(vbuf, sizeof(vbuf), "x%.2f", val);            /* gain */
    else snprintf(vbuf, sizeof(vbuf), "%.1f", val);
    oled_text_color(18, 72, vbuf, FONT_LARGE, COLOR_WHITE);

    /* Visual gauge bar */
    float norm = 0;
    if (btn == 0) norm = (val - 0.25f) / 3.75f;          /* note_pitch 0.25-4.0 */
    else if (btn == 1) norm = val / 32768.0f;             /* platterspeed */
    else if (btn == 2) norm = (val - 0.92f) / 0.16f;      /* bend 0.92-1.08 (centered ±8%) */
    else if (btn == 3) norm = val / 8.0f;                 /* gain (max 8x with soft-clip) */
    if (norm < 0) norm = 0;
    if (norm > 1) norm = 1;
    oled_draw_progress_bar(18, 100, 110, 6, norm, accent);
}

static void draw_function_mode_strip(int y) {
    uint16_t bg = RGB565(15, 10, 30);
    oled_fill_rect(0, y, DISPLAY_WIDTH, 14, bg);

    if (current_function_mode == FUNC_MODE_CUE) {
        oled_fill_rect(0, y, 3, 14, RGB565(255, 200, 0));
        oled_text_color(6, y + 3, "CUE", FONT_SMALL, RGB565(255, 200, 0));
        oled_text_color(32, y + 3, "1:J/S 2:J/S 3:J/S 4:J/S", FONT_SMALL,
                        RGB565(120, 120, 120));
    } else {
        oled_fill_rect(0, y, 3, 14, RGB565(0, 180, 255));
        oled_text_color(6, y + 3, "SET", FONT_SMALL, RGB565(0, 180, 255));
        /* Color-coded button labels */
        oled_text_color(32, y + 3, "1:", FONT_SMALL, THEME_CUE1_COLOR);
        oled_text_color(48, y + 3, "Pit", FONT_SMALL, RGB565(160, 160, 160));
        oled_text_color(76, y + 3, "2:", FONT_SMALL, THEME_CUE2_COLOR);
        oled_text_color(92, y + 3, "Spd", FONT_SMALL, RGB565(160, 160, 160));
        oled_text_color(120, y + 3, "3:", FONT_SMALL, THEME_CUE3_COLOR);
        oled_text_color(136, y + 3, "S/S", FONT_SMALL, RGB565(160, 160, 160));
        oled_text_color(168, y + 3, "4:", FONT_SMALL, THEME_CUE4_COLOR);
        oled_text_color(184, y + 3, "Vol", FONT_SMALL, RGB565(160, 160, 160));
    }
}

/* ── Home Screen — Deck 2 focused ─────────────────────────────── */

static void draw_platter(struct deck *d, int cx, int cy, int r,
                         uint16_t accent) {
    /* ── Fader arc: ring fills CW from 12 o'clock proportional to faderVolume ── */
    float fvol = d->player.faderVolume;
    if (fvol < 0.0f) fvol = 0.0f;
    if (fvol > 1.0f) fvol = 1.0f;

    /* Always draw dim base ring */
    oled_draw_circle(cx, cy, r, RGB565(40, 40, 40));
    oled_draw_circle(cx, cy, r - 1, RGB565(40, 40, 40));

    if (fvol > 0.01f) {
        /* Draw lit arc from -90° (top) CW for fvol*360° */
        float arc_deg = fvol * 360.0f;
        int steps = (int)(arc_deg * 0.5f); /* ~2 pixels per degree */
        if (steps < 1) steps = 1;
        for (int s = 0; s <= steps; s++) {
            float deg = -90.0f + (arc_deg * s / steps);
            float rad = deg * (float)M_PI / 180.0f;
            float cs = cosf(rad), sn = sinf(rad);
            /* Draw 3px thick arc */
            for (int rr = r - 2; rr <= r; rr++) {
                int px = cx + (int)(rr * cs);
                int py = cy + (int)(rr * sn);
                if (px >= 0 && px < 240 && py >= 0 && py < 240)
                    st7789_pixel(px, py, accent);
            }
        }
    }

    /* Touch: extra bright inner glow ring */
    if (capIsTouched) {
        oled_draw_circle(cx, cy, r - 3, accent);
    }

    /* 12 tick marks around the platter */
    for (int t = 0; t < 12; t++) {
        float tick_rad = (t * 30.0f - 90.0f) * (float)M_PI / 180.0f;
        int tick_inner = r - 8;
        int tick_outer = r - 3;
        if (t % 3 == 0) tick_inner = r - 12;
        int tx1 = cx + (int)(tick_inner * cosf(tick_rad));
        int ty1 = cy + (int)(tick_inner * sinf(tick_rad));
        int tx2 = cx + (int)(tick_outer * cosf(tick_rad));
        int ty2 = cy + (int)(tick_outer * sinf(tick_rad));
        uint16_t tick_color = (t % 3 == 0) ? RGB565(140, 140, 140) : RGB565(80, 80, 80);
        oled_draw_line(tx1, ty1, tx2, ty2, tick_color);
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

    int ad = active_deck;
    int other = 1 - ad;
    struct deck *d_hero = decks[ad];
    struct deck *d_other = decks[other];
    uint16_t hero_accent = (ad == 1) ? THEME_DECK2_ACCENT : THEME_DECK1_ACCENT;
    uint16_t other_accent = (ad == 1) ? THEME_DECK1_ACCENT : THEME_DECK2_ACCENT;
    char buf[40];
    bool playing = player_is_active(&d_hero->player);

    /* ── Compact title bar (y=0..16) ─────────────────────────── */
    oled_fill_rect(0, 0, DISPLAY_WIDTH, 16, RGB565(0, 20, 40));
    oled_fill_rect(0, 0, 4, 16, hero_accent);

    snprintf(buf, sizeof(buf), "Dk%d %c %.16s",
             ad + 1, playing ? '>' : '=', get_track_filename(d_hero));
    oled_text_color(8, 2, buf, FONT_SMALL, THEME_TEXT);

    snprintf(buf, sizeof(buf), "%.2fx", d_hero->player.pitch);
    int pw = st7789_string_width(buf, FONT_SMALL);
    oled_text_color(DISPLAY_WIDTH - pw - 4, 2, buf, FONT_SMALL,
                    hero_accent);

    /* ── Mode indicator in title bar ─────────────────────────── */
    if (current_function_mode == FUNC_MODE_SETTINGS) {
        oled_fill_rect(DISPLAY_WIDTH - pw - 18, 4, 10, 8, RGB565(0, 180, 255));
        oled_text_color(DISPLAY_WIDTH - pw - 17, 4, "S", FONT_SMALL, COLOR_WHITE);
    }

    /* ── Large platter shifted left (y=18..148, cx=76, r=58) ── */
    draw_platter(d_hero, 76, 83, 58, hero_accent);

    /* ── Info column right of platter (x=145..236) ───────────── */
    {
        /* Text labels first */
        uint16_t dim = RGB565(160, 160, 160);
        float ps_val = 1.0f;
        get_variable_value("platterspeed", &ps_val);
        snprintf(buf, sizeof(buf), "Spd: %d", (int)ps_val);
        oled_text_color(145, 24, buf, FONT_SMALL, dim);

        snprintf(buf, sizeof(buf), "Pitch: %.2fx", d_hero->player.pitch);
        oled_text_color(145, 38, buf, FONT_SMALL, dim);

        snprintf(buf, sizeof(buf), "Motor: %.2f", d_hero->player.motor_speed);
        oled_text_color(145, 52, buf, FONT_SMALL, dim);

        float slip_val = 200.0f;
        get_variable_value("slippiness", &slip_val);
        snprintf(buf, sizeof(buf), "Slip: %.0f", slip_val);
        oled_text_color(145, 66, buf, FONT_SMALL, dim);

        /* ── Fader history graph below text (right of platter) ─── */
        #define FHIST_LEN 90
        #define FHIST_X   145
        #define FHIST_Y   82
        #define FHIST_H   55
        /* Auto-zoom: find max value in history buffer and scale to fill graph */
        static float fader_history[FHIST_LEN] = {0};
        static int fhist_idx = 0;

        /* Push 1 sample per display frame → smooth 1px/frame scroll.
         * Visible window = FHIST_LEN * HOME_REDRAW_INTERVAL_MS (~2sec at 22ms) */
        fader_history[fhist_idx] = d_hero->player.faderVolume;
        fhist_idx = (fhist_idx + 1) % FHIST_LEN;

        /* Auto-zoom: find peak in history, scale so peak fills ~90% of graph */
        float hist_max = 0.001f;
        for (int i = 0; i < FHIST_LEN; i++) {
            if (fader_history[i] > hist_max) hist_max = fader_history[i];
        }
        float auto_zoom = 0.9f / hist_max;
        if (auto_zoom > 50.0f) auto_zoom = 50.0f;  /* cap zoom when silent */
        if (auto_zoom < 1.0f) auto_zoom = 1.0f;

        /* Baseline dim line */
        oled_draw_line(FHIST_X, FHIST_Y + FHIST_H, FHIST_X + FHIST_LEN,
                       FHIST_Y + FHIST_H, RGB565(30, 30, 30));

        /* Draw scrolling history: oldest left, newest right */
        for (int i = 0; i < FHIST_LEN - 1; i++) {
            int si  = (fhist_idx + i) % FHIST_LEN;
            int si2 = (fhist_idx + i + 1) % FHIST_LEN;
            float v1 = fader_history[si] * auto_zoom;
            float v2 = fader_history[si2] * auto_zoom;
            if (v1 > 1.0f) v1 = 1.0f;
            if (v2 > 1.0f) v2 = 1.0f;
            int x1 = FHIST_X + i;
            int x2 = FHIST_X + i + 1;
            int y1 = FHIST_Y + FHIST_H - (int)(v1 * FHIST_H);
            int y2 = FHIST_Y + FHIST_H - (int)(v2 * FHIST_H);

            /* Bright fade: older = dimmer, newest = full accent */
            int age = (FHIST_LEN - 1 - i);
            int r_c = 255 - age * 2; if (r_c < 40) r_c = 40;
            int g_c = 140 - age;     if (g_c < 20) g_c = 20;
            oled_draw_line(x1, y1, x2, y2, RGB565(r_c, g_c, 0));

            /* Fill below the line for a solid waveform look */
            if (v1 > 0.01f) {
                int fill_bright = 60 - age / 2; if (fill_bright < 10) fill_bright = 10;
                for (int fy = y1 + 1; fy <= FHIST_Y + FHIST_H; fy++)
                    st7789_pixel(x1, fy, RGB565(fill_bright, fill_bright / 3, 0));
            }
        }
    }

    /* ── Full-width progress bar (y=150..156) — loop-aware ──── */
    double elapsed_raw = d_hero->player.track ? player_get_elapsed(&d_hero->player) : 0.0;
    double duration = get_track_duration(d_hero);
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

    /* ── Other deck compact strip (y=176..194) ─────────────── */
    draw_deck_compact(d_other, other + 1, other_accent, 176);

    /* ── Function mode info strip (y=196..210) ────────────────── */
    draw_function_mode_strip(196);

    /* ── Cue flash logic (y=212, moved from 198) ──────────────── */
    {
        unsigned long now = main_millis();
        for (int i = 0; i < 4; i++) {
            if (cue_display_states[i] == CUE_STATE_SET &&
                prev_cue_states[i] != CUE_STATE_SET) {
                cue_flash_idx = i;
                cue_flash_time = now;
            }
            if (cue_display_states[i] == CUE_STATE_ACTIVE &&
                prev_cue_states[i] != CUE_STATE_ACTIVE) {
                cue_trigger_time[i] = now;
            }
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
            oled_text_color((DISPLAY_WIDTH - bw) / 2, 212, buf, FONT_MEDIUM,
                            THEME_CUE_SET);
        } else {
            cue_flash_idx = -1;
        }
    }

    /* ── Settings value overlay (on top of platter when adjusting) ── */
    draw_settings_value_overlay(d_hero);

    /* ── Mode overlay (big temporal label, drawn last) ──────────── */
    draw_mode_overlay();

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

        /* Encoder turn or KB0 = enter main menu (forward/select) */
        if (encoder_movement != 0 || kb0 == 1) {
            mainMenuState = MENU_MAIN;
            selectedItem = 0;
            needsUpdate = true;
        }

        /* Short press on home = no-op (already at root) */

        /* Long press = switch active deck */
        if (button_press == 2) {
            active_deck = 1 - active_deck;
            mode_overlay_start_time = main_millis();
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
                case 2: /* Randomize both decks */
                        deck_random_file(decks[0]);
                        deck_random_file(decks[1]);
                        mainMenuState = MENU_HOME;
                        break;
                case 3: mainMenuState = MENU_RECORD;
                        action_record(decks[1], 1);
                        break;
                case 4: /* Toggle button mode CUE ↔ SETTINGS */
                        current_function_mode = !current_function_mode;
                        if (current_function_mode == FUNC_MODE_CUE)
                            save_variables_to_file("/home/no3z/.scratchtj/config.cfg");
                        break;
                case 5: mainMenuState = MENU_CONTROLLER; break;
                case 6: mainMenuState = MENU_INFO; break;
                default: break;
            }
            selectedItem = 0;
            needsUpdate = true;
        }

        /* Rotary short click = back to home */
        if (button_press == 1) {
            mainMenuState = MENU_HOME;
            needsUpdate = true;
        }

        /* Long press = switch active deck (works from any screen) */
        if (button_press == 2) {
            active_deck = 1 - active_deck;
            mode_overlay_start_time = main_millis();
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
