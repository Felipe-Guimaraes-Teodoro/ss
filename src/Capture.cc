#include "core.h"
#include "pipewire/stream.h"
#include <libportal/portal.h>
#include <libportal/session.h>
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/utils/result.h>
#include <glib.h>
#include <gio/gio.h>
#include <thread>
#include <mutex>
#include <vector>
#include <optional>
#include <chrono>
#include <cstdio>
#include <cstring>

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

static void on_process(void *data) {
    ScreenCapturer *self = (ScreenCapturer *)data;
    struct pw_buffer *b;
    if ((b = pw_stream_dequeue_buffer(self->pw_stream)) == nullptr)
        return;

    struct spa_buffer *buf = b->buffer;
    if (buf->datas[0].data == nullptr) {
        pw_stream_queue_buffer(self->pw_stream, b);
        return;
    }

    void *ptr = buf->datas[0].data;
    uint32_t stride = buf->datas[0].chunk ? buf->datas[0].chunk->stride : (self->width * 4);
    uint32_t img_size = self->height * stride;

    std::lock_guard<std::mutex> lock(self->mutex);
    if (self->latest_frame_data.size() != img_size) {
        self->latest_frame_data.resize(img_size);
    }
    std::memcpy(self->latest_frame_data.data(), ptr, img_size);
    self->frame_ready = true;

    pw_stream_queue_buffer(self->pw_stream, b);
}

static void on_stream_param_changed(void *data, uint32_t id, const struct spa_pod *param) {
    ScreenCapturer *self = (ScreenCapturer *)data;
    
    if (param == nullptr || id != SPA_PARAM_Format)
        return;

    struct spa_video_info_raw info;
    if (spa_format_video_raw_parse(param, &info) >= 0) {
        std::lock_guard<std::mutex> lock(self->mutex);
        self->width = info.size.width;
        self->height = info.size.height;
        
        std::printf("Negotiated Capture Resolution: %dx%d\n", self->width, self->height);
    }
}

static const struct pw_stream_events stream_events = {
    .version = PW_VERSION_STREAM_EVENTS,
    .param_changed = on_stream_param_changed,
    .process = on_process,
};

static void core_done(void *data, uint32_t id, int seq) {
    ScreenCapturer *self = (ScreenCapturer *)data;
    if (id == PW_ID_CORE) {
        std::printf("PipeWire core synced. Connecting stream to Node ID: %u...\n", self->target_node_id);

        struct pw_properties *props = pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Video",
            PW_KEY_MEDIA_CATEGORY, "Capture",
            PW_KEY_MEDIA_ROLE, "Screen",
            nullptr
        );

        self->pw_stream = pw_stream_new(
            self->pw_core,
            "screen-capture-stream",
            props
        );

        pw_stream_add_listener(self->pw_stream, &self->stream_listener, &stream_events, self);

        // Replace your existing spa_pod_builder block with this:
        uint8_t buffer[1024];
        struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
        const struct spa_pod *params[1];

        // Allow the compositor to choose the format, size, and framerate. 
        // We offer an Enum of common 32-bit formats.
        params[0] = (struct spa_pod *)spa_pod_builder_add_object(&b,
                SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
                SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
                SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
                SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(4, 
                    SPA_VIDEO_FORMAT_BGRA,
                    SPA_VIDEO_FORMAT_RGBA, 
                    SPA_VIDEO_FORMAT_RGBx, 
                    SPA_VIDEO_FORMAT_BGRx 
                    )
                // Omit SPA_FORMAT_VIDEO_size and framerate entirely so the server dictates them
                );

        pw_stream_connect(
            self->pw_stream,
            PW_DIRECTION_INPUT,
            self->target_node_id,
            (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS),
            params, 1
        );
    }
}

// Fully designated initializers
static const struct pw_core_events core_events = {
    .version = PW_VERSION_CORE_EVENTS,
    .done = core_done,
};

static void start_pipewire_remote(ScreenCapturer *self, int node_id, int fd) {
    self->target_node_id = node_id;
    pw_init(nullptr, nullptr);
    self->pw_loop = pw_main_loop_new(nullptr);
    
    self->pw_context = pw_context_new(pw_main_loop_get_loop(self->pw_loop), nullptr, 0);
    self->pw_core = pw_context_connect_fd(self->pw_context, fd, nullptr, 0);

    if (!self->pw_core) {
        std::printf("Failed to connect PipeWire via portal socket FD!\n");
        return;
    }

    pw_core_add_listener(self->pw_core, &self->core_listener, &core_events, self);
    pw_core_sync(self->pw_core, PW_ID_CORE, 0);

    std::thread([self]() {
        pw_main_loop_run(self->pw_loop);
    }).detach();
}

static void on_session_started(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    ScreenCapturer *self = (ScreenCapturer *)user_data;
    GError *error = nullptr;
    
    gboolean success = xdp_session_start_finish(XDP_SESSION(source_object), res, &error);
    if (!error && success) {
        std::printf("Screencast session started successfully!\n");
        
        GVariant *streams = xdp_session_get_streams(self->session);
        if (streams && g_variant_n_children(streams) > 0) {
            GVariantIter iter;
            g_variant_iter_init(&iter, streams);
            uint32_t node_id;
            GVariant *dict;
            if (g_variant_iter_next(&iter, "(u@a{sv})", &node_id, &dict)) {
                std::printf("Captured PipeWire Node ID: %u\n", node_id);
                g_variant_unref(dict);
                
                int fd = xdp_session_open_pipewire_remote(self->session);
                if (fd != -1) {
                    std::printf("Pipewire remote FD opened successfully: %d\n", fd);
                    start_pipewire_remote(self, node_id, fd);
                } else {
                    std::printf("Failed to open pipewire remote FD\n");
                }
            }
            g_variant_unref(streams);
        }
    } else {
        std::printf("Session start failed: %s\n", error ? error->message : "unknown");
        if (error) g_error_free(error);
    }
}

static void on_screencast_created(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    ScreenCapturer *self = (ScreenCapturer *)user_data;
    GError *error = nullptr;
    
    self->session = xdp_portal_create_screencast_session_finish(XDP_PORTAL(source_object), res, &error);
    if (error) {
        std::printf("Create screencast session failed: %s\n", error->message);
        g_error_free(error);
        return;
    }

    std::printf("Portal screencast session created. Prompting user...\n");
    xdp_session_start(self->session, nullptr, nullptr, on_session_started, self);
}

ScreenCapturer* init_screen_capture(int width, int height) {
    g_set_prgname("screen-capture-module");
    ScreenCapturer *self = new ScreenCapturer();
    self->width = width;
    self->height = height;

    self->glib_loop = g_main_loop_new(nullptr, FALSE);
    self->portal = xdp_portal_new();

    self->loop_thread = std::thread([self]() {
        xdp_portal_create_screencast_session(
            self->portal,
            XDP_OUTPUT_MONITOR,
            XDP_SCREENCAST_FLAG_NONE,
            XDP_CURSOR_MODE_EMBEDDED,
            XDP_PERSIST_MODE_NONE,
            nullptr,
            nullptr,
            on_screencast_created,
            self
        );
        g_main_loop_run(self->glib_loop);
    });
    self->loop_thread.detach();

    return self;
}

u8* capture_next_frame(ScreenCapturer *cap) {
    if (!cap) return nullptr;
    std::lock_guard<std::mutex> lock(cap->mutex);
    if (cap->frame_ready && !cap->latest_frame_data.empty()) {
        return cap->latest_frame_data.data();
    }
    return nullptr;
}

void release_captured_frame(ScreenCapturer *cap) {
    if (!cap) return;
    std::lock_guard<std::mutex> lock(cap->mutex);
    cap->frame_ready = false;
}

void free_screen_capture(ScreenCapturer *cap) {
    if (!cap) return;

    // Signal background event loops to stop immediately
    if (cap->pw_loop) {
        pw_main_loop_quit(cap->pw_loop);
    }
    if (cap->glib_loop) {
        g_main_loop_quit(cap->glib_loop);
    }

    if (cap->loop_thread.joinable()) {
        cap->loop_thread.join();
    }

    // PipeWire resources in reverse order of creation
    if (cap->pw_stream) {
        pw_stream_destroy(cap->pw_stream);
    }
    if (cap->pw_core) {
        pw_core_disconnect(cap->pw_core);
    }
    if (cap->pw_context) {
        pw_context_destroy(cap->pw_context);
    }
    if (cap->pw_loop) {
        pw_main_loop_destroy(cap->pw_loop);
    }

    // XDG Desktop Portal / GLib resources
    if (cap->session) {
        xdp_session_close(cap->session);
        g_object_unref(cap->session);
    }
    if (cap->portal) {
        g_object_unref(cap->portal);
    }
    if (cap->glib_loop) {
        g_main_loop_unref(cap->glib_loop);
    }

    // free vector memory 
    {
        std::lock_guard<std::mutex> lock(cap->mutex);
        cap->latest_frame_data.clear();
        cap->latest_frame_data.shrink_to_fit();
    }

    delete cap;
}

