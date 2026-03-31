#!/usr/bin/env python3
"""TFT diagnostic - tries different ST7789 configs to find what works."""

import time
import spidev
import RPi.GPIO as GPIO

DC  = 24
RST = 25
CS  = 8

W, H = 240, 240

GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)
GPIO.setup(DC, GPIO.OUT)
GPIO.setup(RST, GPIO.OUT)

spi = spidev.SpiDev()
spi.open(0, 0)
spi.mode = 0

def cmd(c, data=None):
    GPIO.output(DC, 0)
    spi.writebytes([c])
    if data:
        GPIO.output(DC, 1)
        spi.writebytes(data)

def reset():
    GPIO.output(RST, 1); time.sleep(0.1)
    GPIO.output(RST, 0); time.sleep(0.1)
    GPIO.output(RST, 1); time.sleep(0.2)

def init_display():
    reset()
    cmd(0x01); time.sleep(0.2)   # SW reset
    cmd(0x11); time.sleep(0.2)   # sleep out
    cmd(0xB2, [0x0C, 0x0C, 0x00, 0x33, 0x33])  # porch
    cmd(0xB7, [0x35])            # gate control
    cmd(0xBB, [0x19])            # VCOM
    cmd(0xC0, [0x2C])            # LCM
    cmd(0xC2, [0x01])            # VDV/VRH enable
    cmd(0xC3, [0x12])            # VRH
    cmd(0xC4, [0x20])            # VDV
    cmd(0xC6, [0x0F])            # frame rate 60Hz
    cmd(0xD0, [0xA4, 0xA1])     # power
    cmd(0xE0, [0xD0,0x04,0x0D,0x11,0x13,0x2B,0x3F,0x54,
               0x4C,0x18,0x0D,0x0B,0x1F,0x23])
    cmd(0xE1, [0xD0,0x04,0x0C,0x11,0x13,0x2C,0x3F,0x44,
               0x51,0x2F,0x1F,0x1F,0x20,0x23])
    cmd(0x3A, [0x05])            # 16-bit color

def fill(r, g, b, x_off=0, y_off=0):
    """Fill screen with color using given offsets."""
    x0, y0 = x_off, y_off
    x1, y1 = x_off + W - 1, y_off + H - 1
    cmd(0x2A, [x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF])
    cmd(0x2B, [y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF])
    cmd(0x2C)
    GPIO.output(DC, 1)
    c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    pixel = bytes([c >> 8, c & 0xFF])
    row = pixel * W
    for _ in range(H):
        spi.writebytes(list(row))

# ── Try different configs ────────────────────────────────────────────
configs = [
    # (spi_speed, madctl, inversion, y_offset, label)
    (10_000_000, 0x00, True,   0, "10MHz, MADCTL=0x00, inv=ON,  offset=0"),
    (10_000_000, 0x00, True,  40, "10MHz, MADCTL=0x00, inv=ON,  offset=40"),
    (10_000_000, 0x00, True,  80, "10MHz, MADCTL=0x00, inv=ON,  offset=80"),
    (10_000_000, 0x00, False,  0, "10MHz, MADCTL=0x00, inv=OFF, offset=0"),
    (10_000_000, 0x00, False, 80, "10MHz, MADCTL=0x00, inv=OFF, offset=80"),
    (10_000_000, 0x70, True,   0, "10MHz, MADCTL=0x70, inv=ON,  offset=0"),
    (10_000_000, 0x70, True,  40, "10MHz, MADCTL=0x70, inv=ON,  offset=40"),
    (10_000_000, 0x70, True,  80, "10MHz, MADCTL=0x70, inv=ON,  offset=80"),
    ( 4_000_000, 0x00, True,   0, " 4MHz, MADCTL=0x00, inv=ON,  offset=0"),
    ( 4_000_000, 0x00, True,  80, " 4MHz, MADCTL=0x00, inv=ON,  offset=80"),
]

print("=== TFT Diagnostic ===")
print("Will cycle through configs, 3 seconds each.")
print("Tell me which one shows RED on the screen.\n")

for i, (speed, madctl, inv, y_off, label) in enumerate(configs):
    print(f"[{i+1}/{len(configs)}] {label}")
    spi.max_speed_hz = speed
    init_display()
    cmd(0x36, [madctl])
    if inv:
        cmd(0x21)  # inversion ON
    else:
        cmd(0x20)  # inversion OFF
    cmd(0x13)      # normal display
    cmd(0x29)      # display ON
    time.sleep(0.05)
    fill(255, 0, 0, x_off=0, y_off=y_off)
    time.sleep(3)

print("\nDone. If none worked, the SPI wiring may be wrong.")
print("Check: MOSI->SDA(pin19), SCLK->SCL(pin23), CS->CE0(pin24)")
GPIO.cleanup()
