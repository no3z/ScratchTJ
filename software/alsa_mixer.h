#ifndef ALSA_MIXER_H
#define ALSA_MIXER_H

#include <stdbool.h>
#include <alsa/asoundlib.h>

#define MAX_ENUM_ITEMS 32
#define MAX_ITEM_NAME_LENGTH 64

typedef struct {
    char name[64];         // Name of the control
    long min;              // Minimum value for volume controls
    long max;              // Maximum value for volume controls
    long current;          // Current value for volume controls or pswitch (0 or 1)
    bool isVolume;         // True if control is a volume
    bool isBoolean;        // True if control is a pswitch
    bool isEnum;           // True if control is an enum
    int enumItemCount;     // Number of items for enum controls
    char enumItems[MAX_ENUM_ITEMS][MAX_ITEM_NAME_LENGTH]; // Names of enum items
    int currentEnumIndex;  // Current index for enum controls
} MixerControl;

int get_mixer_controls(const char *card, MixerControl *controls, int max_controls);
int set_mixer_control(const char *card, const char *control_name, long value);
int set_mixer_control_boolean(const char *card, const char *control_name, int value);
int set_mixer_control_enum(const char *card, const char *control_name, int index);

#endif // ALSA_MIXER_H
