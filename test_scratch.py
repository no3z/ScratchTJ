import sounddevice as sd
import numpy as np
import wave
import time
from gpiozero import RotaryEncoder
from threading import Thread
import logging

# Configure logging for debugging
logging.basicConfig(level=logging.DEBUG, format='%(asctime)s - %(levelname)s - %(message)s')

# Initialize the rotary encoder
encoder = RotaryEncoder(17, 27)  # Adjust pins as needed for your setup
current_position = 0             # Actual playback position
loop_enabled = True              # Set to loop the audio

# Load the audio file
filename = "scratch.wav"  # Ensure the file is in WAV format for easy playback
logging.info("Loading audio file: %s", filename)

with wave.open(filename, 'rb') as wave_file:
    sample_rate = wave_file.getframerate()
    n_channels = wave_file.getnchannels()
    sample_width = wave_file.getsampwidth()
    logging.debug("Sample rate: %d, Channels: %d, Sample width: %d bytes", sample_rate, n_channels, sample_width)
    
    # Ensure the audio is in 16-bit format for compatibility
    if sample_width != 2:
        raise ValueError("Audio file must be 16-bit PCM format")

    audio_data = np.frombuffer(wave_file.readframes(wave_file.getnframes()), dtype=np.int16)

# Adjust for stereo or mono
if n_channels == 2:
    audio_data = audio_data.reshape(-1, 2)
logging.info("Audio data loaded, total samples: %d", len(audio_data))

# Set buffer size for playback
buffer_size = 1024
logging.debug("Buffer size set to: %d frames", buffer_size)

# Real-time audio callback function
def audio_callback(outdata, frames, time, status):
    global current_position, audio_data

    if status:
        logging.warning("Playback status message: %s", status)

    try:
        # Calculate the effective playback position
        indices = (current_position + np.arange(frames)) % len(audio_data)
        indices = indices.astype(int)

        # Prepare output data based on effective playback position
        if len(indices) < frames:
            output_data = np.zeros((frames, n_channels), dtype=np.int16)
            output_data[:len(indices)] = audio_data[indices]
        else:
            output_data = audio_data[indices[:frames]]

        outdata[:frames] = output_data
        logging.debug("Current position: %d", current_position)

        # Update current position for continuous playback
        current_position = (current_position + frames) % len(audio_data)

    except Exception as e:
        logging.error("Error in audio_callback: %s", e)
        current_position = 0  # Reset position on error

# Monitor rotary encoder and adjust current playback position
def update_playback_position():
    global current_position
    while True:
        delta = encoder.steps
        if delta != 0:
            # Update current position directly based on encoder movement
            step_size = 100000  # Adjust sensitivity of scratching
            current_position = (current_position + delta * step_size) % len(audio_data)
            logging.info("Current position adjusted by encoder: %d", current_position)
            
            # Reset encoder steps after reading
            encoder.steps = 0
        time.sleep(0.05)

# Start audio playback stream
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
    Thread(target=update_playback_position, daemon=True).start()
    
    logging.info("Playing scratch effect. Adjust playback position with rotary encoder. Press Ctrl+C to stop.")
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    logging.info("Stopping playback.")
finally:
    stream.stop()
    stream.close()
    logging.info("Audio stream closed.")
