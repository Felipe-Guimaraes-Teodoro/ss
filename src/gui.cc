#include "gui.h"

#define CLAY_IMPLEMENTATION
#include "../external/clay/clay.h"
#include "../external/clay/renderers/raylib/clay_renderer_raylib.c"

#include "shared.h"

void init_gui(GUIContext &gctx){
    uint64_t totalMemorySize  = Clay_MinMemorySize();
    Clay_Arena clayMemory = Clay_CreateArenaWithCapacityAndMemory(
        totalMemorySize, malloc(totalMemorySize)
    );

    Clay_Initialize(clayMemory, (Clay_Dimensions) { (float)GetScreenWidth(), (float)GetScreenHeight() }, {0});
    Clay_Raylib_Initialize(gstate->window_width, gstate->window_height, "Screen Sharing", FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);

    gctx.fonts[0] = { LoadFontEx("fonts/sora.medium.ttf", 32, 0, 400) };
    SetTextureFilter(gctx.fonts[0].texture, TEXTURE_FILTER_BILINEAR);
    Clay_SetMeasureTextFunction(Raylib_MeasureText, gctx.fonts);
}

#define MAX_INPUT_LEN 128

// if you want to draw an input box just create a variable with this type, set placeholder and call draw_search_bar() with it
typedef struct {
    char text[MAX_INPUT_LEN];
    int textLength;
    bool isFocused;
    const char* placeholder;
} TextInputState;

void UpdateTextInput(TextInputState &input) {
    if (!input.isFocused) return;

    int key = GetCharPressed();
    while (key > 0) {
        if ((key >= 32) && (key <= 125) && (input.textLength < MAX_INPUT_LEN - 1)) {
            input.text[input.textLength] = (char)key;
            input.text[input.textLength + 1] = '\0';
            input.textLength++;
        }
        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE) && input.textLength > 0) {
        input.textLength--;
        input.text[input.textLength] = '\0';
    }
}

// helper for rendering input boxes
void draw_search_bar(TextInputState &input) {
    Clay_ElementId inputId = Clay_GetElementId(CLAY_STRING("InputField"));
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        input.isFocused = Clay_PointerOver(inputId);
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
                .backgroundColor = input.isFocused ? (Clay_Color){ 35, 42, 58, 255 } : (Clay_Color){ 20, 24, 33, 255 },
                .cornerRadius = CLAY_CORNER_RADIUS(8),
                .border = {
                    .color = input.isFocused ? (Clay_Color){ 80, 140, 240, 255 } : (Clay_Color){ 50, 60, 80, 255 },
                    .width = { 2, 2, 2, 2 }
                }
            }
        ) {
            if (input.textLength > 0) {
                CLAY_TEXT(
                    Clay_String((Clay_String){ .length = input.textLength, .chars = input.text }),
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
                    .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER }
                },
                .backgroundColor = btnHovered ? (Clay_Color){ 60, 160, 90, 255 } : (Clay_Color){ 45, 130, 75, 255 },
                .cornerRadius = CLAY_CORNER_RADIUS(8)
            }
        ) {
            CLAY_TEXT(
                CLAY_STRING("OK"),
                CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 255 }, .fontSize = 16 })
            );
        }

        if (btnClicked || (input.isFocused && IsKeyPressed(KEY_ENTER))) {
            TraceLog(LOG_INFO, "Texto Submetido: %s", input.text);
            input.isFocused = false;
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

    // static so its persistent between frames, if not it segfaults
    static TextInputState ping_input = {
        .text = {0},
        .textLength = 0,
        .isFocused = false,
        .placeholder = "Ping to connect"
    };

    Clay_BeginLayout();
    CLAY(
        CLAY_ID("OverlayRoot"),
        {
            .layout = {
                .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) }
            }
        }
    ) {
        CLAY(
            CLAY_ID("SearchBar"),
            {}
        ){
            draw_search_bar(ping_input);
        };
        CLAY(
            CLAY_ID("BottomButton"),
            {
                .layout = {
                    .sizing = { .width = CLAY_SIZING_FIXED(200), .height = CLAY_SIZING_FIXED(50) },
                    .padding = { 12, 12 },
                    .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER }
                },
                .backgroundColor = Clay_PointerOver(Clay_GetElementId(CLAY_STRING("BottomButton"))) 
                                   ? (Clay_Color){ 70, 140, 240, 255 }  // hover color
                                   : (Clay_Color){ 50, 100, 200, 100 }, // default color, can be transparent
                .cornerRadius = CLAY_CORNER_RADIUS(10),
                .floating = {
                    .offset = { .y = -20 },
                    .zIndex = 1,
                    .attachPoints = { 
                        .element = CLAY_ATTACH_POINT_CENTER_BOTTOM, 
                        .parent = CLAY_ATTACH_POINT_CENTER_BOTTOM 
                    },
                    .attachTo = CLAY_ATTACH_TO_PARENT,
                }
            }
        ) {
            CLAY_TEXT(
                CLAY_STRING("Show Screen"),
                CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 255 }, .fontSize = 20 })
            );
        }
    }
    
    Clay_RenderCommandArray renderCommands = Clay_EndLayout(GetFrameTime());

    Clay_Raylib_Render(renderCommands, gctx.fonts);
}

void close_gui(){
    Clay_Raylib_Close();
}

