#include "StreamController.hpp"

#include <array>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <unordered_map>
#include <utility>

#include <gst/gst.h>

#include "CommandQueue.hpp"
#include "Log.hpp"
#include "MediaSession.hpp"
#include "Watchdog.hpp"

namespace newcsio {

namespace {
constexpr std::chrono::milliseconds kWatchdogPoll{1000};
} // namespace

struct StreamController::Impl
{
    Impl()
    {
        gst_init(nullptr, nullptr);

        watchdog_ = std::make_unique<Watchdog>(
            kWatchdogPoll, [this](int id) {
                // Watchdog only signals; the restart runs on the actor thread.
                enqueue([this, id] { handleRestart(id); });
            });

        actorThread_ = std::thread{[this] { runActor(); }};
    }

    ~Impl()
    {
        queue_.stop();
        if (actorThread_.joinable()) actorThread_.join();
        watchdog_.reset(); // joins watchdog thread before sessions destruct
        sessions_.clear(); // each MediaSession dtor joins its loop thread
        gst_deinit();
    }

    void enqueue(std::function<void()> command)
    {
        queue_.push(std::move(command));
    }

    /* ---- actor thread ---- */

    void runActor()
    {
        for (;;)
        {
            auto popped = queue_.waitFor(
                std::chrono::steady_clock::time_point::max());
            if (popped.command)
            {
                popped.command();
                continue;
            }
            if (popped.stopped) return;
        }
    }

    /* ---- actor-thread-only handlers (no locks needed) ---- */

    void handleConfigure(const StreamConfig &config)
    {
        if (!config.valid())
        {
            LOG_WARNING("configure: invalid stream id %d", config.streamId);
            return;
        }
        configTable_[config.streamId] = config;
        LOG_INFO("configure: stream %d role=%d", config.streamId,
                 static_cast<int>(config.role));
    }

    void handleStartConfigured(const StreamConfig &config)
    {
        handleConfigure(config);
        if (config.valid()) handleStart(config.streamId, config.role);
    }

    void handleStart(int streamId, StreamRole role)
    {
        if (streamId < 0 || streamId >= kMaxStreams)
        {
            LOG_WARNING("start: invalid stream id %d", streamId);
            return;
        }
        StreamConfig &cfg = configTable_[streamId];
        if (!cfg.valid())
        {
            LOG_WARNING("start: stream %d not configured", streamId);
            return;
        }
        if (sessions_.count(streamId))
        {
            LOG_INFO("start: stream %d already running", streamId);
            return;
        }

        cfg.role  = role;
        auto sess = std::make_unique<MediaSession>(cfg);
        MediaSession *raw = sess.get();
        sess->start();
        sessions_.emplace(streamId, std::move(sess));

        // Probe reads the session's atomics; safe from the watchdog thread.
        watchdog_->watch(streamId, [raw] { return raw->alive(); });
        LOG_INFO("start: stream %d started", streamId);
    }

    void handleStop(int streamId)
    {
        watchdog_->unwatch(streamId);
        auto it = sessions_.find(streamId);
        if (it == sessions_.end())
        {
            LOG_INFO("stop: stream %d not running", streamId);
            return;
        }
        sessions_.erase(it); // dtor stops pipeline and joins loop thread
        LOG_INFO("stop: stream %d stopped", streamId);
    }

    void handleKeepAlive(int streamId)
    {
        auto it = sessions_.find(streamId);
        if (it != sessions_.end()) it->second->sendKeepAlive();
    }

    void handleRestart(int streamId)
    {
        auto it = sessions_.find(streamId);
        if (it == sessions_.end()) return; // stopped meanwhile

        const StreamRole role = configTable_[streamId].role;
        LOG_WARNING("restart: rebuilding stream %d", streamId);
        handleStop(streamId);
        handleStart(streamId, role);
    }

    CommandQueue queue_;
    std::array<StreamConfig, kMaxStreams> configTable_{};
    std::unordered_map<int, std::unique_ptr<MediaSession>> sessions_;
    std::unique_ptr<Watchdog> watchdog_;
    std::thread actorThread_;
};

StreamController::StreamController()
    : impl_{std::make_unique<Impl>()}
{ }

StreamController::~StreamController() = default;

void StreamController::start(const StreamConfig &config)
{
    impl_->enqueue(
        [impl = impl_.get(), config] { impl->handleStartConfigured(config); });
}

void StreamController::stopServer(int streamId)
{
    impl_->enqueue([impl = impl_.get(), streamId] { impl->handleStop(streamId); });
}

void StreamController::stopClient(int streamId)
{
    impl_->enqueue([impl = impl_.get(), streamId] { impl->handleStop(streamId); });
}

void StreamController::sendKeepAlive(int streamId)
{
    impl_->enqueue([impl = impl_.get(), streamId] {
        impl->handleKeepAlive(streamId);
    });
}

void StreamController::restartClient(int streamId)
{
    impl_->enqueue([impl = impl_.get(), streamId] {
        impl->handleRestart(streamId);
    });
}

} // namespace newcsio
