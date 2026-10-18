#include "draw_screens.h"
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_render.h>

#include "net.h"
#include "shared.h"

typedef enum {
    TOP,
    BOTTOM,
    MIDDLE
} FitDirection;

SDL_FRect fit_video_to_quad(u32 peer, float px, float py, float pw, float ph, FitDirection dir) {
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

    return (SDL_FRect){
        .x = x,
        .y = y,
        .w = w,
        .h = h,
    };
}

PeerScreen screens[MAX_PEERS];

SDL_Texture *get_screen_texture(SDL_Window *window, u32 id) {
    PeerScreen *screen = &screens[id];
    PeerSession *session = &sessions[id];

    if (session->width <= 0 || session->height <= 0) return NULL;
    if (!screen->tex || screen->current_tex_w != session->width || screen->current_tex_h != session->height) {
        if (screen->tex) {
            SDL_DestroyTexture(screen->tex);
        }

        screen->tex = SDL_CreateTexture(
            gstate->renderer, 
            gstate->format, 
            SDL_TEXTUREACCESS_STREAMING, 
            session->width,
            session->height
        );

        screen->current_tex_w = session->width;
        screen->current_tex_h = session->height;
    }

    if (session->pixels != NULL) {
        SDL_UpdateTexture(screen->tex, NULL, session->pixels, session->width * 4);
    }

    return screen->tex;
}

void draw_screens(Client *client, SDL_FRect *dst_quad) {
    SDL_SetRenderDrawColor(gstate->renderer, 20, 30, 40, 255);
    SDL_RenderFillRect(gstate->renderer, dst_quad);
    SDL_SetRenderDrawColor(gstate->renderer, 200, 20, 20, 255);
    SDL_RenderDebugText(gstate->renderer, dst_quad->x, dst_quad->y, "screens quad");

    for (u32 i = 0; i < MAX_PEERS; i++) {
        if (sessions[i].active == false) continue;
        // net already does this
        // if (client->peer->incomingPeerID == sessions[i].peerId) continue; 

        SDL_Texture* tex = get_screen_texture(gstate->window, i);

        if (tex) {
            SDL_FRect quad = fit_video_to_quad(
                i, 
                dst_quad->x, dst_quad->y,
                dst_quad->w, dst_quad->h,
                MIDDLE 
            );
            SDL_RenderTexture(gstate->renderer, tex, NULL, &quad);
        }
    }
}


