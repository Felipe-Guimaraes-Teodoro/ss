#include "gui.h"
#include <raylib.h>

#define CLAY_IMPLEMENTATION
#include "../external/clay/clay.h"
#include "../external/clay/renderers/raylib/clay_renderer_raylib.c"

#include "shared.h"

TextInputState create_text_input(const char* placeholder){
    TextInputState ts;

    ts.text_length = 0;

    ts.is_focused = false;
    ts.pressed_enter = false;
    ts.placeholder = placeholder;

    return ts;
}

void init_gui(GUIContext &gctx){
    uint64_t totalMemorySize  = Clay_MinMemorySize();
    Clay_Arena clayMemory = Clay_CreateArenaWithCapacityAndMemory(
        totalMemorySize, malloc(totalMemorySize)
    );

    Clay_Initialize(clayMemory, (Clay_Dimensions) { (float)GetScreenWidth(), (float)GetScreenHeight() }, {0});
    Clay_Raylib_Initialize(gstate->window_width, gstate->window_height, "Screen Sharing", FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);

    gctx.fonts[0] = { LoadFontEx("fonts/sora.medium.ttf", 48, 0, 400) };
    SetTextureFilter(gctx.fonts[0].texture, TEXTURE_FILTER_BILINEAR);
    Clay_SetMeasureTextFunction(Raylib_MeasureText, gctx.fonts);
    
    gctx.ping_input = create_text_input("Ping to connect");

    gstate->app_state = MENU;
}

// TODO: add QOL stuff (selection, shortcuts, etc)
void UpdateTextInput(TextInputState &input) {
    if (!input.is_focused) return;

    int key = GetCharPressed();
    while (key > 0) {
        if ((key >= 32) && (key <= 125) && (input.text_length < MAX_INPUT_LEN - 1)) {
            input.text[input.text_length] = (char)key;
            input.text[input.text_length + 1] = '\0';
            input.text_length++;
        }
        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE) && input.text_length > 0) {
        input.text_length--;
        input.text[input.text_length] = '\0';
    }

    if (IsKeyPressed(KEY_ENTER)) {
        input.pressed_enter = true;
    }
}

// do some white labeling
#define WHITE_GUI (Clay_Color) {255, 255, 255, 255}
#define BTN_COL (Clay_Color){ 45, 130, 75, 255 }
#define BTN_HOVER (Clay_Color){ 60, 160, 90, 255 }

const Clay_BorderElementConfig HIGHLIGHT_BORDER = {
    .color = { 255, 255, 255, 40 },
    .width = { .left = 3, .right = 0, .top = 3, .bottom = 0 }
};

// helper for rendering input boxes
void draw_input_bar(TextInputState &input) {
    Clay_ElementId inputId = Clay_GetElementId(CLAY_STRING("InputField"));
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        input.is_focused = Clay_PointerOver(inputId);
    }

    UpdateTextInput(input);

    CLAY(
        CLAY_ID("InputBarContainer"),
        {
            .layout = {
                .sizing = { .width = CLAY_SIZING_FIXED(400), .height = CLAY_SIZING_FIXED(48) },
                .padding = { 6, 6 },
                .childGap = 8,
                .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
            },
            .backgroundColor = { 25, 30, 40, 240 },
            .cornerRadius = CLAY_CORNER_RADIUS(12)
        }
    ) {
        CLAY(
            CLAY_ID("InputField"),
            {
                .layout = {
                    .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
                    .padding = { 12, 12 },
                    .childAlignment = { .y = CLAY_ALIGN_Y_CENTER }
                },
                .backgroundColor = input.is_focused ? (Clay_Color){ 35, 42, 58, 255 } : (Clay_Color){ 20, 24, 33, 255 },
                .cornerRadius = CLAY_CORNER_RADIUS(8),
                .border = {
                    .color = input.is_focused ? (Clay_Color){ 80, 140, 240, 255 } : (Clay_Color){ 50, 60, 80, 255 },
                    .width = { 2, 2, 2, 2 }
                }
            }
        ) {
            if (input.text_length > 0) {
                CLAY_TEXT(
                    Clay_String((Clay_String){ .length = input.text_length, .chars = input.text }),
                    CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 255 }, .fontSize = 18 })
                );
            } else {
                CLAY_TEXT(
                    Clay_String({ .length = (int)strlen(input.placeholder), .chars = input.placeholder }),
                    CLAY_TEXT_CONFIG({ .textColor = { 100, 110, 130, 255 }, .fontSize = 18 })
                );
            }
        }

        Clay_ElementId btnId = Clay_GetElementId(CLAY_STRING("SubmitBtn"));
        bool btnHovered = Clay_PointerOver(btnId);
        bool btnClicked = btnHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

        CLAY(
            CLAY_ID("SubmitBtn"),
            {
                .layout = {
                    .sizing = { .width = CLAY_SIZING_FIXED(80), .height = CLAY_SIZING_GROW(0) },
                    .padding = { 8, 8 },
                    .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                },
                .backgroundColor = btnHovered ? BTN_HOVER : BTN_COL,
                .cornerRadius = CLAY_CORNER_RADIUS(8),
                .border = HIGHLIGHT_BORDER,
            }
        ) {
            CLAY_TEXT(
                CLAY_STRING("OK"),
                CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 255 }, .fontSize = 16 })
            );
        }

        if (btnClicked || (input.is_focused && IsKeyPressed(KEY_ENTER))) {
            input.is_focused = false;
            input.pressed_enter = true;

            gstate->app_state = SELECTED;
        }
    }
}

void draw_gui(GUIContext &gctx){
    Clay_SetLayoutDimensions((Clay_Dimensions){ (float)GetScreenWidth(), (float)GetScreenHeight() });

    Vector2 mousePos = GetMousePosition();
    Clay_SetPointerState(
        (Clay_Vector2){ mousePos.x, mousePos.y },
        IsMouseButtonDown(MOUSE_BUTTON_LEFT)
    );

    Clay_BeginLayout();
    if (gstate->app_state == MENU) {
        CLAY(
            CLAY_ID("Root"),
            {
                .layout = {
                    .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
                    .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
                    .layoutDirection = CLAY_TOP_TO_BOTTOM
                }
            }
        ){
            CLAY(
                CLAY_ID("Title"),
                {
                    .layout = {
                        .padding = { .top = 48 }
                    }
                }
            ) {
                CLAY_TEXT(
                    CLAY_STRING("Welcome to Screen Share"),
                    CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 255 }, .fontSize = 32 })
                );
            };

            CLAY(
                CLAY_ID("Input"),
                {
                    .layout {
                        .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
                        .childGap = 16,
                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    }
                }
            ) {
                draw_input_bar(gctx.ping_input);

                if (gctx.ping_input.pressed_enter){
                    gctx.ping_input.pressed_enter = false;
                    gstate->app_state = SELECTED;
                }

                CLAY_TEXT(
                    CLAY_STRING("or"),
                    CLAY_TEXT_CONFIG({ .textColor = WHITE_GUI, .fontSize = 20 })
                );

                Clay_ElementId btnId = Clay_GetElementId(CLAY_STRING("ServerBtn"));
                bool btnHovered = Clay_PointerOver(btnId);
                bool btnClicked = btnHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
                CLAY(
                    CLAY_ID("ServerBtn"),
                    {
                        .layout = {
                            .sizing = { .width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIXED(48) },
                            .padding = { 8, 8 },
                            .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER }
                        },
                        .backgroundColor = btnHovered ? BTN_HOVER : BTN_COL,
                        .cornerRadius = CLAY_CORNER_RADIUS(8),
                        .border = HIGHLIGHT_BORDER,
                    },
                ){
                    CLAY_TEXT(
                        CLAY_STRING("Create the Server"),
                        CLAY_TEXT_CONFIG({ .textColor = WHITE_GUI, .fontSize = 20 })
                    );

                    if (btnClicked) {
                        gctx.ping_input.text[0] = '\0';
                        gctx.ping_input.text_length = 0;

                        gstate->app_state = SELECTED;
                    }
                }
            }
        }
    }else if (gstate->app_state == CONNECTED){
        Clay_ElementId btnId = Clay_GetElementId(CLAY_STRING("DisconnectBtn"));
        bool btnHovered = Clay_PointerOver(btnId);
        bool btnClicked = btnHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

        Clay_ElementId share_btnId = Clay_GetElementId(CLAY_STRING("ShareBtn"));
        bool share_btnHovered = Clay_PointerOver(share_btnId);
        bool share_btnClicked = btnHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

        CLAY(
            CLAY_ID("Root"),
            {
                .layout = {
                    .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
                    .padding = {16, 16, 16, 32},
                    .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_BOTTOM },
                    .layoutDirection = CLAY_LEFT_TO_RIGHT
                }
            }
        ){
            CLAY(
                CLAY_ID("DisconnectBtn"),
                {
                    .layout = {
                        .sizing = { .width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIXED(48) },
                        .padding = { 8, 8 },
                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER }
                    },
                    .backgroundColor = btnHovered ? (Clay_Color){255, 25, 25, 255} : (Clay_Color){220, 10, 10, 255},
                    .cornerRadius = CLAY_CORNER_RADIUS(8),
                    .border = HIGHLIGHT_BORDER,
                },
            ){
                CLAY_TEXT(
                    CLAY_STRING("Quit"),
                    CLAY_TEXT_CONFIG({ .textColor = WHITE_GUI, .fontSize = 20 })
                );

                if (btnClicked) {
                    gstate->app_state = DISCONNECTING;
                }
            };
            CLAY(
                CLAY_ID("ShareBtn"),
                {
                    .layout = {
                        .sizing = { .width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIXED(48) },
                        .padding = { 8, 8 },
                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER }
                    },
                    .backgroundColor = share_btnHovered ? (Clay_Color){25, 25, 255, 255} : (Clay_Color){10, 10, 220, 255},
                    .cornerRadius = CLAY_CORNER_RADIUS(8),
                    .border = HIGHLIGHT_BORDER,
                }
            ) {
                CLAY_TEXT(
                    gstate->sharing ? CLAY_STRING("Stop Sharing") : CLAY_STRING("Resume Sharing"),
                    CLAY_TEXT_CONFIG({ .textColor = WHITE_GUI, .fontSize = 20 })
                );

                if (share_btnClicked) {
                    gstate->sharing = !gstate->sharing;
                }
            }
        }
    }
    
    Clay_RenderCommandArray renderCommands = Clay_EndLayout(GetFrameTime());

    Clay_Raylib_Render(renderCommands, gctx.fonts);
}

void close_gui(){
    Clay_Raylib_Close();
}

