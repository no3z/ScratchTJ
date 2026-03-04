# ScratchTJ: DIY Digital Turntable with Raspberry Pi

A fully open-source portable scratch instrument built around a Raspberry Pi, Arduino Nano, and the [xwax](http://www.xwax.co.uk/) digital vinyl emulation engine. Inspired by the [SC1000 Open Source Turntable](https://github.com/rasteri/SC1000) by **the_rasteri**.

**Demo video** (click to watch):

[![Watch the video](https://img.youtube.com/vi/dSP_wy3YE4I/maxresdefault.jpg)](https://youtu.be/dSP_wy3YE4I)

[![Watch the video](https://img.youtube.com/vi/rufXcn8hjYE/maxresdefault.jpg)](https://youtu.be/rufXcn8hjYE)

**📖 [Full Build Guide](docs/BUILD_GUIDE.md)** — Step-by-step instructions with wiring diagrams, pin connections, and troubleshooting.

## Features

- Two independent decks: beats + scratch sample, mixed through a DJ fader
- 600 PPR rotary encoder platter with capacitive touch via HDD platter
- 1602A LCD display with rotary encoder for menu navigation
- Two dedicated navigation buttons (Enter / Back) on GPIO 17 and 27 with cue point control
- Live input recording from the sound card with source selection
- 5-slot preset system to save and load all settings
- Binary serial protocol at 500 kbaud between Arduino and Pi
- All key parameters tunable in real time from the LCD config menu
- ALSA mixer control for input/output levels
- Pitch mode via rotary encoder long press

## Architecture

```
+--------------+   Serial 500kbaud   +--------------+   I2S Audio   +-----------------+
| Arduino Nano | ------------------- | Raspberry Pi | ------------ | AudioInjector   |
|              |   8-byte binary pkt |              |              | Sound Card      |
| - Encoder    |   (sync+data+XOR)   | - xwax       |              +-----------------+
| - Fader      |                     | - LCD menu   |
| - Cap sensor |                     | - Recording  |
+--------------+                     +--------------+
```

The Arduino reads the 600 PPR rotary encoder (2400 CPR with 4x quadrature decoding), the DJ crossfader, and the capacitive touch sensor at 200 Hz. Data is sent as 8-byte binary packets with XOR checksum. The Raspberry Pi runs the audio engine, LCD menu system, and live recording.

## Bill of Materials

- **Raspberry Pi 2** (or compatible)
- **AudioInjector Sound Card** (I2S audio hat)
- **1602A LCD Display with I2C IC**
- **Rotary Encoder with Push Button** (for menu navigation)
- **600 PPR Magnetoelectric Rotary Encoder** (for the platter)
- **Arduino Nano** (connected via TX/RX serial at 500 kbaud)
- **DJ Fader**
- **Hard Drive Platter** (capacitive touch surface)
- **1200 Ohm Resistor**
- **2x Push Buttons** (Enter/Back, on GPIO 17 and 27)

![AliExpress Components](https://github.com/no3z/ScratchTJ/raw/main/docs/alicomponents.png)

## 3D Printed Parts

All parts were designed in Tinkercad and can be printed on a standard FDM printer.

- **HDD Adapter**: [Tinkercad Model](https://www.tinkercad.com/things/61eF1Ijn7o5-hdd-adapter?sharecode=nWsXvmSv_DllBxx8CcdytpptvyzZCUKnqFkQ3bDZFho) - Channels for 1.5 mm wire connecting the platter to the shaft for capacitive touch.
- **Enclosure**: [Tinkercad Model](https://www.tinkercad.com/things/2LCXX7xvP9b-tinkerscratchv0.1?sharecode=RHVKMN4xlvb5UvUtA5s9apYJHQghMAneHwZlXMxaT3Y) - Houses all components.
- **Buttons Extender**: [Tinkercad Model](https://www.tinkercad.com/things/4uivv2bXmRU-button-extender-scratch) - Add-on structure for the two navigation buttons.

![2 Buttons Add-on](https://github.com/no3z/ScratchTJ/raw/main/docs/2buttons.png)

![3D Print 1](https://github.com/no3z/ScratchTJ/raw/main/docs/IMG_3892.jpg)
![3D Print 2](https://github.com/no3z/ScratchTJ/raw/main/docs/IMG_3893.jpg)

## Build Photos

![Soldering Station](https://github.com/no3z/ScratchTJ/raw/main/docs/soldering_station_setup.jpg)
![Enclosure and Controls](https://github.com/no3z/ScratchTJ/raw/main/docs/enclosure_and_controls.jpg)
![Testing the Assembly](https://github.com/no3z/ScratchTJ/raw/main/docs/testing_assembly.jpg)
![Workbench Setup](https://github.com/no3z/ScratchTJ/raw/main/docs/initial_build_setup.jpg)
![Optical Encoder and HDD Platter](https://github.com/no3z/ScratchTJ/raw/main/docs/optical_encoder_and_platter.jpg)

## Software Overview

The software is a fork of the [SC1000 codebase](https://github.com/rasteri/SC1000) heavily modified for this hardware. Key source files:

| File | Purpose |
|------|---------|
| `arduino_nano/arduino_nano.ino` | Arduino firmware: encoder, fader, cap sensor, binary serial protocol |
| `software/sc_input.c` | Serial reader, encoder processing, platter-to-audio position mapping |
| `software/player.c` | Audio engine with cubic interpolation, pitch filtering, slipmat simulation |
| `software/xwax.c` | Main application, deck init, shared variable registration |
| `software/recording.c` | Live input recording from ALSA capture devices |
| `software/cues.c` | Cue point system with per-track save/load |
| `software/lcd_menu.c` | LCD display, rotary encoder menu navigation, 2-button support |
| `software/deck_menu.c` | Per-deck menu: file browse, transport, cue screen, recording |
| `software/controller_menu.c` | Config menu: Sound Settings, Global Settings, Info, Presets |
| `software/preset_menu.c` | 5-slot preset save/load/reset system |
| `software/shared_variables.c` | Thread-safe runtime variable system for LCD-tunable parameters |
| `software/scsettings.txt` | Persistent configuration file |

### LCD Menu Structure

```
Main Menu
|-- Deck 0 (Beats) -- file browse, transport, volume, cue points, recording
|-- Deck 1 (Samples) -- file browse, transport, volume, cue points, recording
+-- Config
    |-- Sound Settings -- ALSA mixer controls
    |-- Global Settings -- all runtime-tunable parameters (see below)
    |-- Info -- system information
    +-- Presets -- save/load/reset across 5 slots
```

Navigation: rotary encoder scrolls, **Enter** button (GPIO 17) selects, **Back** button (GPIO 27) returns. Long-pressing the rotary encoder enters **Pitch Mode** for adjusting deck playback speed.

### Two-Button Cue System

Each deck has a **CUE** screen where the two physical buttons (Enter / Back) act as independent cue point triggers:

- **Short press** = jump to that button's saved cue point
- **Long press** = set the cue point at the current playback position

Cue positions are saved per track and persist between sessions. This gives you instant access to two hot cues per deck while scratching, without needing to navigate the menu.

### Live Input Recording

The deck menu includes a **Record** option that lets you capture audio directly from the sound card inputs. You can select the input source from a list of available ALSA capture devices, start/stop recording, and the resulting WAV file is automatically loaded into the deck for immediate playback and scratching.

### Runtime-Tunable Parameters

All of these can be adjusted live from the LCD Config > Global Settings menu:

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

The Arduino and Pi communicate using a compact binary protocol at 500 kbaud:

```
Byte:  [0]    [1]    [2]    [3]     [4]     [5]    [6]    [7]
       SYNC   fHi    fLo    angHi   angLo   capHi  capLo  XOR
       0xAA   ---fader----   ---encoder---   --capacitive-  checksum
```

On startup the Pi sends `0x53` ('S'), the Arduino replies `'T'` to confirm the link. A watchdog on the Pi side detects 2-second timeouts and auto-reconnects, with DTR hard-reset of the Arduino after 3 consecutive failures.

### Encoder Optimizations

The system is tuned for a 600 PPR encoder (2400 CPR with 4x quadrature decoding). Unlike the original SC1000's AS5601 magnetic sensor (4096 steps, absolute position over I2C), this uses incremental quadrature with interrupt-driven counting on the Arduino. Key optimizations:

- Native 2400 CPR throughout the pipeline, no lossy mapping to 4096
- Velocity estimation between serial frames for position interpolation
- Cached shared variable pointers in the audio hot path (no strcmp/mutex per frame)
- Full quadrature state table for the menu rotary encoder (all 4 transitions detected)
- Capacitive touch hysteresis to prevent flicker-induced audio clicks

## Building

### Prerequisites (on the Raspberry Pi)

- GCC, make
- SDL2 development libraries
- ALSA development libraries
- wiringPi

### Compile

```bash
cd software
make
```

### Flash the Arduino

Open `arduino_nano/arduino_nano.ino` in the Arduino IDE. Install the `CapacitiveSensor` library, select Arduino Nano, and upload. Both the Arduino firmware and the Pi software must be updated together since they share the binary serial protocol.

### Run

```bash
cd software
./xwax
```

Audio samples go in `~/samples/` (scratch deck) and `~/beats/` (beat deck). The paths are configured in `xwax.c`.

## Project History

This project started as an adaptation of the SC1000 to run on a Raspberry Pi 2 with an AudioInjector audio hat. The original SC1000 runs on an Olimex A13 board with a custom PCB. ScratchTJ replaces the custom PCB with off-the-shelf components (Arduino Nano, standard rotary encoder, DJ fader) and uses serial communication instead of I2C/SPI.

The initial inspiration came from these videos: [SC500 DIY CDJ build](https://www.youtube.com/watch?v=j9CJ7EI0yY4) and [SC1000 Open Source Turntable](https://youtu.be/Llxfi6l2I-U). If you want the real deal, check out the [SC1000 MK2](https://portablismgear.com/sc1000/devices/sc1000mk2.html).

## More Videos

[![Scratching demo](https://img.youtube.com/vi/jpz3jol8UZQ/maxresdefault.jpg)](https://youtu.be/jpz3jol8UZQ)

[![Fader and controls](https://img.youtube.com/vi/uF4GSIXVzZU/maxresdefault.jpg)](https://youtu.be/uF4GSIXVzZU)

## Contributing

If you're interested in building your own ScratchTJ or have ideas to improve it, feel free to open an issue or PR. The project is not perfect, and there are still many ways to make it better -- that's the beauty of open source.

## License

Based on [xwax](http://www.xwax.co.uk/) by Mark Hills, licensed under GNU GPL v2.

---

Kiitos paljon ja happy scratching!
