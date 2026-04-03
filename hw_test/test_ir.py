import RPi.GPIO as GPIO
import time

# Define GPIO pin for the IR sensor
IR_SENSOR_PIN = 17  # Replace with the GPIO pin you connected the 'OUT' pin to

# Set up GPIO
GPIO.setmode(GPIO.BCM)
GPIO.setup(IR_SENSOR_PIN, GPIO.IN)

# Variables to track spin state
last_state = GPIO.input(IR_SENSOR_PIN)
last_change_time = time.time()
spinning = False

# Define timeout for detecting stationary CD (in seconds)
SPIN_TIMEOUT = 0.5  # CD considered stationary if no changes in 0.5 seconds

def log(message):
    """Helper function to print logs with timestamps."""
    print(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] {message}")

def detect_spin():
    global last_state, last_change_time, spinning
    while True:
        # Read the current sensor state
        current_state = GPIO.input(IR_SENSOR_PIN)

        # Log the current sensor state
        log(f"IR Sensor Value: {'HIGH' if current_state else 'LOW'}")

        # Check for a change in reflection (spin activity)
        if current_state != last_state:
            last_change_time = time.time()
            spinning = True
            log("Change detected! CD is spinning!")

        # Check if the CD is stationary
        if time.time() - last_change_time > SPIN_TIMEOUT:
            if spinning:
                spinning = False
                log("No changes detected. CD is stationary.")

        # Update the last state
        last_state = current_state

        # Small delay to stabilize readings
        time.sleep(0.05)  # 50 ms for smoother logs

# Run the detection function
try:
    log("Starting CD Spin Detection...")
    detect_spin()
except KeyboardInterrupt:
    GPIO.cleanup()
    log("Exiting program and cleaning up GPIO.")
