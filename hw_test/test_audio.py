import pygame
import time

# Initialize pygame mixer with specific audio device
pygame.mixer.init()  # Use this if you need to specify the Audio Injector
pygame.mixer.music.load("/home/no3z/Code/beats/1/scratch.mp3")

# Play the file in a loop
pygame.mixer.music.play(loops=-1)  # -1 makes it loop indefinitely

print("Playing scratch.mp3 on Audio Injector in loop. Press Ctrl+C to stop.")

# Keep the script running to let the audio play
try:
    while pygame.mixer.music.get_busy():
        time.sleep(1)  # Sleep to keep CPU usage low
except KeyboardInterrupt:
    print("Stopping playback.")
    pygame.mixer.music.stop()