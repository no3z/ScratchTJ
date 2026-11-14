/*
 * Runtime settings: every value the Config menu can change.
 *
 * Each one is a float registered with shared_variables, so it can be
 * edited in the menu, saved to ~/.scratchtj/config.cfg and stored in
 * presets. The realtime code reads these globals directly (a float read
 * is atomic), which is cheaper than looking them up by name.
 *
 * What each setting does, in plain English: docs/SETTINGS.md
 */

#ifndef SETTINGS_H
#define SETTINGS_H

/* Menu sections */
#define SECTION_PLATTER "Platter"
#define SECTION_FADER   "Fader & Volume"
#define SECTION_SCREEN  "Screen"

/* Fader modes */
#define FADER_MODE_SC1000 0
#define FADER_MODE_CURVE  1

/* Platter */
extern float cfg_platterspeed;     /* encoder counts per second of audio */
extern float cfg_jogreverse;       /* 1 = platter direction flipped */
extern float cfg_touch_threshold;  /* capacitive reading that counts as a touch */
extern float cfg_motion_touch;     /* 1 = a turning platter counts as touched */
extern float cfg_slippiness;       /* slipmat: higher = slower back to speed */
extern float cfg_brakespeed;       /* stop button: higher = longer stop */
extern float cfg_target_pitch;     /* position-tracking gain under the hand */
extern float cfg_pitch_filter;     /* pitch low-pass, 0..1 (1 = no smoothing) */
extern float cfg_vel_ff;           /* platter velocity feed-forward, 0..1 */
extern float cfg_blipthreshold;    /* encoder jump treated as a bad read */

/* Fader & Volume */
extern float cfg_fader_mode;       /* FADER_MODE_SC1000 or FADER_MODE_CURVE */
extern float cfg_cutbeats;         /* 0 off, 1 left end cuts beats, 2 right end */
extern float cfg_fader_open;       /* hysteresis: open above this (0..1023) */
extern float cfg_fader_close;      /* hysteresis: close below this (0..1023) */
extern float cfg_fader_switch;     /* 1 = normal direction, 0 = reversed */
extern float cfg_curve_factor;     /* curve mode: travel to full volume */
extern float cfg_curve_power;      /* curve mode: shape exponent */
extern float cfg_fader_smooth;     /* volume ramp in ms, 0 = instant */
extern float cfg_soft_clip;        /* 1 = tanh saturation instead of hard clip */

/* Screen */
extern float cfg_screen_off;       /* minutes idle before blanking, 0 = never */

/* Register every setting. Call after loadSettings(), so the values from
 * scsettings.txt become the defaults that Reset Defaults returns to. */
void settings_register(void);

#endif
