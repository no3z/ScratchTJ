#ifndef SHARED_VARIABLES_H
#define SHARED_VARIABLES_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

/* How a setting is shown and edited in the menu */
typedef enum {
    VAR_NUMBER = 0,   /* plain number, shown with `decimals` places and `unit` */
    VAR_ONOFF,        /* 0 = Off, 1 = On */
    VAR_CHOICE        /* 0..n-1, shown as choices[value] */
} VarKind;

// Structure to represent an editable variable
typedef struct {
    const char *name;                // Config file key (never change: saved configs use it)
    float *valuePtr;                 // Pointer to the variable's value
    float minValue;                  // Minimum allowable value
    float maxValue;                  // Maximum allowable value
    float stepSize;                  // Step size for adjustments
    float defaultValue;              // Default value for reset
    pthread_mutex_t mutex;           // Mutex for thread-safe access

    /* Presentation (menu) */
    const char *label;               // Short name shown in the menu
    const char *section;             // Menu section it belongs to
    const char *help;                // Plain-English explanation
    const char *unit;                // Unit after the number ("ms", "min"), or NULL
    const char *zero_text;           // Shown instead of 0, e.g. "Never", or NULL
    const char *const *choices;      // Names for VAR_CHOICE
    VarKind kind;
    int decimals;                    // Decimal places for VAR_NUMBER
} EditableVariable;

/* Full description of a setting, for register_setting() */
typedef struct {
    const char *name;
    float *valuePtr;
    float minValue, maxValue, stepSize;
    VarKind kind;
    int decimals;
    const char *unit;
    const char *zero_text;
    const char *const *choices;
    const char *section;
    const char *label;
    const char *help;
} SettingInfo;

// Function declarations
void register_variable(const char *name, float *valuePtr, float minValue, float maxValue, float stepSize);
void register_setting(const SettingInfo *info);
bool get_variable_value(const char *name, float *outValue);
bool set_variable_value(const char *name, float newValue);
EditableVariable *get_editable_variables(int *count);
EditableVariable *find_variable(const char *name);
bool save_variables_to_file(const char *path);
bool load_variables_from_file(const char *path);
void reset_all_variables_to_defaults(void);

/* Text for a value as the menu shows it ("On", "12 min", "0.10") */
void format_variable_value(const EditableVariable *v, float value, char *buf, size_t n);

#endif // SHARED_VARIABLES_H
