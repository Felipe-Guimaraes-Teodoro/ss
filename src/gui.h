#pragma once

#include <raylib.h>
#include <string>
#include <vector>

#define MAX_INPUT_LEN 128
// if you want to draw an input box just create a variable with this type, set placeholder and call draw_search_bar() with it
typedef struct {
    char text[MAX_INPUT_LEN];
    int text_length;
    
    bool is_focused;
    bool pressed_enter;

    const char* placeholder;
} TextInputState;

typedef struct {
    Font fonts[1];
    TextInputState ping_input;

    bool server_btn; // true if user press button to be server
    
    std::vector<std::string> history_lines;
} GUIContext;

void init_gui(GUIContext& gctx);
void draw_gui(GUIContext& gctx);
void load_history(GUIContext& gctx);
void add_history(GUIContext& gctx);
void close_gui();

