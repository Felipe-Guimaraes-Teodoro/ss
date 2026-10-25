#include <raylib.h>

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
    
    SetTargetFPS(60);

    while (!WindowShouldClose()){
        gstate->window_width = GetScreenWidth();
        gstate->window_height = GetScreenHeight();

        if (gstate->app_state == SELECTED){
            if (gctx.ping_input.text_length == 0) {
                initServer(gctx);
                initClient("localhost");
                gstate->app_state = CONNECTED;
            } else {
                initClient(gctx.ping_input.text);
                add_history(gctx);
                gstate->app_state = CONNECTED;
            }
        }

        if (gstate->app_state == DISCONNECTING) {
            if (gstate->clientRunning) {
                gstate->clientRunning = false;
            }
            if (gstate->serverRunning) {
                gstate->serverRunning = false;
            }
            gstate->app_state = MENU;
        }

        BeginDrawing();
        ClearBackground({13, 20, 30});

        if (gstate->app_state == CONNECTED){
            Rectangle dst_quad = {
                .x = 0,
                .y = 0,
                .width = (float)gstate->window_width,
                .height = (float)gstate->window_height 
            }; 

            draw_screens(gstate->client, &dst_quad);
        }


        draw_gui(gctx);

        EndDrawing();
    }

    close_gui();
    enet_deinitialize();
    
    return 0;
}

