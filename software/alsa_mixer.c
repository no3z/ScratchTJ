#include "alsa_mixer.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Open the simple mixer of a card. Return: NULL on failure */
static snd_mixer_t *mixer_open(const char *card)
{
    snd_mixer_t *handle;
    int r;

    if ((r = snd_mixer_open(&handle, 0)) < 0) {
        fprintf(stderr, "Mixer: unable to open: %s\n", snd_strerror(r));
        return NULL;
    }

    if ((r = snd_mixer_attach(handle, card)) < 0)
        fprintf(stderr, "Mixer: unable to attach to card %s: %s\n", card, snd_strerror(r));
    else if ((r = snd_mixer_selem_register(handle, NULL, NULL)) < 0)
        fprintf(stderr, "Mixer: unable to register elements: %s\n", snd_strerror(r));
    else if ((r = snd_mixer_load(handle)) < 0)
        fprintf(stderr, "Mixer: unable to load elements: %s\n", snd_strerror(r));

    if (r < 0) {
        snd_mixer_close(handle);
        return NULL;
    }

    return handle;
}

static snd_mixer_elem_t *find_elem(snd_mixer_t *handle, const char *name, unsigned int index)
{
    snd_mixer_selem_id_t *sid;

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_name(sid, name);
    snd_mixer_selem_id_set_index(sid, index);

    return snd_mixer_find_selem(handle, sid);
}

/* Read the current value of one control from its element */
static void read_value(snd_mixer_elem_t *elem, MixerControl *c)
{
    int sw = 0;
    unsigned int item = 0;

    switch (c->kind) {
    case MIXER_PLAYBACK_VOLUME:
        snd_mixer_selem_get_playback_volume_range(elem, &c->min, &c->max);
        snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &c->current);
        break;
    case MIXER_CAPTURE_VOLUME:
        snd_mixer_selem_get_capture_volume_range(elem, &c->min, &c->max);
        snd_mixer_selem_get_capture_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &c->current);
        break;
    case MIXER_PLAYBACK_SWITCH:
        snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_FRONT_LEFT, &sw);
        c->current = sw;
        break;
    case MIXER_CAPTURE_SWITCH:
        snd_mixer_selem_get_capture_switch(elem, SND_MIXER_SCHN_FRONT_LEFT, &sw);
        c->current = sw;
        break;
    case MIXER_ENUM:
        snd_mixer_selem_get_enum_item(elem, SND_MIXER_SCHN_FRONT_LEFT, &item);
        c->currentEnumIndex = item;
        c->current = item;
        break;
    }
}

/* Write the value of one control to its element */
static int write_value(snd_mixer_elem_t *elem, const MixerControl *c)
{
    switch (c->kind) {
    case MIXER_PLAYBACK_VOLUME:
        return snd_mixer_selem_set_playback_volume_all(elem, c->current);
    case MIXER_CAPTURE_VOLUME:
        return snd_mixer_selem_set_capture_volume_all(elem, c->current);
    case MIXER_PLAYBACK_SWITCH:
        return snd_mixer_selem_set_playback_switch_all(elem, c->current ? 1 : 0);
    case MIXER_CAPTURE_SWITCH:
        return snd_mixer_selem_set_capture_switch_all(elem, c->current ? 1 : 0);
    case MIXER_ENUM: {
        int ch, r = -1;

        /* Set every channel the enum has, not just the left one */
        for (ch = 0; ch <= SND_MIXER_SCHN_LAST; ch++) {
            if (snd_mixer_selem_set_enum_item(elem, ch, c->currentEnumIndex) >= 0)
                r = 0;
        }
        return r;
    }
    }
    return -1;
}

/* Append one control of the given kind for elem, if there is room */
static void add_control(snd_mixer_elem_t *elem, MixerKind kind, const char *suffix,
                        MixerControl *controls, int max_controls, int *count)
{
    MixerControl *c;

    if (*count >= max_controls)
        return;

    c = &controls[*count];
    memset(c, 0, sizeof(*c));

    snprintf(c->elem, sizeof(c->elem), "%s", snd_mixer_selem_get_name(elem));
    c->index = snd_mixer_selem_get_index(elem);
    snprintf(c->name, sizeof(c->name), "%.50s%s", c->elem, suffix);
    c->kind = kind;

    c->isVolume = (kind == MIXER_PLAYBACK_VOLUME || kind == MIXER_CAPTURE_VOLUME);
    c->isBoolean = (kind == MIXER_PLAYBACK_SWITCH || kind == MIXER_CAPTURE_SWITCH);
    c->isEnum = (kind == MIXER_ENUM);

    if (c->isEnum) {
        int items = snd_mixer_selem_get_enum_items(elem);

        if (items <= 0)
            return;
        c->enumItemCount = (items < MAX_ENUM_ITEMS) ? items : MAX_ENUM_ITEMS;
        for (int i = 0; i < c->enumItemCount; i++)
            snd_mixer_selem_get_enum_item_name(elem, i, MAX_ITEM_NAME_LENGTH, c->enumItems[i]);
    }

    read_value(elem, c);
    if (c->isEnum && c->currentEnumIndex >= c->enumItemCount)
        c->currentEnumIndex = 0;

    (*count)++;
}

// Retrieve mixer controls
int get_mixer_controls(const char *card, MixerControl *controls, int max_controls) {
    snd_mixer_t *handle;
    snd_mixer_elem_t *elem;
    int count = 0;

    handle = mixer_open(card);
    if (!handle)
        return 0;

    for (elem = snd_mixer_first_elem(handle); elem; elem = snd_mixer_elem_next(elem)) {
        bool pvol, cvol, psw, csw;
        int kinds;

        if (!snd_mixer_selem_is_active(elem))
            continue;

        if (snd_mixer_selem_is_enumerated(elem)) {
            add_control(elem, MIXER_ENUM, "", controls, max_controls, &count);
            continue;
        }

        pvol = snd_mixer_selem_has_playback_volume(elem);
        psw = snd_mixer_selem_has_playback_switch(elem);

        /* A "common" volume or switch drives playback and capture
         * together; listing it once is enough */
        cvol = snd_mixer_selem_has_capture_volume(elem) && !snd_mixer_selem_has_common_volume(elem);
        csw = snd_mixer_selem_has_capture_switch(elem) && !snd_mixer_selem_has_common_switch(elem);

        /* Only tell the entries apart in the menu when an element
         * really has more than one */
        kinds = pvol + cvol + psw + csw;

        if (pvol)
            add_control(elem, MIXER_PLAYBACK_VOLUME, kinds > 1 ? " Vol" : "", controls, max_controls, &count);
        if (cvol)
            add_control(elem, MIXER_CAPTURE_VOLUME, kinds > 1 ? " Cap Vol" : "", controls, max_controls, &count);
        if (psw)
            add_control(elem, MIXER_PLAYBACK_SWITCH, kinds > 1 ? " Sw" : "", controls, max_controls, &count);
        if (csw)
            add_control(elem, MIXER_CAPTURE_SWITCH, kinds > 1 ? " Cap Sw" : "", controls, max_controls, &count);
    }

    snd_mixer_close(handle);
    return count;
}

// Write one control's value to the sound card
int apply_mixer_control(const char *card, const MixerControl *control) {
    snd_mixer_t *handle;
    snd_mixer_elem_t *elem;
    int result = -1;

    handle = mixer_open(card);
    if (!handle)
        return -1;

    elem = find_elem(handle, control->elem, control->index);
    if (!elem)
        fprintf(stderr, "Mixer: no control '%s'\n", control->elem);
    else if (write_value(elem, control) < 0)
        fprintf(stderr, "Mixer: could not set '%s'\n", control->name);
    else
        result = 0;

    snd_mixer_close(handle);
    return result;
}

#define MAX_SAVED_CONTROLS 64

/* Saved file: one control per line, tab separated:
 *     element name <TAB> element index <TAB> kind <TAB> value */

int mixer_save(const char *card, const char *path) {
    static MixerControl controls[MAX_SAVED_CONTROLS];
    int count, i;
    FILE *f;

    count = get_mixer_controls(card, controls, MAX_SAVED_CONTROLS);
    if (count == 0)
        return -1;

    f = fopen(path, "w");
    if (!f) {
        perror(path);
        return -1;
    }

    fprintf(f, "# ScratchTJ sound card mixer settings\n");
    for (i = 0; i < count; i++) {
        long value = controls[i].isEnum ? controls[i].currentEnumIndex : controls[i].current;

        fprintf(f, "%s\t%u\t%d\t%ld\n", controls[i].elem, controls[i].index,
                (int)controls[i].kind, value);
    }

    fclose(f);
    return 0;
}

int mixer_restore(const char *card, const char *path) {
    snd_mixer_t *handle;
    char line[256];
    int applied = 0;
    FILE *f;

    f = fopen(path, "r");
    if (!f)
        return -1; /* nothing saved yet: leave the card as it is */

    handle = mixer_open(card);
    if (!handle) {
        fclose(f);
        return -1;
    }

    while (fgets(line, sizeof(line), f)) {
        MixerControl c;
        snd_mixer_elem_t *elem;
        char *name, *index, *kind, *value, *save;

        if (line[0] == '#' || line[0] == '\n')
            continue;

        name = strtok_r(line, "\t", &save);
        index = strtok_r(NULL, "\t", &save);
        kind = strtok_r(NULL, "\t", &save);
        value = strtok_r(NULL, "\t\n", &save);
        if (!name || !index || !kind || !value)
            continue;

        memset(&c, 0, sizeof(c));
        c.kind = (MixerKind)atoi(kind);
        if (c.kind < MIXER_PLAYBACK_VOLUME || c.kind > MIXER_ENUM)
            continue;
        c.current = atol(value);
        c.currentEnumIndex = (int)c.current;

        /* The card may have changed since the file was written */
        elem = find_elem(handle, name, (unsigned int)atoi(index));
        if (!elem)
            continue;

        if (write_value(elem, &c) >= 0)
            applied++;
    }

    snd_mixer_close(handle);
    fclose(f);

    printf("Mixer: restored %d settings from %s\n", applied, path);
    return 0;
}
