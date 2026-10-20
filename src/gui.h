#pragma once

#include <raylib.h>

typedef struct {
    Font fonts[1];
} GUIContext;

void init_gui(GUIContext& gctx);
void draw_gui(GUIContext& gctx);
void close_gui();

