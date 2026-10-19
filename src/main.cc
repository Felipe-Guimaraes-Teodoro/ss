#include <raylib.h>

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
    initGstate();

    int is_server;
    Client *client = NULL;

    printf("Welcome to SS!\n");
    printf("is server? Enter 1 or 0\n");
    scanf("%d", &is_server);

    SetTraceLogLevel(LOG_WARNING);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1920, 1080, "Screen Sharing");

    ENetAddress addr;
    if (is_server) {
        Server *server = createServer(PORT);
        addr = server->addr;
        pthread_t serverThread;
        gstate->serverRunning = true;
        pthread_create(&serverThread, NULL, serverThreadFn, server);
        pthread_detach(serverThread);

        enet_address_set_host(&addr, "127.0.0.1");
        addr.port = PORT;
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
        addr.port = PORT; // (favali software foundation, rule 4.2)
        client = createClient(addr);
        pthread_t clientThread;
        gstate->clientRunning = true; 
        pthread_create(&clientThread, NULL, clientThreadFn, client);
        pthread_detach(clientThread);
    }



    SetTargetFPS(60);

    while (!WindowShouldClose()){

        BeginDrawing();
            gstate->window_width = GetScreenWidth();
            gstate->window_height = GetScreenHeight();

            ClearBackground({13, 20, 30});

            Rectangle dst_quad = {
                .x = 0,
                .y = 0,
                .width = (float)gstate->window_width,
                .height = (float)gstate->window_height 
            }; 
            
            draw_screens(client, &dst_quad);
        EndDrawing();
    }

    enet_deinitialize();
    
    return 0;
}

