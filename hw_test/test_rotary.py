from gpiozero import RotaryEncoder, Button
import time

# Initialize rotary encoder on GPIO pins 5 and 6
encoder = RotaryEncoder(25,24, max_steps=0)  # max_steps=0 allows unlimited steps

# Initialize button on GPIO pin 4
button = Button(23)

try:
    while True:
        print(f"Steps: {encoder.steps}")
        
        # Reset encoder steps after each print
        encoder.steps = 0

        # Check button status
        if button.is_pressed:
            print("Button on GPIO 4 is pressed")

        time.sleep(0.1)

except KeyboardInterrupt:
    print("Program terminated")
