/* recording.h */

#ifndef RECORDING_H
#define RECORDING_H

#include <stdio.h>
#include <alsa/asoundlib.h>
#include <pthread.h>
#include "deck.h"

#define MAX_INPUT_SOURCES 10

typedef struct {
    char name[128];
    char device[128];
} InputSource;

typedef struct {
    snd_pcm_t *capture_handle;
    char device_name[128];
    FILE *output_file;
    char filename[256]; 
    pthread_t thread;
    volatile int running;
} RecordingContext;

int get_available_input_sources(InputSource *sources, int max_sources);

int start_recording(RecordingContext *context, const char *input_device, const char *filename);

void stop_recording(struct deck *d, RecordingContext *context);

/* Read peak input level from ALSA capture device.
 * Returns 0.0 - 1.0 normalized peak. Returns -1.0 on error.
 * Opens and closes the device each call (~10ms). Call at ~10Hz. */
float read_input_peak(const char *device);

#endif // RECORDING_H
