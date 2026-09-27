#include "Watchdog.hpp"

#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>

#include "Log.hpp"

struct Watchdog::Impl
{
    struct Subject
    {
        Probe probe;
        bool stalledReported{false};
    };

    std::chrono::milliseconds pollInterval;
    OnStall onStall;

    std::mutex mutex;
    std::condition_variable condition;
    std::unordered_map<int, Subject> subjects;
    bool stopping{false};

    std::thread thread;

    void run();
};

void Watchdog::Impl::run()
{
    std::unique_lock<std::mutex> lock{mutex};
    for (;;)
    {
        condition.wait_for(lock, pollInterval, [this] { return stopping; });
        if (stopping) return;

        /* Snapshot the ids whose probes report dead, so the (possibly slow)
         * onStall callback runs without holding the watchdog lock. */
        for (auto &entry : subjects)
        {
            Subject &subject = entry.second;
            if (subject.stalledReported) continue;

            const bool alive = subject.probe ? subject.probe() : true;
            if (alive) continue;

            const int id            = entry.first;
            subject.stalledReported = true;

            lock.unlock();
            LOG_WARNING("watchdog: subject %d stalled, signalling owner", id);
            if (onStall) onStall(id);
            lock.lock();
        }
    }
}

Watchdog::Watchdog(
    std::chrono::milliseconds pollInterval, OnStall onStall)
    : impl_{std::make_unique<Impl>()}
{
    impl_->pollInterval = pollInterval;
    impl_->onStall      = std::move(onStall);
    impl_->thread       = std::thread{[this] { impl_->run(); }};
}

Watchdog::~Watchdog()
{
    {
        std::lock_guard<std::mutex> lock{impl_->mutex};
        impl_->stopping = true;
    }
    impl_->condition.notify_one();
    if (impl_->thread.joinable()) impl_->thread.join();
}

void Watchdog::watch(int id, Probe probe)
{
    {
        std::lock_guard<std::mutex> lock{impl_->mutex};
        impl_->subjects[id] = Impl::Subject{std::move(probe), false};
    }
    impl_->condition.notify_one();
}

void Watchdog::unwatch(int id)
{
    std::lock_guard<std::mutex> lock{impl_->mutex};
    impl_->subjects.erase(id);
}
