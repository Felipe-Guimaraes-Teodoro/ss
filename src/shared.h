#pragma once

#include "core.h"

typedef enum {
    CONNECTING,
    SCREENSHARING,
} ApplicationState;

typedef struct {
    volatile bool serverRunning;
    volatile bool clientRunning;

    int window_width, window_height;
    
    ApplicationState app_state;

    // u8 padding[32];
    // u8* pixels;
} State;

extern State *gstate;

void initGstate();

