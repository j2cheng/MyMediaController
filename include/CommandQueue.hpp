#pragma once

#include <chrono>
#include <functional>
#include <memory>

/* Single-consumer task queue: any thread may push() a closure, one dedicated
 * consumer (actor) thread drains them via waitFor().
 *
 *   thread A --push(cmd)--\
 *   thread B --push(cmd)---> [ CommandQueue ] --waitFor()--> actor thread
 *   thread C --push(cmd)--/
 *
 * Only the queue's own mutex is ever held; commands run outside any lock, so an
 * actor that owns state behind this queue never needs to lock that state. */
class CommandQueue
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

public:
    struct PopResult
    {
        bool stopped{false};
        std::function<void()> command{};
    };

    CommandQueue();
    ~CommandQueue();

    CommandQueue(const CommandQueue &)            = delete;
    CommandQueue &operator=(const CommandQueue &) = delete;

    /* Enqueue a command from any thread. */
    void push(std::function<void()> command);

    /* Tell the consumer to stop once already-queued commands are drained. */
    void stop();

    /* Wait for the next command or the deadline, whichever comes first. A
     * command result means run it; stopped==true means the queue is draining
     * for shutdown; otherwise the deadline elapsed (use it for periodic work). */
    PopResult waitFor(std::chrono::steady_clock::time_point deadline);
};
