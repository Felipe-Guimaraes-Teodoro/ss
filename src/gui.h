#pragma once

#include <raylib.h>

#define MAX_INPUT_LEN 128
// if you want to draw an input box just create a variable with this type, set placeholder and call draw_search_bar() with it
typedef struct {
    char text[MAX_INPUT_LEN];
    int text_length;
    bool is_focused;
    const char* placeholder;
} TextInputState;

typedef struct {
    Font fonts[1];
    TextInputState ping_input;

    bool server_btn; // true if user press button to be server
} GUIContext;

void init_gui(GUIContext& gctx);
void draw_gui(GUIContext& gctx);
void close_gui();

