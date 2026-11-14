#include "shared_variables.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Maximum number of editable variables
#define MAX_VARIABLES 48

// Array to store editable variables
static EditableVariable variables[MAX_VARIABLES];
static int variableCount = 0;

// Register a new variable (no menu information: shown under its key)
void register_variable(const char *name, float *valuePtr, float minValue, float maxValue, float stepSize) {
    SettingInfo info = {
        .name = name, .valuePtr = valuePtr,
        .minValue = minValue, .maxValue = maxValue, .stepSize = stepSize,
        .kind = VAR_NUMBER, .decimals = 2,
    };
    register_setting(&info);
}

/* Register a setting. Its current value becomes its default, so set the
 * variable to its default before calling this. */
void register_setting(const SettingInfo *info) {
    if (variableCount >= MAX_VARIABLES) {
        printf("Maximum number of variables reached.\n");
        return;
    }
    EditableVariable *v = &variables[variableCount];
    v->name = info->name;
    v->valuePtr = info->valuePtr;
    v->minValue = info->minValue;
    v->maxValue = info->maxValue;
    v->stepSize = info->stepSize;
    v->defaultValue = *info->valuePtr;
    v->label = info->label ? info->label : info->name;
    v->section = info->section;
    v->help = info->help;
    v->unit = info->unit;
    v->zero_text = info->zero_text;
    v->choices = info->choices;
    v->kind = info->kind;
    v->decimals = info->decimals;
    pthread_mutex_init(&v->mutex, NULL);
    variableCount++;
}

EditableVariable *find_variable(const char *name) {
    for (int i = 0; i < variableCount; i++) {
        if (variables[i].name && strcmp(variables[i].name, name) == 0)
            return &variables[i];
    }
    return NULL;
}

// Get the value of a variable safely
bool get_variable_value(const char *name, float *outValue) {
    EditableVariable *v = find_variable(name);
    if (!v)
        return false; // Variable not found
    /* No lock: a float load is atomic, and this is called from
     * the realtime audio thread, which must never block on a
     * mutex held by a low-priority menu thread */
    *outValue = *(v->valuePtr);
    return true;
}

// Set the value of a variable safely
bool set_variable_value(const char *name, float newValue) {
    EditableVariable *v = find_variable(name);
    if (!v)
        return false; // Variable not found
    if (newValue < v->minValue || newValue > v->maxValue)
        return false; // Value out of bounds
    pthread_mutex_lock(&v->mutex);
    *(v->valuePtr) = newValue;
    pthread_mutex_unlock(&v->mutex);
    return true;
}

// Get the list of editable variables
EditableVariable *get_editable_variables(int *count) {
    *count = variableCount;
    return variables;
}

// Save all variable values to a config file
bool save_variables_to_file(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return false;
    fprintf(f, "# ScratchTJ saved config\n");
    fprintf(f, "# Written by the Config menu. Keys are explained in docs/SETTINGS.md\n");
    for (int i = 0; i < variableCount; i++) {
        pthread_mutex_lock(&variables[i].mutex);
        float value = *variables[i].valuePtr;
        pthread_mutex_unlock(&variables[i].mutex);
        if (variables[i].section)
            fprintf(f, "# %s > %s\n", variables[i].section, variables[i].label);
        fprintf(f, "%s=%.6f\n", variables[i].name, value);
    }
    fclose(f);
    return true;
}

// Load variable values from a config file
bool load_variables_from_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return false;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        EditableVariable *v = find_variable(line);
        if (!v) {
            printf("Config: ignoring unknown setting '%s'\n", line);
            continue;
        }
        float val = strtof(eq + 1, NULL);
        /* A value saved under an older, wider range is brought inside
         * the current one rather than thrown away */
        if (val < v->minValue) val = v->minValue;
        if (val > v->maxValue) val = v->maxValue;
        set_variable_value(line, val);
    }
    fclose(f);
    return true;
}

// Reset all variables to their defaults
void reset_all_variables_to_defaults(void) {
    for (int i = 0; i < variableCount; i++) {
        pthread_mutex_lock(&variables[i].mutex);
        *variables[i].valuePtr = variables[i].defaultValue;
        pthread_mutex_unlock(&variables[i].mutex);
    }
}

void format_variable_value(const EditableVariable *v, float value, char *buf, size_t n) {
    int iv = (int)lroundf(value);

    switch (v->kind) {
    case VAR_ONOFF:
        snprintf(buf, n, "%s", value >= 0.5f ? "On" : "Off");
        return;
    case VAR_CHOICE:
        if (v->choices && iv >= (int)v->minValue && iv <= (int)v->maxValue)
            snprintf(buf, n, "%s", v->choices[iv - (int)v->minValue]);
        else
            snprintf(buf, n, "%d", iv);
        return;
    case VAR_NUMBER:
    default:
        if (v->zero_text && fabsf(value) < 1e-6f) {
            snprintf(buf, n, "%s", v->zero_text);
            return;
        }
        if (v->unit)
            snprintf(buf, n, "%.*f %s", v->decimals, value, v->unit);
        else
            snprintf(buf, n, "%.*f", v->decimals, value);
        return;
    }
}
