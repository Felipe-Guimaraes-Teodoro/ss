#pragma once

#include "core.h"
#include "SDL3/SDL.h"

// TODO: use this
typedef enum {
    CONNECTING,
    SCREENSHARING,
} ApplicationState;

typedef struct {
    volatile bool serverRunning;
    volatile bool clientRunning;
    SDL_Window* window;
    SDL_Renderer* renderer;

    int window_width, window_height;
    
    SDL_PixelFormat format;

    // u8 padding[32];
    // u8* pixels;
} State;

extern State *gstate;

void initGstate();

