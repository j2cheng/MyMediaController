#include "MediaSession.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <utility>

#include <gst/gst.h>

#include "Log.hpp"

namespace newcsio {

namespace {

/* Heartbeat is considered stale (session stuck) after this long. */
constexpr int64_t kHeartbeatTimeoutMs = 5000;

int64_t nowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

/* Runs a queued closure on the loop thread, then frees it. */
gboolean invokeTrampoline(gpointer data)
{
    auto *fn = static_cast<std::function<void()> *>(data);
    (*fn)();
    return G_SOURCE_REMOVE;
}

void invokeDestroy(gpointer data)
{
    delete static_cast<std::function<void()> *>(data);
}

} // namespace

struct MediaSession::Impl
{
    explicit Impl(const StreamConfig &cfg)
        : config{cfg}
    {
        context_ = g_main_context_new();
        loop_    = g_main_loop_new(context_, FALSE);
        // Bump once so a freshly-created session isn't seen as stale.
        heartbeat_.store(nowMs());
        healthy_.store(true);
        loopThread_ = std::thread{[this] { runLoop(); }};
    }

    ~Impl()
    {
        // Ask the loop thread to tear down the pipeline, then quit the loop.
        enqueue([this] { teardownPipeline(); });
        enqueue([this] { g_main_loop_quit(loop_); });
        if (loopThread_.joinable()) loopThread_.join();

        g_main_loop_unref(loop_);
        g_main_context_unref(context_);
    }

    /* ---- cross-thread entry points: enqueue onto the loop thread ---- */

    void enqueue(std::function<void()> fn)
    {
        auto *p = new std::function<void()>(std::move(fn));
        g_main_context_invoke_full(
            context_, G_PRIORITY_DEFAULT, invokeTrampoline, p, invokeDestroy);
    }

    /* ---- loop-thread-only state and handlers ---- */

    void runLoop()
    {
        g_main_context_push_thread_default(context_);
        LOG_INFO("session %d: loop thread entered", config.streamId);
        g_main_loop_run(loop_);
        LOG_INFO("session %d: loop thread exiting", config.streamId);
        g_main_context_pop_thread_default(context_);
    }

    void buildPipeline()
    {
        if (pipeline_) return; // already running

        std::string desc = describePipeline();
        GError *err      = nullptr;
        pipeline_        = gst_parse_launch(desc.c_str(), &err);
        if (!pipeline_)
        {
            LOG_ERROR(
                "session %d: pipeline build failed: %s", config.streamId,
                err ? err->message : "unknown");
            if (err) g_error_free(err);
            healthy_.store(false);
            return;
        }
        if (err) g_error_free(err);

        // Attach a bus watch to THIS context (not the default one).
        GstBus *bus     = gst_element_get_bus(pipeline_);
        busWatch_       = gst_bus_create_watch(bus);
        g_source_set_callback(
            busWatch_, (GSourceFunc)&Impl::onBusMessage, this, nullptr);
        g_source_attach(busWatch_, context_);
        gst_object_unref(bus);

        // Per-second heartbeat while healthy, attached to THIS context.
        heartbeatSource_ = g_timeout_source_new_seconds(1);
        g_source_set_callback(
            heartbeatSource_, &Impl::onHeartbeat, this, nullptr);
        g_source_attach(heartbeatSource_, context_);

        gst_element_set_state(pipeline_, GST_STATE_PLAYING);
        healthy_.store(true);
        heartbeat_.store(nowMs());
        LOG_INFO("session %d: pipeline PLAYING", config.streamId);
    }

    void teardownPipeline()
    {
        if (heartbeatSource_)
        {
            g_source_destroy(heartbeatSource_);
            g_source_unref(heartbeatSource_);
            heartbeatSource_ = nullptr;
        }
        if (busWatch_)
        {
            g_source_destroy(busWatch_);
            g_source_unref(busWatch_);
            busWatch_ = nullptr;
        }
        if (pipeline_)
        {
            gst_element_set_state(pipeline_, GST_STATE_NULL);
            gst_object_unref(pipeline_);
            pipeline_ = nullptr;
            LOG_INFO("session %d: pipeline stopped", config.streamId);
        }
    }

    void sendKeepAliveOnLoop()
    {
        if (config.role == StreamRole::Client && pipeline_)
        {
            /* Real impl: reach the rtspsrc/GstRTSPConnection and issue an RTSP
             * GET_PARAMETER keepalive (was CCresGstAppClient::sendKeepAliveMsg).
             * Kept as a marker here to focus on the threading model. */
            LOG_DEBUG("session %d: RTSP keep-alive sent", config.streamId);
        }
    }

    std::string describePipeline() const
    {
        if (config.role == StreamRole::Client)
        {
            // RTSP client: pull, decode, render/consume.
            return "rtspsrc location=" + config.url
                + " latency=200 ! decodebin ! fakesink sync=false";
        }
        /* RTSP server: a production build wires GstRTSPServer to a mount point
         * on config.serverPort. Represented here by a self-contained encode
         * pipeline so the actor/watchdog wiring stays the focus. */
        return "videotestsrc is-live=true ! x264enc tune=zerolatency ! "
               "rtph264pay ! fakesink sync=false";
    }

    /* ---- static GLib callbacks (run on the loop thread) ---- */

    static gboolean onBusMessage(GstBus *, GstMessage *msg, gpointer data)
    {
        auto *self = static_cast<Impl *>(data);
        switch (GST_MESSAGE_TYPE(msg))
        {
        case GST_MESSAGE_ERROR:
        {
            GError *err = nullptr;
            gchar *dbg  = nullptr;
            gst_message_parse_error(msg, &err, &dbg);
            LOG_ERROR(
                "session %d: pipeline error: %s", self->config.streamId,
                err ? err->message : "unknown");
            if (err) g_error_free(err);
            g_free(dbg);
            self->healthy_.store(false); // watchdog will see this
            break;
        }
        case GST_MESSAGE_EOS:
            LOG_WARNING("session %d: end of stream", self->config.streamId);
            self->healthy_.store(false);
            break;
        default:
            break;
        }
        return TRUE; // keep watching
    }

    static gboolean onHeartbeat(gpointer data)
    {
        auto *self = static_cast<Impl *>(data);
        if (self->healthy_.load()) self->heartbeat_.store(nowMs());
        return G_SOURCE_CONTINUE;
    }

    StreamConfig config;

    GMainContext *context_{nullptr};
    GMainLoop *loop_{nullptr};
    GstElement *pipeline_{nullptr};
    GSource *busWatch_{nullptr};
    GSource *heartbeatSource_{nullptr};

    std::atomic<int64_t> heartbeat_{0};
    std::atomic<bool> healthy_{true};

    std::thread loopThread_;
};

MediaSession::MediaSession(const StreamConfig &config)
    : impl_{std::make_unique<Impl>(config)}
{ }

MediaSession::~MediaSession() = default;

void MediaSession::start()
{
    impl_->enqueue([impl = impl_.get()] { impl->buildPipeline(); });
}

void MediaSession::stop()
{
    impl_->enqueue([impl = impl_.get()] { impl->teardownPipeline(); });
}

void MediaSession::sendKeepAlive()
{
    impl_->enqueue([impl = impl_.get()] { impl->sendKeepAliveOnLoop(); });
}

bool MediaSession::alive() const
{
    if (!impl_->healthy_.load()) return false;
    return (nowMs() - impl_->heartbeat_.load()) < kHeartbeatTimeoutMs;
}

int MediaSession::streamId() const
{
    return impl_->config.streamId;
}

} // namespace newcsio
