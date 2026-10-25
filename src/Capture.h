#pragma once
#include <mutex>
#include <thread>
#include <vector>
#include "core.h"
#include "spa/utils/hook.h"
#include <libportal/portal-helpers.h>

struct ScreenCapturer {
    XdpPortal *portal;
    XdpSession *session;
    GMainLoop *glib_loop;
    std::thread loop_thread;
    // std::thread pw_thread;

    struct pw_main_loop *pw_loop;
    struct pw_context *pw_context;
    struct pw_core *pw_core;
    struct pw_stream *pw_stream;
    
    struct spa_hook core_listener;
    struct spa_hook stream_listener;

    std::mutex mutex;
    std::vector<u8> latest_frame_data;
    int width;
    int height;
    uint32_t target_node_id;
    bool frame_ready;
};

ScreenCapturer* init_screen_capture(int width, int height); 

void free_screen_capture(ScreenCapturer *cap); 
u8* capture_next_frame(ScreenCapturer *cap);
void release_captured_frame(ScreenCapturer *cap);

