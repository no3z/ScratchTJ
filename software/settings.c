/*
 * Runtime settings: defaults, ranges and the plain-English help shown
 * in the Config menu. See settings.h and docs/SETTINGS.md.
 *
 * Defaults follow the SC1000/SC500. Everything ScratchTJ adds on top
 * (speed assist, touch by motion, curve fader, fader smoothing, soft
 * clip) starts switched off, so a fresh unit plays like an SC1000.
 *
 * The config keys (the first field) are what config.cfg and presets
 * store: do not rename them, or saved settings stop loading.
 */

#include "settings.h"
#include "shared_variables.h"
#include "xwax.h"

/* Platter */
float cfg_platterspeed = 9100.0f;
float cfg_jogreverse = 1.0f;
float cfg_touch_threshold = 5000.0f;
float cfg_motion_touch = 0.0f;
float cfg_slippiness = 200.0f;
float cfg_brakespeed = 3000.0f;
float cfg_target_pitch = 40.0f;
float cfg_pitch_filter = 0.1f;
float cfg_vel_ff = 0.0f;
float cfg_blipthreshold = 400.0f;

/* Fader & Volume */
float cfg_fader_mode = FADER_MODE_SC1000;
float cfg_cutbeats = 0.0f;
float cfg_fader_open = 5.0f;
float cfg_fader_close = 3.0f;
float cfg_fader_switch = 1.0f;
float cfg_curve_factor = 0.1f;
float cfg_curve_power = 0.2f;
float cfg_fader_smooth = 0.0f;
float cfg_soft_clip = 0.0f;

/* Screen */
float cfg_screen_off = 10.0f;

static const char *const fader_mode_names[] = { "SC1000 cut", "Curve" };
static const char *const cutbeats_names[] = { "Off", "Left end", "Right end" };
static const char *const fader_dir_names[] = { "Reversed", "Normal" };

static const SettingInfo settings[] = {
    /* ── Platter ─────────────────────────────────────────────── */
    {
        .name = "platterspeed", .valuePtr = &cfg_platterspeed,
        .minValue = 1000, .maxValue = 32768, .stepSize = 25,
        .kind = VAR_NUMBER, .decimals = 0,
        .section = SECTION_PLATTER, .label = "Platter speed",
        .help = "How far you turn the platter for one second of sound. "
                "Higher means more turning for the same sound, like a "
                "faster record. 9100 feels like 33 rpm, 12288 like 45 rpm. "
                "SC1000 default: 9100.",
    },
    {
        .name = "jogreverse", .valuePtr = &cfg_jogreverse,
        .minValue = 0, .maxValue = 1, .stepSize = 1,
        .kind = VAR_ONOFF,
        .section = SECTION_PLATTER, .label = "Reverse platter",
        .help = "Flips the platter direction. Change it if turning the "
                "record forwards plays the sound backwards.",
    },
    {
        .name = "touch_threshold", .valuePtr = &cfg_touch_threshold,
        .minValue = 100, .maxValue = 65000, .stepSize = 100,
        .kind = VAR_NUMBER, .decimals = 0,
        .section = SECTION_PLATTER, .label = "Touch threshold",
        .help = "Touch sensor reading above which your hand counts as "
                "on the platter. Watch 'Sensor' with and without your "
                "hand on it and set this about halfway between. Too low: "
                "touches by itself. Too high: misses your hand.",
    },
    {
        .name = "motion_touch", .valuePtr = &cfg_motion_touch,
        .minValue = 0, .maxValue = 1, .stepSize = 1,
        .kind = VAR_ONOFF,
        .section = SECTION_PLATTER, .label = "Touch by motion",
        .help = "Also count the platter as touched while it is being "
                "turned. Only for when the touch sensor does not work: "
                "a held record starts playing as soon as you stop "
                "moving it, and a spinning platter keeps control for a "
                "moment after you let go. Default: Off.",
    },
    {
        .name = "slippiness", .valuePtr = &cfg_slippiness,
        .minValue = 1, .maxValue = 3000, .stepSize = 25,
        .kind = VAR_NUMBER, .decimals = 0,
        .section = SECTION_PLATTER, .label = "Slipmat",
        .help = "How fast the record gets back to normal speed when you "
                "let go of it, or after a backspin. Lower is faster, "
                "like a tight slipmat. Higher is slower, with more drag. "
                "SC1000 default: 200.",
    },
    {
        .name = "brakespeed", .valuePtr = &cfg_brakespeed,
        .minValue = 1, .maxValue = 10000, .stepSize = 250,
        .kind = VAR_NUMBER, .decimals = 0,
        .section = SECTION_PLATTER, .label = "Brake length",
        .help = "How long the track takes to stop when you press stop, "
                "like a turntable's brake. Higher is a slower stop. "
                "3000 is about half a second. SC1000 default: 3000.",
    },
    {
        .name = "target_pitch", .valuePtr = &cfg_target_pitch,
        .minValue = 1, .maxValue = 240, .stepSize = 1,
        .kind = VAR_NUMBER, .decimals = 0,
        .section = SECTION_PLATTER, .label = "Tracking gain",
        .help = "With your hand on the platter, how hard the sound is "
                "pulled to where the platter is. Higher is tighter but "
                "can buzz or wobble. Lower is smoother but lags behind "
                "your hand. SC1000 default: 40.",
    },
    {
        .name = "pitch_filter", .valuePtr = &cfg_pitch_filter,
        .minValue = 0.01f, .maxValue = 1, .stepSize = 0.01f,
        .kind = VAR_NUMBER, .decimals = 2,
        .section = SECTION_PLATTER, .label = "Pitch response",
        .help = "How quickly the speed of the sound follows changes, "
                "under your hand and when the record speeds back up "
                "after you let go. Lower is smoother but slower, higher "
                "is snappier but grainier. SC1000 default: 0.10.",
    },
    {
        .name = "vel_ff", .valuePtr = &cfg_vel_ff,
        .minValue = 0, .maxValue = 1, .stepSize = 0.1f,
        .kind = VAR_NUMBER, .decimals = 1, .zero_text = "Off",
        .section = SECTION_PLATTER, .label = "Speed assist",
        .help = "Feeds the platter's measured speed straight into the "
                "sound, so it follows your hand with less delay and "
                "fast scratches sound sharper. Off is the original "
                "SC1000 feel, 1.0 is full assist. Try 0.5 to 1.0.",
    },
    {
        .name = "blipthreshold", .valuePtr = &cfg_blipthreshold,
        .minValue = 50, .maxValue = 2048, .stepSize = 50,
        .kind = VAR_NUMBER, .decimals = 0,
        .section = SECTION_PLATTER, .label = "Glitch filter",
        .help = "A platter sensor reading that jumps further than this "
                "at once is taken as a bad read and skipped (twice in a "
                "row at most). Raise it if very fast spins stutter, "
                "lower it if you hear random jumps. Default: 400.",
    },

    /* ── Fader & Volume ──────────────────────────────────────── */
    {
        .name = "fader_mode", .valuePtr = &cfg_fader_mode,
        .minValue = 0, .maxValue = 1, .stepSize = 1,
        .kind = VAR_CHOICE, .choices = fader_mode_names,
        .section = SECTION_FADER, .label = "Fader mode",
        .help = "SC1000 cut: the beats always play and the sample plays "
                "at full volume, cutting only at the very ends of the "
                "fader, like a scratch mixer. Curve: the fader "
                "crossfades between beats and sample, shaped by Curve "
                "width and Curve shape.",
    },
    {
        .name = "cutbeats", .valuePtr = &cfg_cutbeats,
        .minValue = 0, .maxValue = 2, .stepSize = 1,
        .kind = VAR_CHOICE, .choices = cutbeats_names,
        .section = SECTION_FADER, .label = "Cut beats",
        .help = "SC1000 cut mode only. Off: both ends of the fader cut "
                "the sample. Left end or Right end: that end cuts the "
                "beats instead, so each end cuts one deck. Left is the "
                "left of the fader bar on the home screen.",
    },
    {
        .name = "faderopenpoint", .valuePtr = &cfg_fader_open,
        .minValue = 1, .maxValue = 200, .stepSize = 1,
        .kind = VAR_NUMBER, .decimals = 0,
        .section = SECTION_FADER, .label = "Open point",
        .help = "SC1000 cut mode. How far the fader must move away from "
                "an end (out of 1023) before the sound comes in. Smaller "
                "means a shorter cut-in. Keep it above Close point. "
                "SC500 Innofader: 5.",
    },
    {
        .name = "faderclosepoint", .valuePtr = &cfg_fader_close,
        .minValue = 0, .maxValue = 199, .stepSize = 1,
        .kind = VAR_NUMBER, .decimals = 0,
        .section = SECTION_FADER, .label = "Close point",
        .help = "SC1000 cut mode. The sound cuts again when the fader "
                "goes back closer to the end than this. The small gap "
                "between open and close stops crackle when the fader "
                "sits right on the edge. SC500 Innofader: 3.",
    },
    {
        .name = "Fad Switch", .valuePtr = &cfg_fader_switch,
        .minValue = 0, .maxValue = 1, .stepSize = 1,
        .kind = VAR_CHOICE, .choices = fader_dir_names,
        .section = SECTION_FADER, .label = "Fader direction",
        .help = "Swaps the two ends of the fader. Change it if moving "
                "the fader left behaves like moving it right.",
    },
    {
        .name = "Fad Factor", .valuePtr = &cfg_curve_factor,
        .minValue = 0.01f, .maxValue = 10, .stepSize = 0.01f,
        .kind = VAR_NUMBER, .decimals = 2,
        .section = SECTION_FADER, .label = "Curve width",
        .help = "Curve mode. How far the fader travels before the sound "
                "is at full volume. Smaller is a sharper cut.",
    },
    {
        .name = "Fad Power", .valuePtr = &cfg_curve_power,
        .minValue = 0.01f, .maxValue = 10, .stepSize = 0.01f,
        .kind = VAR_NUMBER, .decimals = 2,
        .section = SECTION_FADER, .label = "Curve shape",
        .help = "Curve mode. Below 1 the sound jumps in fast, 1 is a "
                "straight line, above 1 it fades in slowly.",
    },
    {
        .name = "fader_smooth", .valuePtr = &cfg_fader_smooth,
        .minValue = 0, .maxValue = 50, .stepSize = 1,
        .kind = VAR_NUMBER, .decimals = 0, .unit = "ms", .zero_text = "Off",
        .section = SECTION_FADER, .label = "Fader smoothing",
        .help = "Fades the volume over this many milliseconds instead "
                "of jumping, to soften clicks on cuts. Off is instant, "
                "like the SC1000. For scratching keep it Off or at "
                "1 to 3 ms; more makes cuts sound slow.",
    },
    {
        .name = "soft_clip", .valuePtr = &cfg_soft_clip,
        .minValue = 0, .maxValue = 1, .stepSize = 1,
        .kind = VAR_ONOFF,
        .section = SECTION_FADER, .label = "Soft clip",
        .help = "When a deck's volume is pushed past 100% (blue button "
                "in Settings mode), round off loud peaks smoothly "
                "instead of hard digital clipping. It also makes normal "
                "levels a little quieter. Off is the SC1000 sound.",
    },

    /* ── Screen ──────────────────────────────────────────────── */
    {
        .name = "screen_off", .valuePtr = &cfg_screen_off,
        .minValue = 0, .maxValue = 60, .stepSize = 1,
        .kind = VAR_NUMBER, .decimals = 0, .unit = "min", .zero_text = "Never",
        .section = SECTION_SCREEN, .label = "Screen off after",
        .help = "Turns the screen off after this many minutes without "
                "using the knob, buttons, platter or fader. Any of them "
                "wakes it again (the knob only wakes it, it does not "
                "move the menu). Never keeps it on. It stays on while "
                "recording.",
    },
};

void settings_register(void)
{
    /* Values from scsettings.txt become the defaults, so Reset Defaults
     * goes back to what the settings file says */
    cfg_platterspeed = (float)scsettings.platterspeed;
    cfg_slippiness = (float)scsettings.slippiness;
    cfg_brakespeed = (float)scsettings.brakespeed;
    cfg_jogreverse = scsettings.jogReverse ? 1.0f : 0.0f;
    cfg_cutbeats = (float)scsettings.cutbeats;
    cfg_fader_open = (float)scsettings.faderopenpoint;
    cfg_fader_close = (float)scsettings.faderclosepoint;

    for (unsigned i = 0; i < sizeof(settings) / sizeof(settings[0]); i++)
    {
        const SettingInfo *s = &settings[i];

        /* A default from the settings file that is outside the menu's
         * range would make the setting impossible to edit */
        if (*s->valuePtr < s->minValue)
            *s->valuePtr = s->minValue;
        if (*s->valuePtr > s->maxValue)
            *s->valuePtr = s->maxValue;

        register_setting(s);
    }
}
