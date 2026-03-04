/* recording.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <alsa/asoundlib.h>

#include "recording.h"
#include "xwax.h"

/* Function to fetch available input sources from ALSA */
int get_available_input_sources(InputSource *sources, int max_sources)
{
    int err;
    int card_num = -1;
    int source_count = 0;

    // Initialize card search
    if (snd_card_next(&card_num) < 0 || card_num < 0)
    {
        printf("No sound cards found.\n");
        return 0;
    }

    while (card_num >= 0 && source_count < max_sources)
    {
        snd_ctl_t *card_handle;
        char card_name[32];

        sprintf(card_name, "hw:%d", card_num);
        if ((err = snd_ctl_open(&card_handle, card_name, 0)) < 0)
        {
            printf("Cannot open control for card %d: %s\n", card_num, snd_strerror(err));
            goto next_card;
        }

        int device_num = -1;
        while (1)
        {
            snd_pcm_info_t *pcm_info;
            snd_pcm_info_alloca(&pcm_info);

            if (snd_ctl_pcm_next_device(card_handle, &device_num) < 0)
            {
                printf("Cannot get next device number\n");
                break;
            }

            if (device_num < 0)
                break;

            snd_pcm_info_set_device(pcm_info, device_num);
            snd_pcm_info_set_subdevice(pcm_info, 0);
            snd_pcm_info_set_stream(pcm_info, SND_PCM_STREAM_CAPTURE);

            if ((err = snd_ctl_pcm_info(card_handle, pcm_info)) < 0)
            {
                // No such device or it's not a capture device
                continue;
            }

            // Get device name
            const char *device_name = snd_pcm_info_get_name(pcm_info);

            // Build the device identifier string
            char device_identifier[128];
            sprintf(device_identifier, "hw:%d,%d", card_num, device_num);

            // Save the input source info
            strncpy(sources[source_count].name, device_name, sizeof(sources[source_count].name) - 1);
            sources[source_count].name[sizeof(sources[source_count].name) - 1] = '\0';
            strncpy(sources[source_count].device, device_identifier, sizeof(sources[source_count].device) - 1);
            sources[source_count].device[sizeof(sources[source_count].device) - 1] = '\0';
            source_count++;

            if (source_count >= max_sources)
                break;
        }

        snd_ctl_close(card_handle);

    next_card:
        if (snd_card_next(&card_num) < 0)
            break;
    }

    return source_count;
}

/* --- Audio passthrough (monitor input while browsing sources) --- */

static PassthroughContext passthrough_ctx;

static int passthrough_configure_pcm(snd_pcm_t *pcm) {
    snd_pcm_hw_params_t *hw;
    snd_pcm_hw_params_alloca(&hw);
    snd_pcm_hw_params_any(pcm, hw);
    snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_S16_LE);
    unsigned int rate = 48000;
    snd_pcm_hw_params_set_rate(pcm, hw, rate, 0);
    snd_pcm_hw_params_set_channels(pcm, hw, 2);
    return snd_pcm_hw_params(pcm, hw);
}

static void *passthrough_thread_func(void *arg) {
    PassthroughContext *ctx = (PassthroughContext *)arg;
    int16_t buffer[512 * 2]; /* 512 frames, stereo */

    while (ctx->running) {
        int r = snd_pcm_readi(ctx->capture_handle, buffer, 512);
        if (r == -EPIPE) {
            snd_pcm_prepare(ctx->capture_handle);
            continue;
        } else if (r < 0) {
            break;
        }

        int w = snd_pcm_writei(ctx->playback_handle, buffer, r);
        if (w == -EPIPE) {
            snd_pcm_prepare(ctx->playback_handle);
            snd_pcm_writei(ctx->playback_handle, buffer, r);
        }
    }
    return NULL;
}

int start_passthrough(const char *input_device) {
    stop_passthrough();

    int err;
    memset(&passthrough_ctx, 0, sizeof(passthrough_ctx));

    err = snd_pcm_open(&passthrough_ctx.capture_handle, input_device,
                        SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        fprintf(stderr, "Passthrough: cannot open capture %s: %s\n",
                input_device, snd_strerror(err));
        return -1;
    }

    if (passthrough_configure_pcm(passthrough_ctx.capture_handle) < 0) {
        fprintf(stderr, "Passthrough: cannot configure capture\n");
        snd_pcm_close(passthrough_ctx.capture_handle);
        passthrough_ctx.capture_handle = NULL;
        return -1;
    }

    err = snd_pcm_open(&passthrough_ctx.playback_handle, "default",
                        SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        fprintf(stderr, "Passthrough: cannot open playback: %s\n", snd_strerror(err));
        snd_pcm_close(passthrough_ctx.capture_handle);
        passthrough_ctx.capture_handle = NULL;
        return -1;
    }

    if (passthrough_configure_pcm(passthrough_ctx.playback_handle) < 0) {
        fprintf(stderr, "Passthrough: cannot configure playback\n");
        snd_pcm_close(passthrough_ctx.capture_handle);
        snd_pcm_close(passthrough_ctx.playback_handle);
        passthrough_ctx.capture_handle = NULL;
        passthrough_ctx.playback_handle = NULL;
        return -1;
    }

    passthrough_ctx.running = 1;
    if (pthread_create(&passthrough_ctx.thread, NULL,
                       passthrough_thread_func, &passthrough_ctx) != 0) {
        passthrough_ctx.running = 0;
        snd_pcm_close(passthrough_ctx.capture_handle);
        snd_pcm_close(passthrough_ctx.playback_handle);
        passthrough_ctx.capture_handle = NULL;
        passthrough_ctx.playback_handle = NULL;
        return -1;
    }

    printf("Passthrough started on %s\n", input_device);
    return 0;
}

void stop_passthrough(void) {
    if (!passthrough_ctx.running) return;
    passthrough_ctx.running = 0;
    pthread_join(passthrough_ctx.thread, NULL);
    if (passthrough_ctx.capture_handle) {
        snd_pcm_close(passthrough_ctx.capture_handle);
        passthrough_ctx.capture_handle = NULL;
    }
    if (passthrough_ctx.playback_handle) {
        snd_pcm_close(passthrough_ctx.playback_handle);
        passthrough_ctx.playback_handle = NULL;
    }
    printf("Passthrough stopped\n");
}

/* Recording thread function */
void *recording_thread_func(void *arg)
{
    RecordingContext *context = (RecordingContext *)arg;
    snd_pcm_t *handle = context->capture_handle;
    FILE *output_file = context->output_file;
    int err;
    snd_pcm_uframes_t frames = scsettings.buffersize;
    int16_t buffer[scsettings.buffersize * 2]; // Assuming stereo

    while (context->running)
    {
        err = snd_pcm_readi(handle, buffer, frames);
        if (err == -EPIPE)
        {
            // Overrun occurred
            fprintf(stderr, "Overrun occurred\n");
            snd_pcm_prepare(handle);
        }
        else if (err < 0)
        {
            fprintf(stderr, "Error from read: %s\n", snd_strerror(err));
        }
        else if (err != (int)frames)
        {
            fprintf(stderr, "Short read, read %d frames\n", err);
        }

        if (err > 0)
        {
            // Write data to file
            fwrite(buffer, sizeof(int16_t), err * 2, output_file); // 2 channels
        }
    }

    return NULL;
}

/* Function to start recording */
int start_recording(RecordingContext *context, const char *input_device, const char *filename)
{
    int err;
    snd_pcm_hw_params_t *hw_params;

    strncpy(context->device_name, input_device, sizeof(context->device_name) - 1);
    context->device_name[sizeof(context->device_name) - 1] = '\0';

    // Open PCM device for recording (capture).
    if ((err = snd_pcm_open(&context->capture_handle, context->device_name,
                            SND_PCM_STREAM_CAPTURE, 0)) < 0)
    {
        fprintf(stderr, "Cannot open input audio device %s (%s)\n",
                context->device_name, snd_strerror(err));
        return -1;
    }

    // Allocate hardware parameters object.
    snd_pcm_hw_params_alloca(&hw_params);

    // Fill it in with default values.
    snd_pcm_hw_params_any(context->capture_handle, hw_params);

    // Set parameters.
    if ((err = snd_pcm_hw_params_set_access(context->capture_handle, hw_params,
                                            SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
    {
        fprintf(stderr, "Error setting access (%s)\n", snd_strerror(err));
        snd_pcm_close(context->capture_handle);
        return -1;
    }

    if ((err = snd_pcm_hw_params_set_format(context->capture_handle, hw_params,
                                            SND_PCM_FORMAT_S16_LE)) < 0)
    {
        fprintf(stderr, "Error setting format (%s)\n", snd_strerror(err));
        snd_pcm_close(context->capture_handle);
        return -1;
    }

    unsigned int sample_rate = 48000;
    if ((err = snd_pcm_hw_params_set_rate(context->capture_handle, hw_params,
                                          sample_rate, 0)) < 0)
    {
        fprintf(stderr, "Error setting rate (%s)\n", snd_strerror(err));
        snd_pcm_close(context->capture_handle);
        return -1;
    }

    if ((err = snd_pcm_hw_params_set_channels(context->capture_handle, hw_params, 2)) < 0)
    {
        fprintf(stderr, "Error setting channels (%s)\n", snd_strerror(err));
        snd_pcm_close(context->capture_handle);
        return -1;
    }

    // Write the parameters to the driver.
    if ((err = snd_pcm_hw_params(context->capture_handle, hw_params)) < 0)
    {
        fprintf(stderr, "Error setting HW params (%s)\n", snd_strerror(err));
        snd_pcm_close(context->capture_handle);
        return -1;
    }
    strncpy(context->filename, filename, sizeof(context->filename) - 1);
    context->filename[sizeof(context->filename) - 1] = '\0';

    // Open the output file.
    context->output_file = fopen(filename, "wb");
    if (!context->output_file)
    {
        perror("Failed to open output file");
        snd_pcm_close(context->capture_handle);
        return -1;
    }

    // Set running flag.
    context->running = 1;

    // Start the recording thread.
    if (pthread_create(&context->thread, NULL, recording_thread_func, context) != 0)
    {
        perror("Failed to create recording thread");
        context->running = 0;
        fclose(context->output_file);
        snd_pcm_close(context->capture_handle);
        return -1;
    }

    return 0;
}

void stop_recording(struct deck *d, RecordingContext *context)
{
    // Set running flag to 0.
    context->running = 0;

    // Wait for thread to finish.
    pthread_join(context->thread, NULL);

    // Close the capture device.
    if (context->capture_handle)
    {
        snd_pcm_close(context->capture_handle);
        context->capture_handle = NULL;
    }

    // Close the output file.
    if (context->output_file)
    {
        fclose(context->output_file);
        context->output_file = NULL;
    }
    struct player *pl = &d->player;

    char *allocated_filename = strdup(context->filename);  // Duplicate the filename
    if (!allocated_filename) {
        fprintf(stderr, "Failed to allocate memory for filename\n");
        return;
    }

    printf("Recording saved to: %s\n", allocated_filename);
    struct track *new_track;
    // Acquire a new track
    new_track = track_acquire_by_import(d->importer, allocated_filename);
    if (new_track == NULL)
    {
        fprintf(stderr, "Failed to load track: %s\n", allocated_filename);
        free(allocated_filename);
        return;
    }

    // Set the new track for the player
    load_track(d, new_track);

    pl->target_position = 0;
    pl->position = 0;
    pl->offset = 0;

    // Clear the filename
    memset(context->filename, 0, sizeof(context->filename));

    // Reset running flag
    context->running = 0;
}