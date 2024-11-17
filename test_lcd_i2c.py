import smbus2
from time import sleep

# I2C device address (change if your LCD has a different address)
I2C_ADDR = 0x27

# LCD commands
LCD_CHR = 1    # Send data
LCD_CMD = 0    # Send command

LCD_LINE_1 = 0x80  # LCD RAM address for the 1st line
LCD_LINE_2 = 0xC0  # LCD RAM address for the 2nd line

LCD_BACKLIGHT = 0x08  # On
# LCD_BACKLIGHT = 0x00  # Off

ENABLE = 0b00000100  # Enable bit

# Timing constants
E_PULSE = 0.0005
E_DELAY = 0.0005

# Initialize the I2C bus
bus = smbus2.SMBus(1)

def lcd_byte(bits, mode):
    """Send byte to data pins via I2C."""
    high_bits = mode | (bits & 0xF0) | LCD_BACKLIGHT
    low_bits = mode | ((bits << 4) & 0xF0) | LCD_BACKLIGHT

    # Write high nibble
    bus.write_byte(I2C_ADDR, high_bits)
    lcd_toggle_enable(high_bits)

    # Write low nibble
    bus.write_byte(I2C_ADDR, low_bits)
    lcd_toggle_enable(low_bits)

def lcd_toggle_enable(bits):
    """Toggle enable pin on LCD."""
    sleep(E_DELAY)
    bus.write_byte(I2C_ADDR, (bits | ENABLE))
    sleep(E_PULSE)
    bus.write_byte(I2C_ADDR, (bits & ~ENABLE))
    sleep(E_DELAY)

def lcd_init():
    """Initialize the LCD display."""
    lcd_byte(0x33, LCD_CMD)  # Initialize
    lcd_byte(0x32, LCD_CMD)  # Set to 4-bit mode
    lcd_byte(0x06, LCD_CMD)  # Cursor move direction
    lcd_byte(0x0C, LCD_CMD)  # Turn cursor off
    lcd_byte(0x28, LCD_CMD)  # 2 line display
    lcd_byte(0x01, LCD_CMD)  # Clear display
    sleep(0.002)

def lcd_string(message, line):
    """Display a string on the LCD."""
    message = message.ljust(16, " ")  # Pad to 16 characters
    lcd_byte(line, LCD_CMD)
    for char in message:
        lcd_byte(ord(char), LCD_CHR)

try:
    lcd_init()
    lcd_string("I2C LCD Test", LCD_LINE_1)
    lcd_string("Hello, World!", LCD_LINE_2)
    sleep(5)
finally:
    lcd_byte(0x01, LCD_CMD)  # Clear display
