#pragma once

#include "core.h"

typedef enum {
    Menu,
    Selected,
    Connected,
} ApplicationState;

typedef struct {
    volatile bool serverRunning;
    volatile bool clientRunning;

    bool sharing; // toggle to turn screen on or off

    int window_width, window_height;
    
    ApplicationState app_state;
} State;

extern State *gstate;

void initGstate();

