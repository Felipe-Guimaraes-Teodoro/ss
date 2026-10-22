#pragma once

#include "core.h"
#include "net.h"

typedef enum {
    MENU,
    SELECTED,
    CONNECTED,
} ApplicationState;

typedef struct {
    volatile bool serverRunning;
    volatile bool clientRunning;

    bool sharing; // toggle to turn screen on or off

    int window_width, window_height;
    
    ApplicationState app_state;

    Server *server;
    Client *client;
} State;

extern State *gstate;

void initGstate();

