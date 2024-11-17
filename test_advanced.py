import sounddevice as sd
import numpy as np
import wave
import time
from gpiozero import RotaryEncoder
from threading import Thread
import logging

# Configure logging
logging.basicConfig(level=logging.DEBUG, format='%(asctime)s - %(levelname)s - %(message)s')

# Set up the rotary encoder
encoder = RotaryEncoder(17, 27)
current_speed = 1.0  # Playback speed
current_position = 0  # Current playback position
direction = 1  # 1 for forward, -1 for reverse
max_speed = 3.0
min_speed = 0.1

# Load audio data
filename = "scratch.wav"  # Ensure the file is in WAV format
logging.info("Loading audio file: %s", filename)
with wave.open(filename, 'rb') as wave_file:
    sample_rate = wave_file.getframerate()
    n_channels = wave_file.getnchannels()
    sample_width = wave_file.getsampwidth()
    logging.debug("Sample rate: %d, Channels: %d, Sample width: %d bytes", sample_rate, n_channels, sample_width)
    
    # Ensure audio is 16-bit for compatibility
    if sample_width != 2:
        raise ValueError("Audio file must be 16-bit PCM format")

    audio_data = np.frombuffer(wave_file.readframes(wave_file.getnframes()), dtype=np.int16)

# Adjust for stereo or mono
if n_channels == 2:
    audio_data = audio_data.reshape(-1, 2)
logging.info("Audio data loaded, total samples: %d", len(audio_data))

# Buffer size for playback
buffer_size = 1024
logging.debug("Buffer size set to: %d frames", buffer_size)

# Real-time audio callback
def audio_callback(outdata, frames, time, status):
    global current_position, current_speed, direction, audio_data

    if status:
        logging.warning("Playback status message: %s", status)

    try:
        # Calculate indices for playback, accounting for speed and direction
        indices = (current_position + direction * np.arange(frames * current_speed)).astype(int)
        indices = np.clip(indices, 0, len(audio_data) - 1)  # Ensure indices within bounds
        
        # Ensure the data fits the expected output shape
        if len(indices) < frames:
            # Pad with silence if indices are too short
            output_data = np.zeros((frames, n_channels), dtype=np.int16)
            output_data[:len(indices)] = audio_data[indices]
        else:
            # Trim to match frames if indices are too long
            output_data = audio_data[indices[:frames]]

        outdata[:] = output_data
        logging.debug("Current position: %d, Frames: %d, Speed: %.2f, Direction: %s",
                      current_position, frames, current_speed, "Forward" if direction == 1 else "Reverse")

        # Update current position
        current_position += int(frames * current_speed * direction)
        if current_position >= len(audio_data) or current_position < 0:
            logging.info("End of audio data reached. Looping back to start.")
            current_position = 0  # Loop back to start if out of bounds

    except Exception as e:
        logging.error("Error in audio_callback: %s", e)
        current_position = 0  # Reset position in case of error

# Function to monitor encoder and adjust speed/direction
def update_scratch_speed():
    global current_speed, direction
    while True:
        delta = encoder.steps
        if delta != 0:
            # Adjust speed within a safe range
            current_speed += delta * 0.1
            current_speed = max(min_speed, min(max_speed, current_speed))
            
            # Update direction based on encoder direction
            direction = 1 if delta > 0 else -1
            logging.info("Scratch speed adjusted: %.2fx, Direction: %s",
                         current_speed, "Forward" if direction == 1 else "Reverse")
        
        # Reset encoder steps
        encoder.steps = 0
        time.sleep(0.05)

# Start audio stream
logging.info("Starting audio playback stream")
stream = sd.OutputStream(
    samplerate=sample_rate,
    channels=n_channels,
    callback=audio_callback,
    blocksize=buffer_size,
    dtype='int16'
)

# Start threads for playback and encoder monitoring
try:
    stream.start()
    logging.info("Audio stream started successfully")
    Thread(target=update_scratch_speed, daemon=True).start()
    
    logging.info("Playing scratch effect. Adjust speed and direction with rotary encoder. Press Ctrl+C to stop.")
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    logging.info("Stopping playback.")
finally:
    stream.stop()
    stream.close()
    logging.info("Audio stream closed.")
