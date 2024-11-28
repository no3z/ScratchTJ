#include "shared_variables.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Maximum number of editable variables
#define MAX_VARIABLES 10

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
    pthread_mutex_init(&variables[variableCount].mutex, NULL);
    variableCount++;
}

// Get the value of a variable safely
bool get_variable_value(const char *name, float *outValue) {
    for (int i = 0; i < variableCount; i++) {
        if (strcmp(variables[i].name, name) == 0) {
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
        if (strcmp(variables[i].name, name) == 0) {
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
