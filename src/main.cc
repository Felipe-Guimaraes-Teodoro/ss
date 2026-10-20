#include <cstdlib>
#include <raylib.h>

#include <cstdio>
#include <enet/enet.h>
#include <obs/obs-avc.h>
#include <pthread.h>

#include "draw_screens.h"
#include "net.h"
#include "shared.h"
#include "gui.h"

int main() {
    initGstate();

    GUIContext gctx;

    SetTraceLogLevel(LOG_WARNING);
    init_gui(gctx);
    
    Client *client = NULL;
    ENetAddress addr;

    SetTargetFPS(60);

    while (!WindowShouldClose()){
        BeginDrawing();
        gstate->window_width = GetScreenWidth();
        gstate->window_height = GetScreenHeight();

        // TODO: for the love of god remove these two from main
        if (gstate->app_state == Selected){
            if (gctx.ping_input.text_length == 0) {
                Server *server = createServer(PORT);
                addr = server->addr;
                pthread_t serverThread;
                gstate->serverRunning = true;
                pthread_create(&serverThread, NULL, serverThreadFn, server);
                pthread_detach(serverThread);

                enet_address_set_host(&addr, "127.0.0.1");
            } else {
                enet_address_set_host(&addr, gctx.ping_input.text);
            }

            addr.port = PORT; // (favali software foundation, rule 4.2)
            client = createClient(addr);
            pthread_t clientThread;
            gstate->clientRunning = true; 
            pthread_create(&clientThread, NULL, clientThreadFn, client);
            pthread_detach(clientThread);

            gstate->app_state = Connected; // very important or else you'll
                                           // be creating endless servers
        }

        ClearBackground({13, 20, 30});

        if (gstate->app_state == Connected){
            Rectangle dst_quad = {
                .x = 0,
                .y = 0,
                .width = (float)gstate->window_width,
                .height = (float)gstate->window_height 
            }; 

            draw_screens(client, &dst_quad);
        }

        draw_gui(gctx);

        EndDrawing();
    }

    close_gui();
    enet_deinitialize();
    
    return 0;
}

