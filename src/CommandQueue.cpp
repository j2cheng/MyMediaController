#include "CommandQueue.hpp"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <utility>

struct CommandQueue::Impl
{
    std::mutex mutex;
    std::condition_variable condition;
    std::deque<std::function<void()>> commands;
    bool stopping{false};
};

CommandQueue::CommandQueue()
    : impl_{std::make_unique<Impl>()}
{ }

CommandQueue::~CommandQueue() = default;

void CommandQueue::push(std::function<void()> command)
{
    {
        std::lock_guard<std::mutex> lock{impl_->mutex};
        impl_->commands.push_back(std::move(command));
    }
    impl_->condition.notify_one();
}

void CommandQueue::stop()
{
    {
        std::lock_guard<std::mutex> lock{impl_->mutex};
        impl_->stopping = true;
    }
    impl_->condition.notify_one();
}

CommandQueue::PopResult
CommandQueue::waitFor(std::chrono::steady_clock::time_point deadline)
{
    std::unique_lock<std::mutex> lock{impl_->mutex};
    impl_->condition.wait_until(lock, deadline, [this] {
        return impl_->stopping || !impl_->commands.empty();
    });

    PopResult result{};
    if (!impl_->commands.empty())
    {
        result.command = std::move(impl_->commands.front());
        impl_->commands.pop_front();
        return result;
    }
    result.stopped = impl_->stopping;
    return result;
}
