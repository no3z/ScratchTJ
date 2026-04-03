#!/usr/bin/env python3
"""
Hardware test for ScratchTJ v2:
  - TFT SPI display (ST7789 240x240)
  - EC11 rotary encoder (on TFT board)
  - MT6701 magnetic encoder via PWM

Pin assignments:
  TFT:  SCL=GPIO11(SCLK), SDA=GPIO10(MOSI), CS=GPIO8(CE0), DC=GPIO22, RES=GPIO27
  EC11: A=GPIO24, B=GPIO25, PUSH=GPIO23
  MT6701 PWM: GPIO18
"""

import time
import sys
import signal
import threading
import struct
import spidev
import RPi.GPIO as GPIO
from PIL import Image, ImageDraw, ImageFont

# ── Pin definitions ──────────────────────────────────────────────────
# TFT
TFT_DC   = 24
TFT_RST  = 25
TFT_BLK  = None   # tied to 3V3
TFT_CS   = 8      # SPI CE0

# EC11 rotary encoder
ENC_A    = 23
ENC_B    = 22
ENC_BTN  = 27

# MT6701 PWM output
MT6701_PWM = 18

# ── TFT display (ST7789 240x240) ────────────────────────────────────
TFT_W, TFT_H = 240, 240

class ST7789:
    """Minimal ST7789 driver using spidev + RPi.GPIO."""

    def __init__(self, spi_bus=0, spi_dev=0, dc=TFT_DC, rst=TFT_RST,
                 width=TFT_W, height=TFT_H, speed_hz=20_000_000):
        self.dc = dc
        self.rst = rst
        self.w = width
        self.h = height

        GPIO.setup(dc, GPIO.OUT)
        if rst is not None:
            GPIO.setup(rst, GPIO.OUT)

        self.spi = spidev.SpiDev()
        self.spi.open(spi_bus, spi_dev)
        self.spi.max_speed_hz = speed_hz
        self.spi.mode = 0
        self.spi.no_cs = False

        self._init_display()

    # low-level helpers
    def _cmd(self, cmd, data=None):
        GPIO.output(self.dc, 0)
        self.spi.writebytes([cmd])
        if data:
            GPIO.output(self.dc, 1)
            self.spi.writebytes(data)

    def _reset(self):
        if self.rst is None:
            return
        GPIO.output(self.rst, 1); time.sleep(0.1)
        GPIO.output(self.rst, 0); time.sleep(0.1)
        GPIO.output(self.rst, 1); time.sleep(0.2)

    def _init_display(self):
        self._reset()
        self._cmd(0x01)          # SW reset
        time.sleep(0.2)
        self._cmd(0x11)          # sleep out
        time.sleep(0.2)

        self._cmd(0xB2, [0x0C, 0x0C, 0x00, 0x33, 0x33])  # porch setting
        self._cmd(0xB7, [0x35])  # gate control
        self._cmd(0xBB, [0x19])  # VCOM
        self._cmd(0xC0, [0x2C])  # LCM control
        self._cmd(0xC2, [0x01])  # VDV/VRH enable
        self._cmd(0xC3, [0x12])  # VRH set
        self._cmd(0xC4, [0x20])  # VDV set
        self._cmd(0xC6, [0x0F])  # frame rate: 60Hz
        self._cmd(0xD0, [0xA4, 0xA1])  # power control

        # positive gamma
        self._cmd(0xE0, [0xD0,0x04,0x0D,0x11,0x13,0x2B,0x3F,0x54,
                         0x4C,0x18,0x0D,0x0B,0x1F,0x23])
        # negative gamma
        self._cmd(0xE1, [0xD0,0x04,0x0C,0x11,0x13,0x2C,0x3F,0x44,
                         0x51,0x2F,0x1F,0x1F,0x20,0x23])

        self._cmd(0x3A, [0x05])  # 16-bit color (RGB565)
        self._cmd(0x36, [0x00])  # MADCTL - no rotation
        self._cmd(0x21)          # inversion on (ST7789 needs this)
        self._cmd(0x13)          # normal display
        self._cmd(0x29)          # display on
        time.sleep(0.1)

    def set_window(self, x0, y0, x1, y1):
        self._cmd(0x2A, [x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF])
        self._cmd(0x2B, [y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF])
        self._cmd(0x2C)

    def show_image(self, img):
        """Display a PIL Image (RGB, 240x240) on the TFT."""
        if img.size != (self.w, self.h):
            img = img.resize((self.w, self.h))
        # convert to RGB565
        pixels = img.tobytes()
        buf = bytearray(self.w * self.h * 2)
        idx = 0
        for i in range(0, len(pixels), 3):
            r, g, b = pixels[i], pixels[i+1], pixels[i+2]
            c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            buf[idx]   = c >> 8
            buf[idx+1] = c & 0xFF
            idx += 2
        self.set_window(0, 0, self.w - 1, self.h - 1)
        GPIO.output(self.dc, 1)
        # send in chunks (spidev limit)
        chunk = 4096
        for i in range(0, len(buf), chunk):
            self.spi.writebytes(buf[i:i+chunk])

    def fill(self, r, g, b):
        """Fill screen with a solid color."""
        img = Image.new('RGB', (self.w, self.h), (r, g, b))
        self.show_image(img)


# ── MT6701 PWM reader ───────────────────────────────────────────────
class MT6701Reader:
    """Reads MT6701 angle from PWM duty cycle using GPIO callbacks."""

    def __init__(self, pin=MT6701_PWM):
        self.pin = pin
        self.rise_tick = 0
        self.fall_tick = 0
        self.period = 0
        self.high_time = 0
        self.angle = 0.0
        self.duty = 0.0
        self.valid = False
        self._lock = threading.Lock()

        GPIO.setup(pin, GPIO.IN)
        GPIO.add_event_detect(pin, GPIO.BOTH, callback=self._edge_cb)

    def _edge_cb(self, channel):
        now = time.monotonic_ns()
        if GPIO.input(self.pin):
            # rising edge
            with self._lock:
                if self.rise_tick > 0:
                    self.period = now - self.rise_tick
                self.rise_tick = now
        else:
            # falling edge
            with self._lock:
                if self.rise_tick > 0:
                    self.high_time = now - self.rise_tick
                    if self.period > 0:
                        self.duty = self.high_time / self.period
                        # MT6701: duty 0..100% maps to 0..360 degrees
                        self.angle = self.duty * 360.0
                        self.valid = True

    def read(self):
        with self._lock:
            return self.angle, self.duty, self.valid


# ── EC11 encoder reader ─────────────────────────────────────────────
class EC11Reader:
    """Simple EC11 quadrature encoder reader."""

    def __init__(self, pin_a=ENC_A, pin_b=ENC_B, pin_btn=ENC_BTN):
        self.pin_a = pin_a
        self.pin_b = pin_b
        self.pin_btn = pin_btn
        self.position = 0
        self.button = False
        self._lock = threading.Lock()

        GPIO.setup(pin_a, GPIO.IN, pull_up_down=GPIO.PUD_UP)
        GPIO.setup(pin_b, GPIO.IN, pull_up_down=GPIO.PUD_UP)
        GPIO.setup(pin_btn, GPIO.IN, pull_up_down=GPIO.PUD_UP)

        self._last_a = GPIO.input(pin_a)
        GPIO.add_event_detect(pin_a, GPIO.BOTH, callback=self._enc_cb,
                              bouncetime=2)

    def _enc_cb(self, channel):
        a = GPIO.input(self.pin_a)
        b = GPIO.input(self.pin_b)
        with self._lock:
            if a != self._last_a:
                if a != b:
                    self.position += 1
                else:
                    self.position -= 1
                self._last_a = a

    def read(self):
        with self._lock:
            pos = self.position
        btn = not GPIO.input(self.pin_btn)  # active low
        return pos, btn


# ── Main test ────────────────────────────────────────────────────────
def main():
    running = True

    def sig_handler(sig, frame):
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, sig_handler)
    signal.signal(signal.SIGTERM, sig_handler)

    GPIO.setmode(GPIO.BCM)
    GPIO.setwarnings(False)

    print("=== ScratchTJ v2 Hardware Test ===")
    print()

    # ── Phase 1: TFT color test ──────────────────────────────────────
    print("[1/4] Initializing TFT (ST7789 240x240 on SPI0, DC=GPIO22, RST=GPIO27)...")
    try:
        tft = ST7789()
        print("  TFT initialized OK")
    except Exception as e:
        print(f"  TFT FAILED: {e}")
        print("  Check wiring: SCL->GPIO11, SDA->GPIO10, CS->GPIO8, DC->GPIO22, RES->GPIO27")
        GPIO.cleanup()
        sys.exit(1)

    print("[2/4] TFT color test - cycling RED, GREEN, BLUE...")
    for name, color in [("RED", (255,0,0)), ("GREEN", (0,255,0)), ("BLUE", (0,0,255))]:
        print(f"  {name}...", end=" ", flush=True)
        tft.fill(*color)
        print("OK")
        time.sleep(0.7)

    # ── Phase 2: Init sensors ────────────────────────────────────────
    print("[3/4] Initializing MT6701 PWM reader on GPIO18...")
    mt = MT6701Reader()
    print("  Waiting for PWM signal...", end=" ", flush=True)
    deadline = time.time() + 3.0
    while time.time() < deadline:
        _, _, valid = mt.read()
        if valid:
            break
        time.sleep(0.05)
    angle, duty, valid = mt.read()
    if valid:
        print(f"OK (angle={angle:.1f}°, duty={duty:.3f})")
    else:
        print("NO SIGNAL (check PWM wire to GPIO18)")

    print("[4/4] Initializing EC11 encoder (A=GPIO24, B=GPIO25, BTN=GPIO23)...")
    ec = EC11Reader()
    print("  EC11 ready")

    # ── Phase 3: Live display ────────────────────────────────────────
    print()
    print("=== LIVE TEST - rotate platter / encoder, press button ===")
    print("    Press Ctrl+C to exit")
    print()

    try:
        font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 18)
        font_big = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf", 28)
        font_title = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf", 16)
    except:
        font = ImageFont.load_default()
        font_big = font
        font_title = font

    frame = 0
    while running:
        angle, duty, mt_valid = mt.read()
        enc_pos, enc_btn = ec.read()

        # build display frame
        img = Image.new('RGB', (TFT_W, TFT_H), (0, 0, 0))
        draw = ImageDraw.Draw(img)

        # title
        draw.text((10, 5), "ScratchTJ v2 Test", fill=(0, 200, 255), font=font_title)
        draw.line([(5, 26), (235, 26)], fill=(0, 100, 128), width=1)

        # MT6701 section
        draw.text((10, 35), "MT6701 Platter", fill=(180, 180, 180), font=font_title)
        if mt_valid:
            draw.text((10, 58), f"{angle:6.1f}°", fill=(0, 255, 100), font=font_big)
            draw.text((10, 90), f"duty: {duty:.4f}", fill=(120, 120, 120), font=font)
            # draw angle arc
            cx, cy, r = 190, 75, 30
            draw.ellipse([(cx-r, cy-r), (cx+r, cy+r)], outline=(60,60,60))
            import math
            rad = math.radians(angle - 90)
            ex = cx + r * math.cos(rad)
            ey = cy + r * math.sin(rad)
            draw.line([(cx, cy), (ex, ey)], fill=(0, 255, 100), width=2)
        else:
            draw.text((10, 58), "NO SIGNAL", fill=(255, 50, 50), font=font_big)

        draw.line([(5, 115), (235, 115)], fill=(0, 100, 128), width=1)

        # EC11 section
        draw.text((10, 122), "EC11 Encoder", fill=(180, 180, 180), font=font_title)
        draw.text((10, 145), f"pos: {enc_pos:+d}", fill=(255, 255, 0), font=font_big)

        btn_color = (0, 255, 0) if enc_btn else (80, 80, 80)
        btn_text = "PRESSED" if enc_btn else "released"
        draw.text((10, 180), f"btn: {btn_text}", fill=btn_color, font=font)

        # button indicator circle
        cx, cy = 200, 165
        draw.ellipse([(cx-15, cy-15), (cx+15, cy+15)],
                     fill=btn_color, outline=(200,200,200))

        draw.line([(5, 205), (235, 205)], fill=(0, 100, 128), width=1)

        # footer
        draw.text((10, 212), f"frame {frame}", fill=(60, 60, 60), font=font_title)

        tft.show_image(img)
        frame += 1

        # also print to console periodically
        if frame % 10 == 1:
            mt_str = f"{angle:6.1f}°" if mt_valid else "NO SIGNAL"
            print(f"\r  MT6701: {mt_str}  |  EC11: pos={enc_pos:+d} btn={'Y' if enc_btn else 'N'}  |  frame {frame}  ", end="", flush=True)

        time.sleep(0.05)  # ~20 fps

    print("\n\nShutting down...")
    tft.fill(0, 0, 0)
    GPIO.cleanup()
    print("Done.")


if __name__ == "__main__":
    main()
