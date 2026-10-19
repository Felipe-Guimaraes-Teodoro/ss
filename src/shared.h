#pragma once

#include "core.h"

// TODO: use this
typedef enum {
    CONNECTING,
    SCREENSHARING,
} ApplicationState;

typedef struct {
    volatile bool serverRunning;
    volatile bool clientRunning;

    int window_width, window_height;
    
    // u8 padding[32];
    // u8* pixels;
} State;

extern State *gstate;

void initGstate();

