#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <cstdio>
#include <enet/enet.h>
#include <obs/obs-avc.h>
#include <pthread.h>

#include "draw_screens.h"
#include "net.h"
#include "shared.h"

void *serverThreadFn(void* arg) {
    Server *server = (Server*) arg;
    runServer(server, &gstate->serverRunning);
    return NULL;
}

void *clientThreadFn(void *arg) {
    Client *client = (Client*) arg;
    runClient(client, &gstate->clientRunning);
    return NULL;
}



int main() {
    printf("Welcome to SS!\n");
    initGstate();
    SDL_Init(SDL_INIT_VIDEO);
    
    SDL_CreateWindowAndRenderer("ss", gstate->window_width, gstate->window_height, SDL_WINDOW_RESIZABLE, &gstate->window, &gstate->renderer); 
    SDL_GetWindowSize(gstate->window, &gstate->window_width, &gstate->window_height);

    int is_server;
    Client *client = NULL;
    printf("is server? Enter 1 or 0\n");
    scanf("%d", &is_server);
    ENetAddress addr;
    if (is_server) {
        Server *server = createServer(7777);
        addr = server->addr;
        pthread_t serverThread;
        gstate->serverRunning = true;
        pthread_create(&serverThread, NULL, serverThreadFn, server);
        pthread_detach(serverThread);

        enet_address_set_host(&addr, "127.0.0.1");
        addr.port = 7777;
        client = createClient(addr);
        pthread_t clientThread;
        gstate->clientRunning = true; 
        pthread_create(&clientThread, NULL, clientThreadFn, client);
        pthread_detach(clientThread);
    } else {
        char buf[512];
        printf("enter server address: (ipv4)\n");
        scanf("%s", buf);

        enet_address_set_host(&addr, buf);
        addr.port = 7777; // (favali software foundation, rule 4.2)
        client = createClient(addr);
        pthread_t clientThread;
        gstate->clientRunning = true; 
        pthread_create(&clientThread, NULL, clientThreadFn, client);
        pthread_detach(clientThread);
    }

    SDL_Event event;
    bool done = false;

    while (!done) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                done = true;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    done = true;
                }
            }
        }
        
        SDL_GetWindowSize(gstate->window, &gstate->window_width, &gstate->window_height);
        SDL_SetRenderDrawColor(gstate->renderer, 13, 20, 30, 255); 
        SDL_RenderClear(gstate->renderer);

        SDL_FRect dst_quad = {
            .x = 0,
            .y = 0,
            .w = (float)gstate->window_width,
            .h = (float)gstate->window_height 
        }; 
        draw_screens(client, &dst_quad); 

        SDL_RenderPresent(gstate->renderer);
        SDL_Delay(TICK_MS);
    }

    SDL_Quit();
    enet_deinitialize();
    
    return 0;
}

