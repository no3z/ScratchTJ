#include "alsa_mixer.h"
#include <string.h>
#include <stdio.h>

// Retrieve mixer controls
int get_mixer_controls(const char *card, MixerControl *controls, int max_controls) {
    snd_mixer_t *handle;
    snd_mixer_elem_t *elem;
    int control_count = 0;

    if (snd_mixer_open(&handle, 0) < 0) {
        fprintf(stderr, "Unable to open mixer\n");
        return 0;
    }

    if (snd_mixer_attach(handle, card) < 0) {
        fprintf(stderr, "Unable to attach mixer to card %s\n", card);
        snd_mixer_close(handle);
        return 0;
    }

    if (snd_mixer_selem_register(handle, NULL, NULL) < 0) {
        fprintf(stderr, "Unable to register mixer elements\n");
        snd_mixer_close(handle);
        return 0;
    }

    if (snd_mixer_load(handle) < 0) {
        fprintf(stderr, "Unable to load mixer elements\n");
        snd_mixer_close(handle);
        return 0;
    }

    for (elem = snd_mixer_first_elem(handle); elem && control_count < max_controls; elem = snd_mixer_elem_next(elem)) {
        if (snd_mixer_selem_is_active(elem)) {
            MixerControl *control = &controls[control_count];

            memset(control, 0, sizeof(MixerControl));
            snprintf(control->name, sizeof(control->name), "%s", snd_mixer_selem_get_name(elem));

            // Initialize flags
            control->isVolume = false;
            control->isBoolean = false;
            control->isEnum = false;

            // Handle volume controls
            if (snd_mixer_selem_has_playback_volume(elem) || snd_mixer_selem_has_capture_volume(elem)) {
                control->isVolume = true;
                if (snd_mixer_selem_has_playback_volume(elem)) {
                    snd_mixer_selem_get_playback_volume_range(elem, &control->min, &control->max);
                    snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &control->current);
                } else {
                    snd_mixer_selem_get_capture_volume_range(elem, &control->min, &control->max);
                    snd_mixer_selem_get_capture_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &control->current);
                }
            }
            // Handle boolean (switch) controls
            else if (snd_mixer_selem_has_playback_switch(elem) || snd_mixer_selem_has_capture_switch(elem)) {
                control->isBoolean = true;
                int value;
                if (snd_mixer_selem_has_playback_switch(elem)) {
                    snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_FRONT_LEFT, &value);
                } else {
                    snd_mixer_selem_get_capture_switch(elem, SND_MIXER_SCHN_FRONT_LEFT, &value);
                }
                control->current = value;
            }
            // Handle enum controls
            else {
                int enum_items = snd_mixer_selem_get_enum_items(elem);
                if (enum_items > 0) {
                    control->isEnum = true;
                    control->enumItemCount = (enum_items < MAX_ENUM_ITEMS) ? enum_items : MAX_ENUM_ITEMS;
                    for (unsigned int idx = 0; idx < control->enumItemCount; idx++) {
                        snd_mixer_selem_get_enum_item_name(elem, idx, MAX_ITEM_NAME_LENGTH, control->enumItems[idx]);
                    }
                    unsigned int idx;
                    snd_mixer_selem_get_enum_item(elem, SND_MIXER_SCHN_FRONT_LEFT, &idx);
                    control->currentEnumIndex = idx;
                } else {
                    // Control type unknown
                    continue;
                }
            }

            control_count++;
        }
    }

    snd_mixer_close(handle);
    return control_count;
}

// Update a mixer control value (for volume controls)
int set_mixer_control(const char *card, const char *control_name, long value) {
    snd_mixer_t *handle;
    snd_mixer_elem_t *elem;
    int result = -1;

    if (snd_mixer_open(&handle, 0) < 0) {
        fprintf(stderr, "Unable to open mixer\n");
        return -1;
    }

    if (snd_mixer_attach(handle, card) < 0) {
        fprintf(stderr, "Unable to attach mixer\n");
        snd_mixer_close(handle);
        return -1;
    }

    if (snd_mixer_selem_register(handle, NULL, NULL) < 0) {
        fprintf(stderr, "Unable to register mixer\n");
        snd_mixer_close(handle);
        return -1;
    }

    if (snd_mixer_load(handle) < 0) {
        fprintf(stderr, "Unable to load mixer\n");
        snd_mixer_close(handle);
        return -1;
    }

    for (elem = snd_mixer_first_elem(handle); elem; elem = snd_mixer_elem_next(elem)) {
        if (strcmp(control_name, snd_mixer_selem_get_name(elem)) == 0) {
            if (snd_mixer_selem_has_playback_volume(elem)) {
                snd_mixer_selem_set_playback_volume_all(elem, value);
                result = 0;
            } else if (snd_mixer_selem_has_capture_volume(elem)) {
                snd_mixer_selem_set_capture_volume_all(elem, value);
                result = 0;
            }
            break;
        }
    }

    snd_mixer_close(handle);
    return result;
}

// Update a boolean mixer control (for pswitch controls)
int set_mixer_control_boolean(const char *card, const char *control_name, int value) {
    snd_mixer_t *handle;
    snd_mixer_elem_t *elem;
    int result = -1;

    if (snd_mixer_open(&handle, 0) < 0) {
        fprintf(stderr, "Unable to open mixer\n");
        return -1;
    }

    if (snd_mixer_attach(handle, card) < 0) {
        fprintf(stderr, "Unable to attach mixer\n");
        snd_mixer_close(handle);
        return -1;
    }

    if (snd_mixer_selem_register(handle, NULL, NULL) < 0) {
        fprintf(stderr, "Unable to register mixer\n");
        snd_mixer_close(handle);
        return -1;
    }

    if (snd_mixer_load(handle) < 0) {
        fprintf(stderr, "Unable to load mixer\n");
        snd_mixer_close(handle);
        return -1;
    }

    for (elem = snd_mixer_first_elem(handle); elem; elem = snd_mixer_elem_next(elem)) {
        if (strcmp(control_name, snd_mixer_selem_get_name(elem)) == 0) {
            if (snd_mixer_selem_has_playback_switch(elem)) {
                snd_mixer_selem_set_playback_switch_all(elem, value);
                result = 0;
            } else if (snd_mixer_selem_has_capture_switch(elem)) {
                snd_mixer_selem_set_capture_switch_all(elem, value);
                result = 0;
            }
            break;
        }
    }

    snd_mixer_close(handle);
    return result;
}

// Update an enum mixer control
int set_mixer_control_enum(const char *card, const char *control_name, int index) {
    snd_mixer_t *handle;
    snd_mixer_elem_t *elem;
    int result = -1;

    if (snd_mixer_open(&handle, 0) < 0) {
        fprintf(stderr, "Unable to open mixer\n");
        return -1;
    }

    if (snd_mixer_attach(handle, card) < 0) {
        fprintf(stderr, "Unable to attach mixer\n");
        snd_mixer_close(handle);
        return -1;
    }

    if (snd_mixer_selem_register(handle, NULL, NULL) < 0) {
        fprintf(stderr, "Unable to register mixer\n");
        snd_mixer_close(handle);
        return -1;
    }

    if (snd_mixer_load(handle) < 0) {
        fprintf(stderr, "Unable to load mixer\n");
        snd_mixer_close(handle);
        return -1;
    }

    for (elem = snd_mixer_first_elem(handle); elem; elem = snd_mixer_elem_next(elem)) {
        if (strcmp(control_name, snd_mixer_selem_get_name(elem)) == 0) {
            int enum_items = snd_mixer_selem_get_enum_items(elem);
            if (enum_items > 0) {
                if (index >= 0 && index < enum_items) {
                    snd_mixer_selem_set_enum_item(elem, SND_MIXER_SCHN_FRONT_LEFT, index);
                    result = 0;
                } else {
                    fprintf(stderr, "Invalid enum index\n");
                }
            }
            break;
        }
    }

    snd_mixer_close(handle);
    return result;
}
