#pragma once
#include <raylib.h>

#include "core.h"
#include "net.h"

void draw_screens(Client *client, Rectangle *dst_quad);

typedef struct {
    Texture2D tex;
    int current_tex_w;
    int current_tex_h;
    bool initialized;
} PeerScreen;

extern PeerScreen screens[MAX_PEERS];

