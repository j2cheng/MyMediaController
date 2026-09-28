#pragma once

#include <memory>

#include "StreamConfig.hpp"

namespace newcsio {

/* The dispatcher ACTOR (modern replacement for CCresRTSPProject).
 *
 *   caller threads --config/start/stop/keepAlive--\
 *                                                  > [CommandQueue] --> actor
 *   watchdog thread --onStall(restart)------------/                    thread
 *
 * Every public method returns immediately after enqueuing; the single actor
 * thread owns the config table and the live MediaSession map, so that state
 * needs no locks. A Watchdog thread probes each session's liveness and, on a
 * stall, enqueues a restart back onto this actor -- it never restarts directly. */
class StreamController
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

public:
    StreamController();
    ~StreamController();

    StreamController(const StreamController &)            = delete;
    StreamController &operator=(const StreamController &) = delete;

    /* Configure and start in one atomic command (role comes from config).
     * Preferred entry point: immune to cross-thread config/start ordering. */
    void start(const StreamConfig &config);

    void stopServer(int streamId);
    void stopClient(int streamId);
    void sendKeepAlive(int streamId);
    void restartClient(int streamId);
};

} // namespace newcsio
