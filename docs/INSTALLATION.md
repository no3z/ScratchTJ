# ScratchTJ MK2 -- Installation & Setup Guide

## 1. Raspberry Pi Setup

### Operating System

Install Raspberry Pi OS (Lite or Desktop). Tested on Raspbian Buster and Bullseye.

```bash
# Update system
sudo apt update && sudo apt upgrade -y

# Install build dependencies
sudo apt install -y build-essential libasound2-dev libsdl2-dev \
  git i2c-tools
```

### Enable Interfaces

```bash
sudo raspi-config
```

Enable:
- **I2C** (for MT6701 encoder)
- **SPI** (for ST7789 TFT display)
- **Serial** (for Arduino Nano communication -- disable serial console, keep hardware UART)

Reboot after changes.

### AudioInjector HAT

Install the AudioInjector sound card overlay:

```bash
# Add to /boot/config.txt:
dtoverlay=audioinjector-wm8731-audio

# Reboot and verify
aplay -l
# Should show "audioinjector" device
```

Configure ALSA mixer defaults:

```bash
amixer -c 0 sset 'Output Mixer HiFi' on
amixer -c 0 sset 'Line' 31
```

---

## 2. Arduino Nano Firmware

The Arduino Nano reads the DJ fader (analog) and capacitive touch sensor, sending data to the Pi over serial at 500,000 baud.

### Flash the Firmware

1. Connect the Arduino Nano via USB to your development machine
2. Open `arduino_nano/arduino_nano.ino` in the Arduino IDE
3. Select board: **Arduino Nano** (ATmega328P, Old Bootloader for CH340 clones)
4. Select port: `/dev/ttyUSB0` (or equivalent)
5. Upload

### Serial Protocol (MK2)

The Arduino sends 6-byte packets:

| Byte | Content |
|---|---|
| 0 | `0xAA` sync byte |
| 1 | Fader high byte |
| 2 | Fader low byte |
| 3 | Capacitive touch high byte |
| 4 | Capacitive touch low byte |
| 5 | Checksum (XOR of bytes 1-4) |

### Capacitive Touch Circuit

```
Arduino D10 ---- 1.2k ohm ---- Arduino D12 ---- wire ---- spring contact
                                                               |
                                                         copper slip ring
                                                               |
                                                         HDD platter
```

The `CapacitiveSensor` library measures charge time. When a finger touches the metal platter, body capacitance increases the reading.

---

## 3. Wiring

### I2C (MT6701 Encoder)

| MT6701 Pin | Pi Pin |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO 2 (SDA1) |
| SCL | GPIO 3 (SCL1) |

Verify with: `i2cdetect -y 1` -- should show `06` at address 0x06.

### SPI (ST7789 TFT Display)

| TFT Pin | Pi Pin |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SCL | GPIO 11 (SPI0_SCLK) |
| SDA | GPIO 10 (SPI0_MOSI) |
| DC | GPIO 24 |
| RST | GPIO 25 |
| CS | GPIO 8 (SPI0_CE0) |
| BLK | 3.3V (or GPIO for dimming) |

### EC11 Rotary Encoder (Menu Navigation)

| Encoder Pin | Pi Pin |
|---|---|
| CLK | GPIO 23 |
| DT | GPIO 22 |
| SW | GPIO 27 |
| GND | GND |

### Buttons

| Button | Pi Pin | Color |
|---|---|---|
| KB0 (back) | GPIO 17 | -- |
| Cue 1 | Via MCP23017 or direct GPIO | Red |
| Cue 2 | Via MCP23017 or direct GPIO | Green |
| Cue 3 | Via MCP23017 or direct GPIO | Yellow |
| Cue 4 | Via MCP23017 or direct GPIO | Blue |

Buttons are active-low with internal pull-ups enabled.

### Arduino Nano (Serial)

| Arduino Pin | Pi Pin |
|---|---|
| TX | GPIO 15 (RXD) |
| RX | GPIO 14 (TXD) |
| GND | GND |

**Important:** Use a voltage divider or level shifter on the Arduino TX -> Pi RX line if your Nano runs at 5V. The Pi GPIO is 3.3V only.

---

## 4. Build the Software

```bash
# Clone the repository
git clone https://github.com/no3z/ScratchTJ.git
cd ScratchTJ/software

# Build
make -j4

# The binary is ./xwax
```

### Run

```bash
sudo LC_ALL=en_GB.utf8 nice -n -19 ./xwax
```

- `sudo` -- required for direct GPIO/SPI/I2C access
- `LC_ALL=en_GB.utf8` -- required locale setting (xwax fails without it)
- `nice -n -19` -- real-time priority for audio performance

### Music Files

Place audio files (MP3, FLAC, WAV, OGG) in directories. Use the Deck menu to navigate and load tracks.

---

## 5. Configuration

Edit `scsettings.txt` in the software directory, or use the on-device Controller menu.

### Key Parameters

| Parameter | Default | Description |
|---|---|---|
| `buffersize` | 1024 | Audio buffer size (samples) |
| `platterspeed` | 9100 | Encoder-to-audio ratio (9100 = 33rpm for 14-bit) |
| `brakespeed` | 3000 | Stop deceleration time |
| `faderopenpoint` | 5 | Fader open threshold |
| `faderclosepoint` | 3 | Fader close threshold |
| `pitchrange` | 50 | Pitch bend range (%) |
| `platterenabled` | 1 | Enable/disable platter (0/1) |

### Presets

The 3-slot preset system saves all shared variables to files:
- `preset_1.cfg`, `preset_2.cfg`, `preset_3.cfg`
- Save/load through the Presets menu
- Last-used preset is auto-loaded on startup

---

## 6. Development Workflow

### Remote Sync & Build

From your development machine:

```bash
# Sync source files to Pi
rsync -avz --exclude='.git' --exclude='*.o' \
  software/ pi@192.168.1.88:~/ScratchTJ/software/

# Build on Pi
ssh pi@192.168.1.88 "cd ~/ScratchTJ/software && make -j4 2>&1"
```

### Useful Commands

```bash
# Check I2C devices
i2cdetect -y 1

# Read MT6701 angle (raw)
i2cget -y 1 0x06 0x03 w

# Monitor serial from Arduino
stty -F /dev/ttyUSB0 500000 raw
cat /dev/ttyUSB0 | xxd

# Test TFT display
cd software && make test_tft_ui && sudo ./test_tft_ui
```

---

## Troubleshooting

| Problem | Solution |
|---|---|
| "Could not honour the local encoding" | Set `LC_ALL=en_GB.utf8` before running |
| No sound output | Check `amixer` settings, verify AudioInjector overlay |
| TFT blank | Check SPI enabled, verify wiring (DC/RST pins) |
| Encoder not detected | Run `i2cdetect -y 1`, check I2C enabled |
| Arduino not communicating | Check `/dev/ttyUSB0` exists, verify baud rate 500000 |
| Platter not responding | Check `platterenabled=1` in scsettings.txt |
| Buttons not working | Check GPIO pin mapping in scsettings.txt |
| Permission denied | Run with `sudo` |
