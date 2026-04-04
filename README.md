# ScratchTJ MK2 -- DIY Digital Scratch Turntable

A fully open-source portable scratch instrument built around a Raspberry Pi, Arduino Nano, and the [xwax](http://www.xwax.co.uk/) digital vinyl emulation engine.

This project is based on the work by **[the_rasteri](https://github.com/rasteri)** and the [SC1000](https://github.com/rasteri/SC1000) open-source portable turntable. The software is a fork of the SC1000 codebase, and the hardware design draws heavily from that project. Without rasteri's original work, ScratchTJ would not exist.

> **Project status:** this repository tracks **MK2**.
> The previous generation is preserved in Git tag **`mk1`**.

**[Full Build Guide](docs/BUILD_GUIDE.md)** -- step-by-step wiring, assembly, and component documentation.

![MK2 top view -- platter, TFT display, buttons, and fader](docs/images/image6.jpeg)

▶ *Click to play video:*

[![Scratch demo](https://img.youtube.com/vi/LH9r2fsUk-c/hqdefault.jpg)](https://youtu.be/LH9r2fsUk-c?si=Gcfz0AI6pA22WaxF)

## Features

- Two independent decks: beats + scratch sample, mixed through a DJ fader
- MT6701 magnetic angle sensor for high-precision platter tracking (MK2 upgrade from 600 PPR optical)
- ST7789 240x240 TFT display with rotary encoder menu navigation
- Touch-sensitive HDD platter with capacitive sensing
- Four cue buttons via Arduino (A0--A3) with short-press jump and long-press set
- Two function modes for cue buttons: **CUE** (jump/set cue points) and **SETTINGS** (platter-based parameter control)
- SETTINGS mode: hold button + rotate platter to adjust pitch, platterspeed, volume (gain), or toggle start/stop
- Active deck switching via rotary long press -- platter, cue buttons, and display follow selected deck
- Live input recording from AudioInjector capture
- Save/load configuration via Config menu
- Binary serial protocol at 500 kbaud between Arduino and Pi
- All key parameters tunable in real time from the menu or SETTINGS function buttons

![MK2 alternate angle -- TFT and platter closeup](docs/images/image8.jpeg)

▶ *Click to play video:*

[![Randomize function scratch](https://img.youtube.com/vi/6zyM5B6j0_E/hqdefault.jpg)](https://youtu.be/6zyM5B6j0_E?si=wSV1pAbMLMZF-k79)

## Architecture

```
+--------------+   Serial 500kbaud   +--------------+   I2S Audio   +-----------------+
| Arduino Nano | ------------------- | Raspberry Pi | ------------ | AudioInjector   |
|              |   7-byte binary pkt |              |              | Sound Card      |
| - Fader      |   (sync+data+XOR)   | - xwax       |              +-----------------+
| - Cap sensor |                     | - TFT menu   |
| - 4 cue btns |                     |              |
+--------------+                     | - MT6701 I2C |
                                     | - Recording  |
                                     +--------------+
```

The Arduino handles the DJ crossfader, capacitive touch sensor, and 4 cue buttons, sending data as 7-byte binary packets with XOR checksum at ~500 Hz. The Raspberry Pi reads the MT6701 magnetic encoder via I2C, runs the audio engine, TFT menu system, and live recording.

![MK2 internals -- wiring, platter mount, and AudioInjector sound card](docs/images/image1.jpeg)

## Bill of Materials

- **Raspberry Pi** (2/3/4)
- **AudioInjector Sound Card** (I2S audio HAT)
- **Arduino Nano** (serial at 500 kbaud for fader + cap touch)
- **MT6701 Magnetic Angle Sensor** (I2C, platter position)
- **ST7789 240x240 TFT Display** (SPI, menu UI)
- **Rotary Encoder with Push Button** (menu navigation)
- **60mm DJ Crossfader**
- **HDD Platter** (capacitive touch surface)
- **Cue Buttons** (4x, wired to Arduino A0--A3)
- **Diametric Magnet** (for MT6701 sensing)
- **3D Printed MK2 Enclosure**

![MT6701 magnetic encoder module](docs/images/mt6701_magnetic_encoder.png)
![ST7789 TFT display module](docs/images/st7789_tft_display.png)

## Software Overview

Key source files:

| File | Purpose |
|------|---------|
| `arduino_nano/arduino_nano.ino` | Arduino firmware: fader, cap sensor, cue buttons, binary serial |
| `software/sc_input.c` | Serial reader, MT6701 I2C encoder, platter-to-audio position mapping |
| `software/player.c` | Audio engine with cubic interpolation, pitch filtering, slipmat simulation |
| `software/xwax.c` | Main application, deck init, shared variable registration |
| `software/recording.c` | Live input recording from ALSA capture devices |
| `software/cues.c` | Cue point system with per-track save/load |
| `software/lcd_menu.c` | TFT display, rotary encoder menu navigation |
| `software/deck_menu.c` | Per-deck menu: file browse, transport, cue screen, recording |
| `software/controller_menu.c` | Config menu: Sound Settings, Global Settings, Info |
| `software/preset_menu.c` | 3-slot preset save/load (compiled, not yet wired into menu) |
| `software/shared_variables.c` | Thread-safe runtime variable system for menu-tunable parameters |

## Menu System

The menu is driven by the EC11 rotary encoder (scroll + push) and the KB0 button (GPIO 17). The home screen shows a live platter visualization for the active deck with info column, fader graph, and cue indicators. A function mode info strip shows the current button assignments.

▶ *Click to play video:*

[![Menu and buttons demo](https://img.youtube.com/vi/j2CSrozANF8/hqdefault.jpg)](https://youtube.com/shorts/j2CSrozANF8?si=jhMayBKkclHSwKNa)

### Full Menu Tree

```
HOME SCREEN
│  Active deck platter visualization (fader arc, ghost/physical needles,
│  touch glow, elapsed time), speed/pitch/motor/slip info column,
│  fader history graph, progress bar, other deck compact strip,
│  function mode info strip, cue flash banner, cue bar overlay
│  Title bar shows [S] indicator when in SETTINGS mode
│  Long press rotary = switch active deck (DECK 1 ↔ DECK 2)
│  KB0 or encoder turn → Main Menu
│
├── Dk2 [▶/■] filename 0:00  .. Deck 2
│   ├── Load File
│   │   ├── Start/Stop
│   │   ├── Next File
│   │   ├── Previous File
│   │   ├── Random File
│   │   ├── Next Folder
│   │   ├── Previous Folder
│   │   └── Record ............ enter recording workflow
│   │   (KB0 enters folder/file browser for the selected action)
│   ├── Start/Stop ............. toggle playback
│   ├── Settings
│   │   ├── Jog Pitch Mode ..... toggle pitch bend via platter
│   │   ├── Toggle Jog Reverse . reverse platter direction
│   │   └── Platter Speed ...... adjust encoder-to-audio ratio
│   └── Info ................... filename, path, position, pitch,
│                                volume, touch state, motor speed, cue points
│
├── Dk1 [▶/■] filename 0:00  .. Deck 1 (same structure as Deck 2)
│
├── Randomize .................. load random tracks on both decks
│
├── Record Dk2 ................. recording workflow
│   ├── Record Setup
│   │   ├── Source .............. AudioInjector hw:1,0 (KB0 cycles)
│   │   └── RECORD ............. KB0 starts capture
│   └── Recording .............. live MM:SS timer, blinking red dot
│                                KB0 = stop (auto-loads into deck)
│
├── Btn: CUE/SETTINGS ......... toggle function mode for cue buttons
│
├── Config
│   ├── Sound Settings ......... all ALSA mixer controls
│   │   └── (per control) ...... volume slider, boolean, or enum
│   ├── Global Settings ........ all registered shared variables
│   │   └── (per variable) ..... value adjust with min/max/step
│   │       (fader variables show live curve graph)
│   ├── Save Config ............ writes to ~/.scratchtj/config.cfg
│   └── Reset Defaults ......... restores all to factory values
│
└── Info
    ├── CPU/Mem ................ live CPU and memory usage
    └── Version ................ "ScratchTJ v2" + build date
```

### Navigation Controls

| Control | Action |
|---------|--------|
| Encoder rotate | Scroll through menu items / adjust values |
| KB0 button (GPIO 17) | Select / confirm / enter submenu |
| Encoder push (GPIO 27) short press | Back / cancel / return to previous menu |
| Encoder push long press (500ms) | Switch active deck (DECK 1 ↔ DECK 2) |

### Function Modes (CUE / SETTINGS)

Toggle via main menu item "Btn: CUE/SETTINGS". Default is SETTINGS on startup.

**CUE mode** -- cue buttons work as cue points:

| Button | Action |
|--------|--------|
| Short press | Jump to stored cue point |
| Long press (1s) | Set cue point at current position |

**SETTINGS mode** -- cue buttons become parameter controls:

| Button | Color | Function | Control |
|--------|-------|----------|---------|
| Btn 1 | Red | Pitch (note_pitch) | Hold + rotate platter (0.25x -- 4.0x) |
| Btn 2 | Green | Platter Speed | Hold + rotate platter |
| Btn 3 | Yellow | Start/Stop | Press to toggle |
| Btn 4 | Blue | Volume (gain) | Hold + rotate platter (up to x3.60) |

When holding a SETTINGS button and rotating the platter, a live overlay shows the parameter name, value, and gauge bar. Audio position is frozen during adjustment. Config auto-saves when buttons are released.

### Runtime-Tunable Parameters

All adjustable live from Config > Global Settings:

| Parameter | Default | Range | Step | Description |
|-----------|---------|-------|------|-------------|
| Fad Factor | 0.1 | 0.01 - 10 | 0.01 | Fader curve transition point |
| Fad Power | 0.2 | 0.01 - 10 | 0.01 | Fader curve exponent |
| Fad Switch | 1.0 | 0 - 1 | 1.0 | Fader direction (0=normal, 1=reversed) |
| slippiness | 200 | 1 - 3000 | 25 | Slipmat simulation feel |
| target_pitch | 40 | 1 - 240 | 1 | Position tracking gain |
| brakespeed | 3000 | 1 - 10000 | 500 | Stop button deceleration rate |
| platterspeed | 9100 | 1 - 32768 | 256 | Encoder-to-audio position ratio |
| blipthreshold | 400 | 50 - 2048 | 50 | Encoder glitch rejection threshold |
| pitch_filter | 0.1 | 0.01 - 1.0 | 0.01 | Pitch low-pass filter (0=smooth, 1=instant) |
| fader_sharp | 1.0 | 0 - 1.0 | 0.05 | Fader sharpness |
| fhist_secs | 2.0 | 0.1 - 120 | 0.1 | Fader history graph time window (seconds) |

### Serial Protocol

```
Byte:  [0]     [1]     [2]     [3]     [4]     [5]       [6]
       SYNC    fHi     fLo     capHi   capLo   buttons   XOR
       0xAA    ──fader──       ──cap touch──    4-bit     checksum
               (10-bit)        (16-bit)         bitfield  (bytes 1-5)
```

In MK2, the Arduino no longer sends encoder angle data -- the Pi reads the MT6701 directly over I2C. The packet is 7 bytes: sync, fader (2), cap touch (2), cue buttons (1), checksum (1).

Handshake: Pi sends `0x53` ('S'), Arduino replies `'T'`. Watchdog detects 2-second timeouts with DTR hard-reset after 3 consecutive failures.

![Arduino Nano in its 3D printed cradle -- the 1.2kΩ resistor and the wire going to the platter spring contact are both soldered to the same D12 sense pin](docs/images/image4.jpeg)

## Building

### Prerequisites (on the Raspberry Pi)

- GCC, make
- SDL2 development libraries
- ALSA development libraries
- wiringPi

### Compile and Run

```bash
cd ~/ScratchTJ/software
make -j4
sudo LC_ALL=en_GB.utf8 nice -n -19 ./xwax
```

If locale is not set as above, xwax may fail to start correctly.

### Flash the Arduino

Open `arduino_nano/arduino_nano.ino` in the Arduino IDE. Install the `CapacitiveSensor` library, select Arduino Nano, and upload. Both the Arduino firmware and the Pi software must be updated together since they share the binary serial protocol.

Audio samples go in `~/samples/` (scratch deck) and `~/beats/` (beat deck). The paths are configured in `xwax.c`.

▶ *Click to play video:*

[![Little scratch demo](https://img.youtube.com/vi/L9gylG18938/hqdefault.jpg)](https://youtube.com/shorts/L9gylG18938?si=5t2caiyyweBquCjl)

## Hardware Test Scripts

Hardware test and diagnostic scripts are in the [`hw_test/`](hw_test/) directory.

## Documentation

- **[Build Guide](docs/BUILD_GUIDE.md)** -- Step-by-step wiring and assembly
- **[Installation](docs/INSTALLATION.md)** -- Software installation details
- **[Software Architecture](docs/SOFTWARE.md)** -- Thread model, subsystems
- **[Enclosure Design](docs/enclosure/ENCLOSURE_DESIGN.md)** -- 3D printable parts
- **[Parametric Encoder Mount](docs/enclosure/parametric_encoder_mount.scad)** -- OpenSCAD source for the MT6701 encoder mount (fully parametric)

![Encoder mount exploded view in OpenSCAD](docs/images/encoder_mount_openscad.png)

## Project History

ScratchTJ started as an adaptation of rasteri's SC1000 to run on a Raspberry Pi with an AudioInjector audio hat, replacing the Olimex A13 and custom PCB with off-the-shelf components (Arduino Nano, standard encoder, DJ fader) and serial communication instead of I2C/SPI.

MK2 replaces the 600 PPR optical encoder with an MT6701 magnetic angle sensor, the 1602A LCD with an ST7789 TFT display, and introduces a redesigned enclosure with better platter support and Hall-sensor alignment.

## License and Attributions

Licensed under GNU GPL v2.

- **[xwax](http://www.xwax.co.uk/)** by Mark Hills -- the digital vinyl emulation engine at the core of this project
- **[SC1000](https://github.com/rasteri/SC1000)** by the_rasteri -- the open-source portable turntable that ScratchTJ is forked from
- **[AudioInjector](http://www.audioinjector.net/)** -- I2S sound card HAT for Raspberry Pi

---

Kiitos paljon ja happy scratching!
