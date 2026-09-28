// Lightweight unit tests for the StreamController library.
// No external framework: a CHECK macro tracks failures and main() returns
// non-zero if any check fails (so CTest / CI report the result correctly).

#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "CommandQueue.hpp"
#include "RtspStreamInterface.h"

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        ++g_checks;                                                        \
        if (!(cond)) {                                                     \
            ++g_failures;                                                  \
            std::printf("  FAIL: %s (line %d)\n", #cond, __LINE__);        \
        }                                                                  \
    } while (0)

// CommandQueue is a deterministic, self-contained actor primitive: the ideal
// unit under test without spinning up GStreamer pipelines.
void test_command_queue_runs_in_order()
{
    std::printf("test_command_queue_runs_in_order\n");

    CommandQueue queue;
    std::vector<int> order;

    for (int i = 0; i < 5; ++i)
        queue.push([&order, i] { order.push_back(i); });

    // Drain everything already queued on this thread acting as the consumer.
    for (int i = 0; i < 5; ++i)
    {
        auto popped = queue.waitFor(std::chrono::steady_clock::now() +
                                    std::chrono::seconds(1));
        CHECK(popped.command != nullptr);
        if (popped.command) popped.command();
    }

    CHECK(order.size() == 5);
    for (int i = 0; i < static_cast<int>(order.size()); ++i)
        CHECK(order[i] == i);
}

void test_command_queue_stop_drains()
{
    std::printf("test_command_queue_stop_drains\n");

    CommandQueue queue;
    std::atomic<int> ran{0};
    queue.push([&ran] { ++ran; });
    queue.stop();

    // The queued command should still be delivered before the stop signal.
    auto first = queue.waitFor(std::chrono::steady_clock::now() +
                               std::chrono::seconds(1));
    CHECK(first.command != nullptr);
    if (first.command) first.command();
    CHECK(ran.load() == 1);

    auto second = queue.waitFor(std::chrono::steady_clock::now() +
                                std::chrono::seconds(1));
    CHECK(second.command == nullptr);
    CHECK(second.stopped);
}

void test_command_queue_timeout()
{
    std::printf("test_command_queue_timeout\n");

    CommandQueue queue;
    auto popped = queue.waitFor(std::chrono::steady_clock::now() +
                                std::chrono::milliseconds(20));
    CHECK(popped.command == nullptr);
    CHECK(!popped.stopped);
}

// Smoke test for the C facade: the controller must start up, accept a combined
// configure+start command, and tear down cleanly without crashing.
void test_c_api_lifecycle()
{
    std::printf("test_c_api_lifecycle\n");

    NewCsio_Init();

    NewCsioStreamConfig server{};
    server.streamId   = 0;
    server.role       = NEWCSIO_ROLE_SERVER;
    server.serverPort = 8554;
    NewCsio_Start(&server);

    NewCsioStreamConfig client{};
    client.streamId = 1;
    client.role     = NEWCSIO_ROLE_CLIENT;
    client.url      = "rtsp://127.0.0.1:8554/test";
    NewCsio_Start(&client);

    NewCsio_SendKeepAlive(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    NewCsio_StopClient(1);
    NewCsio_StopServer(0);
    NewCsio_DeInit();

    CHECK(true); // reaching here without a crash is the assertion
}

} // namespace

int main()
{
    test_command_queue_runs_in_order();
    test_command_queue_stop_drains();
    test_command_queue_timeout();
    test_c_api_lifecycle();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
