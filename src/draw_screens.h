#pragma once
#include <SDL3/SDL_video.h>

#include "core.h"
#include "net.h"

void draw_screens(Client *client, SDL_FRect *dst_quad); 

typedef struct {
    SDL_Texture *tex;
    int current_tex_w;
    int current_tex_h;
    bool initialized;
} PeerScreen;

extern PeerScreen screens[MAX_PEERS];

