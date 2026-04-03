# ScratchTJ MK2 -- DIY Digital Scratch Turntable

A fully open-source portable scratch instrument built around a Raspberry Pi, Arduino Nano, and the [xwax](http://www.xwax.co.uk/) digital vinyl emulation engine.

This project is based on the work by **[the_rasteri](https://github.com/rasteri)** and the [SC1000](https://github.com/rasteri/SC1000) open-source portable turntable. The software is a fork of the SC1000 codebase, and the hardware design draws heavily from that project. Without rasteri's original work, ScratchTJ would not exist.

> **Project status:** this repository tracks **MK2**.
> The previous generation is preserved in Git tag **`mk1`**.

![MK2 top view -- platter, TFT display, buttons, and fader](docs/images/image6.jpeg)

## Demo Videos

[![Randomize function scratch](https://img.youtube.com/vi/6zyM5B6j0_E/hqdefault.jpg)](https://youtu.be/6zyM5B6j0_E?si=wSV1pAbMLMZF-k79)
[![Scratch demo](https://img.youtube.com/vi/LH9r2fsUk-c/hqdefault.jpg)](https://youtu.be/LH9r2fsUk-c?si=Gcfz0AI6pA22WaxF)

[![Menu demo](https://img.youtube.com/vi/j2CSrozANF8/hqdefault.jpg)](https://youtube.com/shorts/j2CSrozANF8?si=jhMayBKkclHSwKNa)
[![Little scratch demo](https://img.youtube.com/vi/L9gylG18938/hqdefault.jpg)](https://youtube.com/shorts/L9gylG18938?si=5t2caiyyweBquCjl)

## Features

- Two independent decks: beats + scratch sample, mixed through a DJ fader
- MT6701 magnetic angle sensor for high-precision platter tracking (MK2 upgrade from 600 PPR optical)
- ST7789 240x240 TFT display with rotary encoder menu navigation
- Touch-sensitive HDD platter with capacitive sensing
- Two dedicated cue buttons (GPIO 17 / 27) with short-press jump and long-press set
- Live input recording from ALSA capture devices
- 5-slot preset system to save and load all settings
- Binary serial protocol at 500 kbaud between Arduino and Pi
- All key parameters tunable in real time from the menu
- Pitch mode via rotary encoder long press

![MK2 alternate angle -- TFT and platter closeup](docs/images/image8.jpeg)

## Architecture

```
+--------------+   Serial 500kbaud   +--------------+   I2S Audio   +-----------------+
| Arduino Nano | ------------------- | Raspberry Pi | ------------ | AudioInjector   |
|              |   8-byte binary pkt |              |              | Sound Card      |
| - Fader      |   (sync+data+XOR)   | - xwax       |              +-----------------+
| - Cap sensor |                     | - TFT menu   |
+--------------+                     | - MT6701 I2C |
                                     | - Recording  |
                                     +--------------+
```

The Arduino handles the DJ crossfader and capacitive touch sensor, sending data as 8-byte binary packets with XOR checksum at 200 Hz. The Raspberry Pi reads the MT6701 magnetic encoder via I2C, runs the audio engine, TFT menu system, and live recording.

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
- **Cue Buttons** (2x, on GPIO 17 and 27)
- **Diametric Magnet** (for MT6701 sensing)
- **3D Printed MK2 Enclosure**

![MT6701 magnetic encoder module](docs/images/mt6701_magnetic_encoder.png)
![ST7789 TFT display module](docs/images/st7789_tft_display.png)

## Software Overview

Key source files:

| File | Purpose |
|------|---------|
| `arduino_nano/arduino_nano.ino` | Arduino firmware: fader, cap sensor, binary serial protocol |
| `software/sc_input.c` | Serial reader, MT6701 I2C encoder, platter-to-audio position mapping |
| `software/player.c` | Audio engine with cubic interpolation, pitch filtering, slipmat simulation |
| `software/xwax.c` | Main application, deck init, shared variable registration |
| `software/recording.c` | Live input recording from ALSA capture devices |
| `software/cues.c` | Cue point system with per-track save/load |
| `software/lcd_menu.c` | TFT display, rotary encoder menu navigation, 2-button support |
| `software/deck_menu.c` | Per-deck menu: file browse, transport, cue screen, recording |
| `software/controller_menu.c` | Config menu: Sound Settings, Global Settings, Info, Presets |
| `software/preset_menu.c` | 5-slot preset save/load/reset system |
| `software/shared_variables.c` | Thread-safe runtime variable system for menu-tunable parameters |

### Menu Structure

```
Main Menu
|-- Deck 0 (Beats) -- file browse, transport, volume, cue points, recording
|-- Deck 1 (Samples) -- file browse, transport, volume, cue points, recording
+-- Config
    |-- Sound Settings -- ALSA mixer controls
    |-- Global Settings -- all runtime-tunable parameters
    |-- Info -- system information
    +-- Presets -- save/load/reset across 5 slots
```

Navigation: rotary encoder scrolls, **Enter** button (GPIO 17) selects, **Back** button (GPIO 27) returns. Long-pressing the rotary encoder enters **Pitch Mode**.

### Runtime-Tunable Parameters

All adjustable live from Config > Global Settings:

| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| Fad Factor | 0.1 | 0.1 - 10 | Fader curve transition point |
| Fad Power | 0.2 | 0.1 - 10 | Fader curve exponent |
| Fad Switch | 1.0 | 0 - 1 | Fader direction (0=normal, 1=reversed) |
| slippiness | 200 | 1 - 3000 | Slipmat simulation feel |
| target_pitch | 15 | 1 - 240 | Position tracking gain |
| brakespeed | 3000 | 1 - 10000 | Stop button deceleration rate |
| platterspeed | 1333 | 1 - 8192 | Encoder-to-audio position ratio |
| blipthreshold | 59 | 50 - 2048 | Encoder glitch rejection threshold |
| pitch_filter | 0.2 | 0.01 - 1.0 | Pitch low-pass filter (0=smooth, 1=instant) |
| cap_threshold | 5000 | 500 - 30000 | Capacitive touch activation level |
| cap_hysteresis | 500 | 0 - 5000 | Touch on/off hysteresis band |

### Serial Protocol

```
Byte:  [0]     [1]     [2]     [3]     [4]     [5]       [6]
       SYNC    fHi     fLo     capHi   capLo   buttons   XOR
       0xAA    ──fader──       ──cap touch──    4-bit     checksum
               (10-bit)        (16-bit)         bitfield  (bytes 1-5)
```

In MK2, the Arduino no longer sends encoder angle data -- the Pi reads the MT6701 directly over I2C. The packet is 7 bytes: sync, fader (2), cap touch (2), cue buttons (1), checksum (1).

Handshake: Pi sends `0x53` ('S'), Arduino replies `'T'`. Watchdog detects 2-second timeouts with DTR hard-reset after 3 consecutive failures.

![Arduino Nano in its 3D printed cradle with USB serial connection](docs/images/image4.jpeg)

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

## Hardware Test Scripts

Hardware test and diagnostic scripts are in the [`hw_test/`](hw_test/) directory.

## Documentation

- **[Build Guide](docs/BUILD_GUIDE.md)** -- Step-by-step wiring and assembly
- **[Installation](docs/INSTALLATION.md)** -- Software installation details
- **[Software Architecture](docs/SOFTWARE.md)** -- Thread model, subsystems
- **[Enclosure Design](docs/enclosure/ENCLOSURE_DESIGN.md)** -- 3D printable parts
- **[Parametric Encoder Mount](docs/enclosure/parametric_encoder_mount.scad)** -- OpenSCAD source for the MT6701 encoder mount (fully parametric)

## Project History

ScratchTJ started as an adaptation of rasteri's SC1000 to run on a Raspberry Pi with an AudioInjector audio hat, replacing the Olimex A13 and custom PCB with off-the-shelf components (Arduino Nano, standard encoder, DJ fader) and serial communication instead of I2C/SPI.

MK2 replaces the 600 PPR optical encoder with an MT6701 magnetic angle sensor, the 1602A LCD with an ST7789 TFT display, and introduces a redesigned enclosure with better platter support and Hall-sensor alignment.

## License

Based on [xwax](http://www.xwax.co.uk/) by Mark Hills, licensed under GNU GPL v2.

---

Kiitos paljon ja happy scratching!
