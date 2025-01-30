/* recording.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <alsa/asoundlib.h>
#include <stdint.h>              // WAV HEADER ADDED
#include "recording.h"
#include "xwax.h"

#define WAV_HEADER_SIZE 44       // WAV HEADER ADDED

/* WAV HEADER ADDED - Write a placeholder WAV header at the beginning */
static void write_wav_header(FILE *file, unsigned int sample_rate, int channels, int bits_per_sample)
{
    // The "RIFF" chunk descriptor
    fwrite("RIFF", 1, 4, file);

    // Placeholder for file size (will update later in update_wav_header)
    uint32_t chunk_size = 0;  // 4 bytes
    fwrite(&chunk_size, 4, 1, file);

    // Format
    fwrite("WAVE", 1, 4, file);

    // "fmt " sub-chunk
    fwrite("fmt ", 1, 4, file);
    uint32_t subchunk1_size = 16;
    fwrite(&subchunk1_size, 4, 1, file);

    uint16_t audio_format = 1; // PCM
    fwrite(&audio_format, 2, 1, file);

    uint16_t num_channels = (uint16_t) channels;
    fwrite(&num_channels, 2, 1, file);

    uint32_t sampleRate = sample_rate;
    fwrite(&sampleRate, 4, 1, file);

    uint32_t byte_rate = sample_rate * channels * (bits_per_sample / 8);
    fwrite(&byte_rate, 4, 1, file);

    uint16_t block_align = channels * (bits_per_sample / 8);
    fwrite(&block_align, 2, 1, file);

    uint16_t bps = bits_per_sample;
    fwrite(&bps, 2, 1, file);

    // "data" sub-chunk
    fwrite("data", 1, 4, file);

    // Placeholder for subchunk2_size (will update later)
    uint32_t subchunk2_size = 0;
    fwrite(&subchunk2_size, 4, 1, file);
}

/* WAV HEADER ADDED - Update the WAV header with the correct file sizes */
static void update_wav_header(const char *filename)
{
    FILE *file = fopen(filename, "r+b");
    if (!file) {
        perror("Failed to open file for WAV header update");
        return;
    }

    // File size = total bytes - 8 (RIFF chunk descriptor)
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    if (file_size < WAV_HEADER_SIZE) {
        // File too small or error
        fclose(file);
        return;
    }

    // Chunk size (at offset 4)
    uint32_t chunk_size = (uint32_t)(file_size - 8);
    fseek(file, 4, SEEK_SET);
    fwrite(&chunk_size, 4, 1, file);

    // Subchunk2 size (at offset 40)
    uint32_t subchunk2_size = (uint32_t)(file_size - WAV_HEADER_SIZE);
    fseek(file, 40, SEEK_SET);
    fwrite(&subchunk2_size, 4, 1, file);

    fclose(file);
}

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

    // WAV HEADER ADDED: Write a placeholder WAV header before capturing PCM
    write_wav_header(context->output_file, sample_rate, 2, 16);

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

    // WAV HEADER ADDED: Update the file now that recording is done
    update_wav_header(context->filename);

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
