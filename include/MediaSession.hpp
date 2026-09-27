#pragma once

#include <memory>

#include "StreamConfig.hpp"

namespace newcsio {

/* One media stream, realised as an ACTOR that owns a GStreamer pipeline.
 *
 * A dedicated thread runs a GLib main loop (GMainLoop + GMainContext). All
 * pipeline state is touched only on that loop thread, so no locks guard it.
 * Other threads interact by posting closures into the loop via
 * g_main_context_invoke() -- that is the "queue between two threads": the caller
 * thread enqueues, the actor (loop) thread executes.
 *
 * Liveness is exposed through two atomics the Watchdog may read from any thread:
 *   - heartbeat: bumped each second by an in-loop GLib timeout while healthy;
 *   - healthy:   cleared when the bus reports a pipeline error.
 * The Watchdog only reads these and signals; it never restarts the session
 * itself -- the StreamController's actor does that. */
class MediaSession
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

public:
    explicit MediaSession(const StreamConfig &config);
    ~MediaSession(); // stops the loop and joins the actor thread

    MediaSession(const MediaSession &)            = delete;
    MediaSession &operator=(const MediaSession &) = delete;

    /* Post work onto the loop thread; returns immediately. */
    void start();
    void stop();
    void sendKeepAlive();

    /* Watchdog liveness probe -- callable from any thread. Returns true while
     * the last heartbeat is fresh and no error was latched. */
    bool alive() const;

    int streamId() const;
};

} // namespace newcsio
