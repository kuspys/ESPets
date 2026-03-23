#ifndef COMMON_H
#define COMMON_H

#include <Arduino.h>

// Structure to send data
// Must match both sender and receiver
typedef struct struct_message {
    unsigned int id;
    char text[32];
    float value;
    bool status;
} struct_message;

#endif
