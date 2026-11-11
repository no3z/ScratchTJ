#ifndef ALSA_MIXER_H
#define ALSA_MIXER_H

#include <stdbool.h>
#include <alsa/asoundlib.h>

#define MAX_ENUM_ITEMS 32
#define MAX_ITEM_NAME_LENGTH 64

/* The ALSA mixer the sound card's controls live on. "default" follows
 * ctl.!default in /etc/asound.conf, which points at the audio HAT. */
#define MIXER_CARD "default"

/* Where the mixer settings made in the menu are kept between runs */
#define MIXER_STATE_PATH "/home/no3z/.scratchtj/mixer.cfg"

/* One adjustable thing on a mixer element. An element that has, say,
 * both a capture volume and a capture switch yields two controls. */
typedef enum {
    MIXER_PLAYBACK_VOLUME,
    MIXER_CAPTURE_VOLUME,
    MIXER_PLAYBACK_SWITCH,
    MIXER_CAPTURE_SWITCH,
    MIXER_ENUM
} MixerKind;

typedef struct {
    char name[64];         // Label shown in the menu
    char elem[64];         // ALSA simple element name
    unsigned int index;    // ALSA simple element index
    MixerKind kind;
    long min;              // Minimum value for volume controls
    long max;              // Maximum value for volume controls
    long current;          // Current value for volume controls or switch (0 or 1)
    bool isVolume;         // True if control is a volume
    bool isBoolean;        // True if control is a switch
    bool isEnum;           // True if control is an enum
    int enumItemCount;     // Number of items for enum controls
    char enumItems[MAX_ENUM_ITEMS][MAX_ITEM_NAME_LENGTH]; // Names of enum items
    int currentEnumIndex;  // Current index for enum controls
} MixerControl;

/* Read every control of the card with its current value.
 * Return: the number of controls stored */
int get_mixer_controls(const char *card, MixerControl *controls, int max_controls);

/* Write control->current (or currentEnumIndex) to the sound card.
 * Return: 0 on success, -1 if the control could not be set */
int apply_mixer_control(const char *card, const MixerControl *control);

/* Save all current mixer values to a file / apply a saved file.
 * Return: 0 on success, -1 on failure (including no saved file) */
int mixer_save(const char *card, const char *path);
int mixer_restore(const char *card, const char *path);

#endif // ALSA_MIXER_H
