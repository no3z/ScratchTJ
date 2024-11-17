import RPi.GPIO as GPIO
from time import sleep

# LCD pin setup (using BCM numbering)
LCD_RS = 17  # Register Select
LCD_E = 27   # Enable
LCD_D4 = 25
LCD_D5 = 22
LCD_D6 = 16
LCD_D7 = 24

LCD_WIDTH = 16      # Maximum characters per line
LCD_CHR = True      # Character mode
LCD_CMD = False     # Command mode

# Timing constants
E_PULSE = 0.0005
E_DELAY = 0.0005

# Setup GPIO
GPIO.setmode(GPIO.BCM)
GPIO.setup(LCD_E, GPIO.OUT)
GPIO.setup(LCD_RS, GPIO.OUT)
GPIO.setup(LCD_D4, GPIO.OUT)
GPIO.setup(LCD_D5, GPIO.OUT)
GPIO.setup(LCD_D6, GPIO.OUT)
GPIO.setup(LCD_D7, GPIO.OUT)

def lcd_byte(bits, mode):
    GPIO.output(LCD_RS, mode)
    
    # High nibble
    GPIO.output(LCD_D4, bits & 0x10 == 0x10)
    GPIO.output(LCD_D5, bits & 0x20 == 0x20)
    GPIO.output(LCD_D6, bits & 0x40 == 0x40)
    GPIO.output(LCD_D7, bits & 0x80 == 0x80)
    lcd_toggle_enable()
    
    # Low nibble
    GPIO.output(LCD_D4, bits & 0x01 == 0x01)
    GPIO.output(LCD_D5, bits & 0x02 == 0x02)
    GPIO.output(LCD_D6, bits & 0x04 == 0x04)
    GPIO.output(LCD_D7, bits & 0x08 == 0x08)
    lcd_toggle_enable()

def lcd_toggle_enable():
    sleep(E_DELAY)
    GPIO.output(LCD_E, True)
    sleep(E_PULSE)
    GPIO.output(LCD_E, False)
    sleep(E_DELAY)

def lcd_init():
    # LCD Initialization sequence
    lcd_byte(0x33, LCD_CMD)  # Initialize
    lcd_byte(0x32, LCD_CMD)  # Set to 4-bit mode
    lcd_byte(0x28, LCD_CMD)  # 2 line, 5x7 matrix
    lcd_byte(0x0C, LCD_CMD)  # Display on, Cursor off, Blink off
    lcd_byte(0x06, LCD_CMD)  # Increment cursor
    lcd_byte(0x01, LCD_CMD)  # Clear display (additional delay needed here)
    sleep(0.003)             # Clear display needs a longer delay

def lcd_string(message):
    message = message.ljust(LCD_WIDTH, " ")  # Pad to ensure 16 characters
    for i in range(LCD_WIDTH):
        lcd_byte(ord(message[i]), LCD_CHR)

try:
    lcd_init()
    lcd_string("Hello, World!")
    sleep(3)
finally:
    GPIO.cleanup()
