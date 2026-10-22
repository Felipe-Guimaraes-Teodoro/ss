#include "shared.h"
#include <cstdlib>
#include <raylib.h>

State* gstate = NULL;

void initGstate() {
    gstate = (State*)malloc(sizeof(State));
    gstate->window_width = 1920;
    gstate->window_height = 1080;
    gstate->app_state = MENU;
    gstate->sharing = true;
}

