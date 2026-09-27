#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>

/* Independent monitor thread. Callers register a monitored subject by id with a
 * probe callback that reports whether the subject is still alive (heartbeat
 * fresh AND healthy). If a probe reports dead, the watchdog invokes onStall(id)
 * exactly once and then waits for the subject to be re-registered (after the
 * owner restarts it).
 *
 * The watchdog NEVER touches subject state directly: onStall only signals (the
 * owner's actor performs the real restart), mirroring the gRPC verify-session
 * watchdog that merely calls TryCancel(). */
class Watchdog
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

public:
    /* probe: return true while the subject is alive. onStall: invoked on the
     * watchdog thread when a probe first returns false. */
    using Probe   = std::function<bool()>;
    using OnStall = std::function<void(int id)>;

    explicit Watchdog(
        std::chrono::milliseconds pollInterval, OnStall onStall);
    ~Watchdog();

    Watchdog(const Watchdog &)            = delete;
    Watchdog &operator=(const Watchdog &) = delete;

    void watch(int id, Probe probe);
    void unwatch(int id);
};
