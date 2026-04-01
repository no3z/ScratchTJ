#include "shared_variables.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Maximum number of editable variables
#define MAX_VARIABLES 20

// Array to store editable variables
static EditableVariable variables[MAX_VARIABLES];
static int variableCount = 0;

// Register a new variable
void register_variable(const char *name, float *valuePtr, float minValue, float maxValue, float stepSize) {
    if (variableCount >= MAX_VARIABLES) {
        printf("Maximum number of variables reached.\n");
        return;
    }
    variables[variableCount].name = name;
    variables[variableCount].valuePtr = valuePtr;
    variables[variableCount].minValue = minValue;
    variables[variableCount].maxValue = maxValue;
    variables[variableCount].stepSize = stepSize;
    variables[variableCount].defaultValue = *valuePtr;
    pthread_mutex_init(&variables[variableCount].mutex, NULL);
    variableCount++;
}

// Get the value of a variable safely
bool get_variable_value(const char *name, float *outValue) {
    for (int i = 0; i < variableCount; i++) {
        if (variables[i].name && strcmp(variables[i].name, name) == 0) {
            pthread_mutex_lock(&variables[i].mutex);
            *outValue = *(variables[i].valuePtr);
            pthread_mutex_unlock(&variables[i].mutex);
            return true;
        }
    }
    return false; // Variable not found
}

// Set the value of a variable safely
bool set_variable_value(const char *name, float newValue) {
    for (int i = 0; i < variableCount; i++) {
        if (variables[i].name && strcmp(variables[i].name, name) == 0) {
            if (newValue < variables[i].minValue || newValue > variables[i].maxValue) {
                return false; // Value out of bounds
            }
            pthread_mutex_lock(&variables[i].mutex);
            *(variables[i].valuePtr) = newValue;
            pthread_mutex_unlock(&variables[i].mutex);
            return true;
        }
    }
    return false; // Variable not found
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
    for (int i = 0; i < variableCount; i++) {
        pthread_mutex_lock(&variables[i].mutex);
        fprintf(f, "%s=%.6f\n", variables[i].name, *variables[i].valuePtr);
        pthread_mutex_unlock(&variables[i].mutex);
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
        float val = strtof(eq + 1, NULL);
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
