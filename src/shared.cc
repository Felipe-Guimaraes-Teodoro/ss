#include "shared.h"
#include <cstdlib>

State* gstate = NULL;

void initGstate() {
    gstate = (State*)malloc(sizeof(State));
    gstate->window_width = 1200;
    gstate->window_height = 900;
}

