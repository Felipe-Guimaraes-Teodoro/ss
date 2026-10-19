#include <raylib.h>

#include "draw_screens.h"
#include "../external/clay/clay.h"

#include "net.h"
#include "shared.h"

typedef enum {
    TOP,
    BOTTOM,
    MIDDLE
} FitDirection;

Rectangle fit_video_to_quad(u32 peer, float px, float py, float pw, float ph, FitDirection dir) {
    float ar = (float)sessions[peer].height / (float)sessions[peer].width;
    float w, h, x, y;

    if (pw * ar > ph) {
        // not enough height: fit to quad height, compress width, and center horizontally
        h = ph;
        w = h / ar;
        x = px + (pw - w) * 0.5f;
        y = py;
    } else {
        // enough height: fit to quad width and center vertically
        w = pw;
        h = w * ar;
        x = px;

        float mul = 0.5f;
        if (dir == TOP)
            mul = 0.0;
        if (dir == BOTTOM)
            mul = 1.0;

        y = py + (ph - h) * mul;
    }

    return (Rectangle){
        .x = x,
        .y = y,
        .width = w,
        .height = h,
    };
}

PeerScreen screens[MAX_PEERS];

Texture2D get_screen_texture(u32 id) {
    PeerScreen *screen = &screens[id];
    PeerSession *session = &sessions[id];

    if (session->width <= 0 || session->height <= 0) return {0};

    if(screen->tex.id == 0 || screen->current_tex_w != session->width || screen->current_tex_h != session->height){
        if (screen->tex.id != 0) {
            UnloadTexture(screen->tex);
        }

        Image img = GenImageColor(session->width, session->height, BLANK);
        screen->tex = LoadTextureFromImage(img);
        UnloadImage(img);

        screen->current_tex_w = session->width;
        screen->current_tex_h = session->height;
    }

    if (session->pixels != NULL) {
        UpdateTexture(screen->tex, session->pixels);
    }

    return screen->tex;
}

void draw_screens(Client *client, Rectangle *dst_quad) {
    for (u32 i = 0; i < MAX_PEERS; i++) {
        if (sessions[i].active == false) continue;
        // net already does this
        // if (client->peer->incomingPeerID == sessions[i].peerId) continue; 

        Texture2D tex = get_screen_texture(i);

        if (tex.id != 0) {
            Rectangle src, dst;
            src.x = 0;
            src.y = 0;
            src.width = sessions[i].width;
            src.height = sessions[i].height;

            dst = fit_video_to_quad(i, dst_quad->x, dst_quad->y, dst_quad->width, dst_quad->height, MIDDLE);
            DrawTexturePro(
                tex,
                src,
                dst,
                {0., 0.},
                0,
                WHITE
            );
        }
    }
}

