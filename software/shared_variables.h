#ifndef SHARED_VARIABLES_H
#define SHARED_VARIABLES_H

#include <pthread.h>
#include <stdbool.h>

// Structure to represent an editable variable
typedef struct {
    const char *name;                // Name of the variable
    float *valuePtr;                 // Pointer to the variable's value
    float minValue;                  // Minimum allowable value
    float maxValue;                  // Maximum allowable value
    float stepSize;                  // Step size for adjustments
    pthread_mutex_t mutex;           // Mutex for thread-safe access
} EditableVariable;

// Function declarations
void register_variable(const char *name, float *valuePtr, float minValue, float maxValue, float stepSize);
bool get_variable_value(const char *name, float *outValue);
bool set_variable_value(const char *name, float newValue);
EditableVariable *get_editable_variables(int *count);

#endif // SHARED_VARIABLES_H
