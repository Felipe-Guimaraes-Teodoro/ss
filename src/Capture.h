#pragma once
#include <mutex>
#include <thread>
#include <vector>
#include "core.h"
#include "spa/utils/hook.h"
#include <libportal/portal-helpers.h>

struct ScreenCapturer {
    XdpPortal *portal = nullptr;
    XdpSession *session = nullptr;
    GMainLoop *glib_loop = nullptr;
    std::thread loop_thread;
    // std::thread pw_thread;

    struct pw_main_loop *pw_loop = nullptr;
    struct pw_context *pw_context = nullptr;
    struct pw_core *pw_core = nullptr;
    struct pw_stream *pw_stream = nullptr;
    
    struct spa_hook core_listener;
    struct spa_hook stream_listener;

    std::mutex mutex;
    std::vector<u8> latest_frame_data;
    int width = 0;
    int height = 0;
    uint32_t target_node_id = 0;
    bool frame_ready = false;
};

ScreenCapturer* init_screen_capture(int width, int height); 

void free_screen_capture(ScreenCapturer *cap); 
u8* capture_next_frame(ScreenCapturer *cap);
void release_captured_frame(ScreenCapturer *cap);

