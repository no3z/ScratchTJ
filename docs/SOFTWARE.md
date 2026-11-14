# ScratchTJ MK2 -- Software Architecture

## Overview

ScratchTJ is built on top of [xwax](http://www.xwax.co.uk/), an open-source digital vinyl emulation system. The ScratchTJ software adds hardware input handling, a TFT display system, a menu UI, and a configuration framework to make xwax work as a standalone DJ instrument without a monitor or keyboard.

---

## Thread Model

The software runs multiple threads for real-time performance:

```
Main thread (xwax)
  |-- Audio callback (ALSA, real-time priority)
  |-- Deck threads (track decoding, file I/O)
  |
  +-- SC_Input thread (sc_input.c)
  |     Reads serial from Arduino (fader, cap touch)
  |     Reads MT6701 encoder via I2C
  |     Processes cue button GPIO
  |     Updates deck state
  |
  +-- Rotary encoder thread (lcd_menu.c)
        Polls EC11 encoder via GPIO
        Drives menu state machine
        Renders TFT display
```

Settings are plain `float` globals (`cfg_*`, see `settings.h`). The audio and input threads read them directly, since a float read is atomic. Only the menu thread writes them, through the `shared_variables` registry.

---

## Module Reference

### sc_input.c -- Hardware Input Handler

The central input module. Runs in its own thread started by `SC_Input_Start()`.

**Responsibilities:**
- Opens `/dev/ttyUSB0` at 500,000 baud
- Parses 7-byte Arduino packets (sync, fader, cap touch, cue buttons, checksum)
- Reads MT6701 angle over I2C (`/dev/i2c-1`, address 0x06)
- Converts raw angle to deck position using `platterspeed` ratio
- Processes capacitive touch: touched when the reading is above `cfg_touch_threshold`, or, with `cfg_motion_touch` on, while the platter turns
- Fader, in two modes chosen by `cfg_fader_mode`:
  - `process_fader_sc1000()` is the SC1000 hard cut: hysteresis on each end (`cfg_fader_open` / `cfg_fader_close`), with `cfg_cutbeats` choosing which end, if any, cuts the beats deck;
  - `process_fader_curve()` is the ScratchTJ crossfade curve (`cfg_curve_factor` / `cfg_curve_power`).
- Reads cue button states via GPIO
- Calls into xwax deck API to update fader position, platter angle, touch state
- Stamps `input_activity_ms` whenever the user plays (touch, fader, buttons, platter), for the screen saver

**Key globals:**
- `capIsTouched` -- current touch state
- `cap_raw` -- last raw touch-sensor reading (shown live in the Touch threshold editor)
- `input_activity_ms` -- `CLOCK_MONOTONIC` ms of the last player activity
- `ADCs[4]` -- raw analog readings
- `cue_display_states[4]` -- cue point states for display

### st7789.c -- TFT Display Driver

Low-level SPI framebuffer driver for the 240x240 ST7789 display.

**Architecture:**
- Maintains a 240x240 RGB565 framebuffer in memory
- All drawing operations write to the framebuffer
- `st7789_flush()` sends the entire framebuffer over SPI in one transfer
- Uses `/dev/spidev0.0` with direct GPIO for DC and RST pins
- `st7789_sleep(1)` sends DISPOFF + SLPIN, and `st7789_sleep(0)` sends SLPOUT + DISPON. The screen saver uses these. The backlight pin (BLK) is wired to 3.3 V, so the backlight itself stays lit.

**Drawing primitives:**
- `st7789_pixel()`, `st7789_hline()`, `st7789_vline()`
- `st7789_fill_rect()`
- `st7789_draw_char()` / `st7789_draw_string()` -- built-in 8x8 bitmap font with scaling
- Three font sizes: `FONT_SMALL` (8px), `FONT_MEDIUM` (16px), `FONT_LARGE` (24px)

### oled_display.c -- High-Level Display Layer

Provides themed, menu-aware drawing functions on top of the ST7789 driver. Despite the filename (historical from the planned SSD1306 OLED), this drives the ST7789 TFT.

**Color theme system:**
- Title bar: dark blue background, white text
- Selected item: bright blue highlight
- Values: green text
- Cue indicators: per-cue colors (red, green, yellow, blue)
- Deck accents: cyan for Deck 1, orange for Deck 2

**High-level drawing functions:**
- `oled_draw_title_bar()` -- renders a colored title bar
- `oled_draw_menu_list()` -- scrollable list with selection highlight and scrollbar
- `oled_draw_value_list()` -- scrollable list with a right-aligned value per row (Config sections)
- `oled_draw_value_screen()` -- parameter editing display (name, value, range)
- `oled_draw_fader_curve()` -- both decks' Curve-mode volume curves
- `oled_set_blank()` / `oled_is_blank()` -- screen saver. While blank, `oled_flush()` skips the SPI transfer, and waking re-sends the framebuffer.
- `oled_draw_cue_bar()` -- colored cue point indicators with position
- `oled_draw_progress_bar()`, `oled_draw_center_fader()` -- visualization widgets
- `oled_draw_confirm()` -- yes/no confirmation dialog

### gpio_direct.c -- Direct GPIO Access

Replaces wiringPi with direct BCM2835/BCM2836 register access via `/dev/mem`. No external library dependencies.

**Functions:**
- `gpio_direct_init()` -- maps BCM GPIO registers
- `gpio_read(pin)` / `gpio_write(pin, value)`
- `gpio_set_input(pin)` / `gpio_set_output(pin)`
- `gpio_set_pullup(pin)` -- enables internal pull-up resistor
- `gpio_millis()` -- millisecond timer

Requires root privileges for `/dev/mem` access.

### lcd_menu.c -- Menu State Machine

The menu controller. Runs in a dedicated thread polling the EC11 rotary encoder.

**State machine:**
```
MENU_HOME <---> MENU_MAIN
                  |
                  +-- MENU_DECK1 --> deck sub-menus
                  +-- MENU_DECK2 --> deck sub-menus
                  +-- MENU_CONTROLLER --> controller sub-menus
                  +-- MENU_INFO
                  +-- MENU_RECORD
```

**Encoder handling:**
- Polls CLK/DT pins at ~1kHz with debouncing
- Detects rotation direction and button press/release
- KB0 button supports short press (back) and long press (home)

**Screen saver:** `screen_saver_poll()` runs at the start of `rotary_encoder_moved()`, which every menu loop calls.
- It blanks the screen after `cfg_screen_off` minutes with no knob, KB0 or `input_activity_ms` activity.
- It never blanks while a deck is recording.
- Any activity wakes the screen. A knob turn or press that wakes the screen is swallowed, so it does not also move the menu.

### main_menu.c -- Home Screen & Top Menu

**Home screen displays:**
- Current track name and artist
- Deck play/pause state with colored indicators
- Elapsed time and remaining time
- Fader position visualization
- Waveform strip (`draw_waveform()`): about 2 s of the active deck centred on the playhead, right of the platter. Built from the per-64-sample peak meter xwax fills while importing (`track_get_ppm()`), so a frame costs about 1500 byte reads. The draw holds `player_track_read_lock()` so a track swap on another thread can't free the track mid-read.
- Cue point bar (4 slots with colors)
- Touch state indicator

**Main menu items:**
- Deck 1, Deck 2, Controller, Info, Presets

### deck_menu.c -- Per-Deck Controls

Provides file browsing, playback control, volume adjustment, and recording for each deck.

**Sub-menus:**
- **Load File** -- navigate folders, select tracks, random file
- **Settings** -- jog pitch, jog reverse
- **Volume** -- real-time volume adjustment with visual feedback
- **Info** -- track metadata display
- **Record** -- select input source and record to WAV

### controller_menu.c -- System Configuration

**Sub-menus:**
- **Sound Settings** -- ALSA mixer parameters (volume, gain, etc.)
- **Platter / Fader & Volume / Screen** -- `enter_settings_section()` lists the settings whose `section` matches, each with its formatted value. The editor screen shows:
  - the value;
  - a live readout (record rpm, touch sensor, fader position);
  - the default and range;
  - the wrapped `help` text;
  - for Curve settings, the curve graph.

  Values change live. Turning fast takes bigger steps. KB0 keeps the value, and knob push restores the old one. The section saves to `config.cfg` on exit if anything changed.
- **Save Config** -- persist current settings to file
- **Reset Defaults** -- after confirmation, reset every setting to its default and save
- **Restart (rescan files)** -- restart xwax in place so it re-reads the beats and samples folders (same as `sudo kill -USR1 $(pgrep xwax)`)
- **Reboot Pi** -- reboot the whole device

### preset_menu.c -- Preset System

3-slot save/load system for entire configurations.

- Each slot saves all shared variable values to `preset_N.cfg`
- Last-used preset number is stored and auto-loaded on startup
- Menu shows slot status (empty/saved) and allows save/load operations

### settings.c -- Settings Table

One table (`SettingInfo[]`) describes every setting in the Config menu. Each entry has:
- the config key and pointer to its `cfg_*` global;
- the on-screen label and menu section;
- its kind: number, On/Off, or choice list;
- min, max and step;
- its unit, decimals and zero text (for example "Off" or "Never");
- a plain-English help text.

`settings_register()` runs after `loadSettings()`:
1. It seeds values from `scsettings.txt` (platterspeed, slippiness, brakespeed, jogreverse, faderopen/closepoint, cutbeats).
2. It clamps each value into its range.
3. It registers each entry, so the `scsettings.txt` values become the defaults that Reset Defaults returns to.

To add a setting:
1. Declare an `extern float cfg_x` in `settings.h`.
2. Define it and add a table entry in `settings.c`.
3. Read `cfg_x` where it is used.
4. Document it in `docs/SETTINGS.md`.

### shared_variables.c -- Configuration Registry

The registry of runtime-adjustable parameters.

**Features:**
- `register_setting()` takes the full metadata (label, section, help, kind, unit, choices). `register_variable()` remains for plain numbers.
- `format_variable_value()` formats a value for display: On/Off, choice text, zero text, or a number with its unit.
- Serializes to and from `~/.scratchtj/config.cfg` in key=value format, writing a `# Section > Label` comment above each key. On load, unknown keys are reported and skipped, and values are clamped into range.
- Bulk reset to defaults
- `find_variable()` / `get_variable_value()` look up by name without locking. Realtime code should read the `cfg_*` globals instead.

---

## Build System

The Makefile builds a single `xwax` binary:

```bash
make -j4        # Build main binary
make test_tft_ui  # Build standalone TFT test
make clean      # Remove build artifacts
```

**Dependencies:**
- GCC
- SDL2 (`libsdl2-dev`)
- ALSA (`libasound2-dev`)
- pthreads
- Math library (`-lm`)

No wiringPi, no Python, no Node.js. The entire system is pure C with standard Linux APIs.

---

## Data Flow

```
                    Arduino Nano
                   /dev/ttyUSB0
                   500,000 baud
                        |
                    [7-byte packet]
                    fader + cap touch
                        |
                        v
    MT6701 ----I2C----> sc_input.c -----> deck API (xwax)
    (angle)             |                      |
                        |                      v
                   cue buttons           Audio Engine
                   (GPIO)               (ALSA callback)
                        |                      |
                        v                      v
                   lcd_menu.c            Speaker/Headphones
                        |
                        v
                   oled_display.c
                        |
                        v
                   st7789.c (SPI)
                        |
                        v
                   TFT Display
```

---

## Configuration File Format

`scsettings.txt` uses a simple key=value format:

```
# Comments start with #
buffersize=1024
platterspeed=9100
brakespeed=3000
```

GPIO mappings use a special format:
```
gpio=port,pin,pull,edge,action
```

MIDI mappings:
```
midii=status,channel,data,edge,action
```

See the comments in `scsettings.txt`, and [SETTINGS.md](SETTINGS.md) for every key.

Values saved from the Config menu go to `~/.scratchtj/config.cfg`, in the same key=value format. They override `scsettings.txt`. Presets (`preset_N.cfg`) store the same keys.
