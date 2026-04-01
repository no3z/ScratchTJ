# ScratchTJ Build Guide

**A step-by-step guide to building your own portable DJ scratch instrument**

> This guide is written so that anyone can follow along. We'll explain every part, every wire, and every trick.

---

## What Are We Building?

ScratchTJ is a portable digital turntable you can use to scratch sounds like a DJ. It has two "decks" — one plays a beat loop, the other plays a scratch sample — and a crossfader lets you cut between them, just like on a real DJ setup.

![Finished ScratchTJ — platter, TFT display, colored cue buttons, fader, and encoder knob](../../docs/image6.jpeg)

**MK2 build video** — testing all new components:

[![MK2 build video](https://img.youtube.com/vi/ob8X8q4Xurs/maxresdefault.jpg)](https://youtu.be/ob8X8q4Xurs)

**How it works in a nutshell:**

1. You spin a **hard drive platter** (the shiny disc from inside an old hard drive)
2. An **MT6701 magnetic encoder** underneath detects angle and direction via I2C, read directly by the Pi
3. An **Arduino Nano** reads the fader, touch sensor, and 4 cue buttons and sends the data to a **Raspberry Pi**
4. The **Raspberry Pi** plays audio — scratching the sound forward and backward based on how you move the platter
5. A **DJ fader** lets you cut the sound in and out
6. A **TFT display** and **buttons** let you pick tracks and change settings

---

## Parts You Need (Bill of Materials)

Here's your shopping list. Most of these can be found on AliExpress, Amazon, or eBay.

| Part | What It Does | Approx. Price |
|------|-------------|---------------|
| **Raspberry Pi 2** (or 3/4) | The brain — runs the audio software | $15-40 |
| **AudioInjector Sound Card** | I2S audio hat for high-quality sound | $20-30 |
| **Arduino Nano** | Reads the fader, touch sensor, and cue buttons | $3-5 |
| **MT6701 Hall-Effect Angle Sensor** | 14-bit magnetic encoder for platter position (I2C at 0x06) | $3-5 |
| **Rotary Encoder with Push Button** (small, for menu) | Navigate the TFT menu by turning and clicking | $1-2 |
| **ST7789 240x240 TFT Display** | Shows menus, track names, settings (SPI interface) | $3-6 |
| **DJ Crossfader** (any linear fader) | Cut between beats and scratch sample | $5-15 |
| **Hard Drive Platter** | The spinning disc you touch to scratch | Free (from old HDD) |
| **1200 Ohm Resistor** (1.2kΩ) | Used for capacitive touch sensing | $0.10 |
| **4x Cue Buttons** | Dedicated cue point triggers (on Arduino A0-A3) | $1 |
| **2x Navigation Buttons** | Enter and Back buttons for menu (GPIO 17, GPIO 27) | $0.50 |
| **Jumper Wires** | For all the connections | $2-3 |
| **Micro USB Cable** | Powers the Arduino from the Pi | $1 |
| **Power Supply** (5V 2.5A for Pi) | Powers the whole thing | $5-8 |

**Optional but recommended:**

- 3D printer (for the enclosure and HDD adapter)
- Soldering iron and solder
- Breadboard (for testing before soldering)
- Hot glue gun

---

## Step 1: Understanding the Big Picture

Before we wire anything, let's understand how data flows through the system:

```
┌─────────────────────────────────────────────────────────────────┐
│                        ARDUINO NANO                             │
│                                                                 │
│  ┌──────────┐  ┌────────────────────────┐  ┌────────────────┐  │
│  │ DJ Fader │  │ Capacitive Touch       │  │ 4 Cue Buttons  │  │
│  │          │  │ (HDD Platter + 1.2kΩ)  │  │                │  │
│  │ → A5     │  │ Send pin → D10         │  │ A0, A1, A2, A3 │  │
│  │          │  │ Sense pin → D12        │  │ (INPUT_PULLUP)  │  │
│  └──────────┘  └────────────────────────┘  └────────────────┘  │
│                                                                 │
│  Sends 7-byte binary packet at ~500 Hz                          │
│  over serial TX at 500,000 baud                                 │
│                                                                 │
└──────────────────────┬──────────────────────────────────────────┘
                       │ Serial (TX/RX)
                       │ 500,000 baud
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│                      RASPBERRY PI                               │
│                                                                 │
│  ┌──────────┐  ┌──────────────┐  ┌──────────────────────────┐  │
│  │ xwax     │  │ TFT Display  │  │ AudioInjector Sound Card │  │
│  │ audio    │  │ ST7789 SPI   │  │ I2S audio output         │  │
│  │ engine   │  │              │  │                           │  │
│  └──────────┘  │ DC  → GPIO24 │  │ Output → Headphones /    │  │
│                │ RST → GPIO25 │  │          Speaker          │  │
│  ┌──────────────────────┐     │  └──────────────────────────┘  │
│  │ MT6701 Encoder (I2C) │     │                                │
│  │ Address 0x06          │     │                                │
│  │ on /dev/i2c-1         │     │                                │
│  └──────────────────────┘     │                                │
│  ┌─────────────────────┐  ┌──────────────────┐                 │
│  │ Menu Rotary Encoder  │  │ 2 Nav Buttons    │                 │
│  │ CLK → GPIO 23        │  │ Enter → GPIO 17  │                 │
│  │ DT  → GPIO 22        │  │ Back  → GPIO 27  │                 │
│  │ SW  → GPIO 27        │  │                   │                 │
│  └─────────────────────┘  └──────────────────┘                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Step 2: Setting Up the Arduino Nano

The Arduino Nano reads three types of input and sends the data to the Raspberry Pi over serial.

### 2.1 — The DJ Crossfader

The fader is a simple potentiometer (variable resistor). It outputs a voltage between 0V and 5V depending on its position.

**Wiring:**

```
DJ Fader                 Arduino Nano
────────                 ────────────
  Left pin   ──────────►  GND
  Middle pin ──────────►  A5 (analog input)
  Right pin  ──────────►  5V
```

> **Tip:** If the fader direction feels backwards when you test it, you can swap the Left and Right connections, or change it in software later using the `Fad Switch` setting in the menu.

### 2.2 — Capacitive Touch Sensing (The Cool Part!)

This is the magic trick that lets the system know when your hand is touching the platter. Here's how it works:

A hard drive platter is a metal disc. Your body has a tiny amount of electrical capacitance (ability to store charge). When you touch the metal platter, the capacitance measured by the Arduino changes. The Arduino library `CapacitiveSensor` measures how long it takes to charge a small circuit through a resistor — when you touch the platter, it takes longer, and the reading goes up.

**Wiring:**

```
Arduino Nano                                    HDD Platter
────────────                                    ───────────
                    1200Ω Resistor
  D10 (send) ──────┤├──────────── D12 (sense) ──── wire to platter
```

Here's what's happening electrically:

```
   D10            1.2kΩ           D12
    │            ┌─┤├─┐            │
    │ (send)     │     │  (sense)  │
    ├────────────┘     └───────────┤
    │                              │
    │                         ┌────┴────┐
    │                         │  wire   │
    │                         │  runs   │
    │                         │ through │
    │                         │ encoder │
    │                         │  shaft  │
    │                         │    ↓    │
    │                         │  HDD    │
    │                         │ Platter │
    │                         └─────────┘
    │                              │
    │                         Your finger
    │                         touches the
    │                         metal platter
    │                         adding ~human
    │                         body capacitance
```

> **The trick:** You need a thin wire (like 1.5mm copper wire) that runs through a channel in the encoder shaft adapter, making electrical contact with the spinning HDD platter. The 3D-printed HDD adapter has channels for this wire.

> **The 1200Ω resistor:** This value is tuned for this specific setup. A larger resistor makes the sensor more sensitive but slower. 1200Ω gives a good balance between sensitivity and speed for DJ scratching.

**How the readings work:**

- **Not touching:** The capacitive sensor reads a low value (maybe 100-2000)
- **Touching the platter:** The reading jumps up significantly (maybe 5000-30000)
- The software has a threshold (`cap_threshold`, default 5000) — above this means "touched"
- There's also `cap_hysteresis` (default 500) to prevent flickering at the boundary

### 2.3 — The 4 Cue Buttons

Four momentary push buttons connected to analog pins on the Arduino, used as cue point triggers for the scratch deck.

**Wiring:**

```
Cue Buttons              Arduino Nano
───────────              ────────────
  Button 1  ─────────►  A0 (to GND)
  Button 2  ─────────►  A1 (to GND)
  Button 3  ─────────►  A2 (to GND)
  Button 4  ─────────►  A3 (to GND)
```

Each button connects between its Arduino pin and GND. The Arduino uses internal pull-up resistors (`INPUT_PULLUP`), so no external resistors are needed. When pressed, the pin reads LOW.

> **How the buttons work:** In the scratch deck, **short press** jumps to a saved cue point, **long press** (1 second) sets the cue point at the current playback position. Each button has its own independent cue.

![Arduino Nano mounted on 3D-printed base with USB cable, resistors, and wiring](../../docs/image4.jpeg)

### 2.4 — Complete Arduino Wiring Summary

```
┌─────────────────────────────────────────────┐
│              ARDUINO NANO                    │
│                                              │
│  D10 ──► Cap Sensor SEND (through 1.2kΩ)    │
│  D12 ◄── Cap Sensor SENSE (wire to platter) │
│  A0  ◄── Cue Button 1 (to GND)              │
│  A1  ◄── Cue Button 2 (to GND)              │
│  A2  ◄── Cue Button 3 (to GND)              │
│  A3  ◄── Cue Button 4 (to GND)              │
│  A5  ◄── Fader middle pin                   │
│  5V  ──► Fader right pin                     │
│  GND ──► Fader left pin, all button legs     │
│  TX  ──► Raspberry Pi RX (GPIO 15)           │
│  RX  ◄── Raspberry Pi TX (GPIO 14)           │
│                                              │
└─────────────────────────────────────────────┘
```

### 2.5 — Upload the Arduino Code

1. Install the **Arduino IDE** on your computer
2. Go to **Sketch > Include Library > Manage Libraries** and search for **CapacitiveSensor** — install it
3. Open `arduino_nano/arduino_nano.ino` from the ScratchTJ project
4. Select **Tools > Board > Arduino Nano**
5. Select the correct **Port**
6. Click **Upload**

> **Testing:** Open the Serial Monitor at 500000 baud. You should see a stream of binary data (it'll look like garbage in text mode, and that's correct). If you see nothing, check your wiring.

---

## Step 3: Setting Up the Raspberry Pi

### 3.1 — Install the Operating System

1. Download **Raspberry Pi OS Lite** (no desktop needed — this project runs headless)
2. Flash it to an SD card using **Raspberry Pi Imager**
3. Boot the Pi, connect via SSH or keyboard+monitor
4. Run updates:
   ```bash
   sudo apt update && sudo apt upgrade -y
   ```

### 3.2 — Install the AudioInjector Sound Card

The AudioInjector is an I2S sound card that sits on top of the Pi's GPIO header.

1. Place the AudioInjector HAT on the Pi's GPIO pins
2. Enable I2S audio by editing `/boot/config.txt`:
   ```bash
   sudo nano /boot/config.txt
   ```
   Add this line:
   ```
   dtoverlay=audioinjector-wm8731-audio
   ```
   Comment out (add `#` before) the default audio:
   ```
   #dtparam=audio=on
   ```
3. Reboot:
   ```bash
   sudo reboot
   ```
4. Verify:
   ```bash
   aplay -l
   ```
   You should see the AudioInjector listed as a sound card.

### 3.3 — Enable the Serial Port and I2C

The Pi communicates with the Arduino over its hardware serial port (`/dev/serial0`) and reads the MT6701 encoder over I2C.

1. Run `sudo raspi-config`
2. Go to **Interface Options > Serial Port**
3. Say **No** to "login shell over serial"
4. Say **Yes** to "serial port hardware enabled"
5. Go to **Interface Options > I2C** → Enable
6. Go to **Interface Options > SPI** → Enable (for the TFT display)
7. Reboot

### 3.4 — Connect the Arduino to the Pi (Serial)

```
Arduino Nano             Raspberry Pi
────────────             ────────────
  TX         ──────────►  GPIO 15 (RXD)
  RX         ◄──────────  GPIO 14 (TXD)
  GND        ──────────►  GND
```

> **Important:** The Arduino Nano runs at 5V logic and the Raspberry Pi GPIO pins are 3.3V. The Pi's RX pin can tolerate 5V from the Arduino TX, but if you want to be safe, use a voltage divider (two resistors) on the Arduino TX → Pi RX line. Many people skip this and it works fine, but be aware of the risk.

> **Power:** You can power the Arduino from the Pi's 5V pin, or use a separate USB cable. Using the Pi's 5V is simpler.

### 3.5 — Wire the MT6701 Encoder (I2C)

The MT6701 is a 14-bit Hall-effect angle sensor that provides 16384 counts per revolution. The Pi reads it directly over I2C — no Arduino involved.

```
MT6701                   Raspberry Pi
──────                   ────────────
  VCC          ──────────►  3.3V
  GND          ──────────►  GND
  SDA          ──────────►  GPIO 2 (SDA, Pin 3)
  SCL          ──────────►  GPIO 3 (SCL, Pin 5)
```

Verify with:
```bash
sudo i2cdetect -y 1
```
You should see address **0x06**.

![Encoder mount housing with connector pins visible from the side](../../docs/image5.jpeg)

> **Mounting:** The MT6701 goes underneath the platter. A small diametrically magnetized magnet attaches to the platter shaft and sits above the sensor. As the platter spins, the magnetic field rotates and the sensor reports the absolute angle.

### 3.6 — Wire the TFT Display (SPI)

The ST7789 240x240 TFT display connects via SPI:

```
ST7789 TFT               Raspberry Pi
──────────               ────────────
  VCC          ──────────►  3.3V
  GND          ──────────►  GND
  SCL (SCLK)  ──────────►  SPI0 CLK (GPIO 11, Pin 23)
  SDA (MOSI)  ──────────►  SPI0 MOSI (GPIO 10, Pin 19)
  CS           ──────────►  SPI0 CE0 (GPIO 8, Pin 24)
  DC           ──────────►  GPIO 24 (Pin 18)
  RST          ──────────►  GPIO 25 (Pin 22)
```

The display runs at 40 MHz SPI in mode 0.

### 3.7 — Wire the Menu Rotary Encoder

This is the small rotary encoder (with a click button) used to navigate the TFT menu — not the MT6701 platter sensor.

```
Menu Encoder             Raspberry Pi
────────────             ────────────
  CLK          ──────────►  GPIO 23
  DT           ──────────►  GPIO 22
  SW (button)  ──────────►  GPIO 27
  +            ──────────►  3.3V
  GND          ──────────►  GND
```

> **How this encoder works in the menu:** Turn it left/right to scroll through options. Click the button to go back. Long-press the button to enter **Pitch Mode** (adjusts playback speed).

### 3.8 — Wire the Two Navigation Buttons

These two buttons are used for Enter/Back navigation in the menu, and they also work as cue point triggers in the deck's CUE screen.

```
Buttons                  Raspberry Pi
───────                  ────────────
  Enter button  ─────────►  GPIO 17
  Back button   ─────────►  GPIO 27
  (other leg of each) ───►  GND
```

> **Pull-up resistors:** The Pi's internal pull-up resistors are used (enabled in software), so you don't need external resistors. Just wire each button between its GPIO pin and GND.

---

## Step 4: Complete Raspberry Pi Wiring Summary

Here's every wire that connects to the Raspberry Pi:

```
┌────────────────────────────────────────────────────────────┐
│                    RASPBERRY PI GPIO                        │
│                                                             │
│  Pin 1  (3.3V)    ──► MT6701 VCC, TFT VCC, Encoder +      │
│  Pin 2  (5V)      ──► Arduino 5V                            │
│  Pin 3  (GPIO 2)  ──► MT6701 SDA (I2C data)                │
│  Pin 5  (GPIO 3)  ──► MT6701 SCL (I2C clock)               │
│  Pin 6  (GND)     ──► All GNDs                              │
│  Pin 8  (GPIO 14) ──► Arduino RX                            │
│  Pin 10 (GPIO 15) ◄── Arduino TX                            │
│  Pin 11 (GPIO 17) ◄── Enter Button (KB0)                    │
│  Pin 13 (GPIO 27) ◄── Back Button / Encoder SW              │
│  Pin 15 (GPIO 22) ◄── Menu Encoder DT                       │
│  Pin 16 (GPIO 23) ◄── Menu Encoder CLK                      │
│  Pin 18 (GPIO 24) ──► TFT DC                                │
│  Pin 19 (MOSI)    ──► TFT SDA (data)                        │
│  Pin 22 (GPIO 25) ──► TFT RST                               │
│  Pin 23 (SCLK)    ──► TFT SCL (clock)                       │
│  Pin 24 (CE0)     ──► TFT CS                                │
│                                                             │
│  AudioInjector HAT sits on top of the GPIO header           │
│  (uses I2S pins internally)                                 │
│                                                             │
└────────────────────────────────────────────────────────────┘
```

![Internal wiring — Arduino, encoder, DuPont connectors, and routing](../../docs/image1.jpeg)

![Cable routing inside the enclosure](../../docs/image2.jpeg)

> **Note:** The AudioInjector HAT plugs onto the GPIO header and uses some pins for I2S audio. It has pass-through headers so you can still access the other GPIO pins. Make sure none of your wires conflict with the AudioInjector's pins.

---

## Step 5: The 3D-Printed Parts

You need to print three parts. All models are on Tinkercad and can be printed on any standard 3D printer.

### 5.1 — HDD Platter Adapter

**[Download from Tinkercad](https://www.tinkercad.com/things/61eF1Ijn7o5-hdd-adapter?sharecode=nWsXvmSv_DllBxx8CcdytpptvyzZCUKnqFkQ3bDZFho)**

This adapter connects the HDD platter to the encoder shaft. The key feature is a **channel for the capacitive touch wire** — a thin copper wire runs through this channel, making contact with the platter so the Arduino can detect when you're touching it.

### 5.2 — Main Enclosure

**[Download from Tinkercad](https://www.tinkercad.com/things/2LCXX7xvP9b-tinkerscratchv0.1?sharecode=RHVKMN4xlvb5UvUtA5s9apYJHQghMAneHwZlXMxaT3Y)**

Houses the Raspberry Pi, Arduino, fader, TFT display, and encoder. The platter encoder mounts underneath with the shaft poking through the top.

![Enclosure side view — Arduino panel and encoder hole visible](../../docs/image3.jpeg)

### 5.3 — Button Extender

**[Download from Tinkercad](https://www.tinkercad.com/things/4uivv2bXmRU-button-extender-scratch)**

An add-on bracket that holds the navigation/cue buttons in a comfortable position.

![Assembled top view — platter, TFT, and encoder from above](../../docs/image8.jpeg)

---

## Step 6: Build the Software

### 6.1 — Install Dependencies

On the Raspberry Pi:

```bash
sudo apt install -y build-essential libsdl2-dev libasound2-dev wiringpi git
```

### 6.2 — Clone the Project

```bash
git clone https://github.com/no3z/ScratchTJ.git
cd ScratchTJ/software
```

### 6.3 — Compile

```bash
make
```

This compiles the xwax-based audio engine with all the ScratchTJ modifications.

### 6.4 — Add Audio Samples

Create two folders for your audio:

```bash
mkdir -p ~/samples ~/beats
```

- Put your **scratch samples** (short sounds, stabs, vocal chops) in `~/samples/`
- Put your **beat loops** in `~/beats/`

Supported format: WAV files (any sample rate, the engine will resample).

### 6.5 — Run!

```bash
cd ~/ScratchTJ/software
sudo LC_ALL=en_GB.utf8 nice -n -19 ./xwax
```

The TFT display should light up and show the main menu. You can now browse and load tracks on each deck.

> **Note:** The `LC_ALL` locale must be set or xwax will fail with "Could not honour the local encoding". The `nice -n -19` gives the process highest scheduling priority for low-latency audio.

---

## Step 7: How to Use It

### Basic Scratching

1. Load a sample on **Deck 1** (Samples) using the menu
2. Load a beat on **Deck 0** (Beats)
3. Start the beat playing (use the menu or button)
4. Touch the platter and spin it — you'll hear the scratch sample move forward and backward
5. Use the crossfader to cut the scratch in and out over the beat

### Setting Cue Points

1. Navigate to a deck and select **CUE**
2. Play or scratch to the position you want
3. **Long-press** one of the 4 cue buttons (or Enter/Back) to save that position
4. **Short-press** the same button to instantly jump back to that cue point
5. Each button has its own independent cue — so you get multiple hot cues per deck

### Recording Live Audio

1. Navigate to a deck and select **Record**
2. Choose your input source (from the list of available ALSA capture devices)
3. Start recording — audio from the sound card input is captured to a WAV file
4. Stop recording — the file is automatically loaded into the deck
5. You can now scratch with the audio you just recorded!

### Adjusting Settings

Go to **Config > Global Settings** to tweak parameters in real time:

- **platterspeed** (default 1333): How the encoder maps to audio position. Higher = faster.
- **pitch_filter** (default 0.2): Smoothness of pitch response. Lower = smoother. Higher = more responsive.
- **cap_threshold** (default 5000): The capacitive sensor level that counts as "touched." Adjust if touch detection is too sensitive or not sensitive enough.
- **slippiness** (default 200): How the virtual slipmat feels when you release the platter.
- **fader_sharp**: Controls how sharp the fader cut feels.

### Saving Presets

Found settings you like? Go to **Config > Presets** to save them into one of 5 preset slots. You can load them back anytime or reset to defaults.

---

## Step 8: Troubleshooting

### "The platter doesn't respond"
- Check that the MT6701 is detected on I2C: `sudo i2cdetect -y 1` — should show 0x06
- Make sure I2C is enabled in `raspi-config`
- Check wiring: MT6701 SDA → GPIO 2, SCL → GPIO 3
- Verify the magnet is properly aligned above the MT6701 sensor

### "Touch detection doesn't work"
- Check the 1200Ω resistor is between D10 and D12 on the Arduino
- Make sure the wire from D12 actually makes physical contact with the HDD platter
- Try adjusting `cap_threshold` in the settings (lower = more sensitive)
- Open the Arduino Serial Monitor and check if the capacitive value changes when you touch the platter

### "The TFT display is blank"
- Check SPI wiring: MOSI → GPIO 10, SCLK → GPIO 11, CS → CE0, DC → GPIO 24, RST → GPIO 25
- Make sure SPI is enabled in `raspi-config`
- Check that the display gets 3.3V power

### "Audio sounds choppy or glitchy"
- Increase `buffersize` in `scsettings.txt` (try 2048 or 4096)
- Make sure no other processes are consuming CPU
- Check that the AudioInjector is properly seated on the GPIO header
- Make sure you're running with `nice -n -19` for priority scheduling

### "The fader direction is backwards"
- Change the `Fad Switch` parameter to 1 (or 0) in Config > Global Settings
- Or swap the left/right wires on the fader

### "Serial communication fails"
- Check Arduino TX → Pi GPIO 15, Arduino RX → Pi GPIO 14
- Verify serial is enabled in `raspi-config` (hardware enabled, login shell disabled)
- Check that `/dev/serial0` exists: `ls -la /dev/serial0`
- The watchdog auto-reconnects after 2 seconds and resets the Arduino via DTR after 3 failures

---

## How the Software Works (For the Curious)

If you want to understand or modify the code, here's what each file does:

| File | What It Does |
|------|-------------|
| `arduino_nano.ino` | Reads fader (A5), capacitive sensor (D10/D12), and 4 cue buttons (A0-A3). Sends 7-byte binary packets at ~500 Hz over serial at 500,000 baud. |
| `sc_input.c` | Receives serial data on the Pi, reads MT6701 encoder over I2C, calculates platter position and velocity, detects touch on/off with hysteresis, processes cue button presses. |
| `player.c` | The audio engine — uses cubic interpolation to play audio at variable speed based on platter movement. Has pitch filtering and slipmat simulation. |
| `lcd_menu.c` | Drives the ST7789 TFT display and handles the menu rotary encoder and buttons. |
| `deck_menu.c` | Each deck's submenu: file browser, transport controls, CUE screen (with cue button control), and recording. |
| `recording.c` | Captures live audio from ALSA input devices and saves to WAV. |
| `cues.c` | Manages cue points — save, load, jump. Persists cue positions to files alongside the audio tracks. |
| `shared_variables.c` | Thread-safe system for runtime-tunable parameters. The TFT menu reads/writes these in real time. |
| `preset_menu.c` | Save/load/reset all parameters across 5 preset slots stored in `~/.scratchtj/presets/`. |
| `st7789.c` | ST7789 TFT display driver — SPI communication at 40 MHz. |
| `gpio_direct.c` | Direct GPIO access for Raspberry Pi — reads buttons and encoder without wiringPi overhead. |

### The Binary Serial Protocol

Every ~2ms, the Arduino sends exactly 7 bytes:

```
Byte 0: 0xAA        (sync byte — the Pi scans for this to find packet start)
Byte 1: Fader High   (fader value, 0-1023, high byte)
Byte 2: Fader Low    (fader value, low byte)
Byte 3: Cap High     (capacitive sensor value, high byte)
Byte 4: Cap Low      (capacitive sensor value, low byte)
Byte 5: Buttons      (4-bit bitfield, one bit per cue button)
Byte 6: Checksum     (XOR of bytes 1-5, for error detection)
```

The Pi validates every packet by checking the sync byte and XOR checksum. If communication is lost for 2 seconds, a watchdog timer triggers and auto-reconnects (with a hard DTR reset of the Arduino after 3 failures).

The Pi also auto-detects legacy 8-byte packets from older Arduino firmware for backwards compatibility.

---

## Credits

- Based on the **[SC1000 Open Source Turntable](https://github.com/rasteri/SC1000)** by **the_rasteri**
- Audio engine from **[xwax](http://www.xwax.co.uk/)** by Mark Hills, licensed under GNU GPL v2
- Inspiration from the [SC500 DIY CDJ build](https://www.youtube.com/watch?v=j9CJ7EI0yY4)

---

**Happy scratching!** If you build one, share it — open an issue or PR on [GitHub](https://github.com/no3z/ScratchTJ).
