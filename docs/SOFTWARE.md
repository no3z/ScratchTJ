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

All shared state between threads is protected by mutexes in the `shared_variables` system.

---

## Module Reference

### sc_input.c -- Hardware Input Handler

The central input module. Runs in its own thread started by `SC_Input_Start()`.

**Responsibilities:**
- Opens `/dev/ttyUSB0` at 500,000 baud
- Parses 6-byte Arduino packets (sync, fader, cap touch, checksum)
- Reads MT6701 angle over I2C (`/dev/i2c-1`, address 0x06)
- Converts raw angle to deck position using `platterspeed` ratio
- Processes capacitive touch (touch/release detection with threshold)
- Reads cue button states via GPIO
- Calls into xwax deck API to update fader position, platter angle, touch state

**Key globals:**
- `capIsTouched` -- current touch state
- `ADCs[4]` -- raw analog readings
- `cue_display_states[4]` -- cue point states for display

### st7789.c -- TFT Display Driver

Low-level SPI framebuffer driver for the 240x240 ST7789 display.

**Architecture:**
- Maintains a 240x240 RGB565 framebuffer in memory
- All drawing operations write to the framebuffer
- `st7789_flush()` sends the entire framebuffer over SPI in one transfer
- Uses `/dev/spidev0.0` with direct GPIO for DC and RST pins

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
- `oled_draw_value_screen()` -- parameter editing display (name, value, range)
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

### main_menu.c -- Home Screen & Top Menu

**Home screen displays:**
- Current track name and artist
- Deck play/pause state with colored indicators
- Elapsed time and remaining time
- Fader position visualization
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
- **Global Settings** -- all registered shared variables with real-time editing
- **Save Config** -- persist current settings to file
- **Reset Defaults** -- restore factory defaults

### preset_menu.c -- Preset System

3-slot save/load system for entire configurations.

- Each slot saves all shared variable values to `preset_N.cfg`
- Last-used preset number is stored and auto-loaded on startup
- Menu shows slot status (empty/saved) and allows save/load operations

### shared_variables.c -- Configuration Registry

Thread-safe registry of runtime-adjustable parameters.

**Features:**
- Variables registered with name, pointer, min/max, step size, default
- Mutex-protected read/write access
- Serialize to/from file (key=value format)
- Bulk reset to defaults
- Used by the menu system for real-time parameter editing

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
                    [6-byte packet]
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

See the comments in `scsettings.txt` for full documentation of all parameters.
