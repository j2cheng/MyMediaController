# MyMediaController — RTSP media with an actor / watchdog / queue architecture

A modern C++17 rewrite of the `csio/cresRTSP` RTSP media client/server, using the
same threading model discussed for `collab_stream_in_v4`
(`ClientVerifySession` + `CommandQueue` + watchdog): **actor threads own all
state, cross-thread work is passed as closures over a queue, and an independent
watchdog restarts stuck sessions.**

## Mapping from the old design

| Old (`csio/cresRTSP`)                         | New (`NewCsio`)                        |
| --------------------------------------------- | -------------------------------------- |
| `CCresRTSPProject` (dispatcher thread + ring buffer) | `StreamController` (actor + `CommandQueue`) |
| `CCresRTSPManager` / `CCresRTSPClient` / `CCresRTSPServer` / `CCresGstAppClient` | `MediaSession` (one GLib-loop actor per stream) |
| `csioThreadBaseClass` + hand-rolled event queue | `std::thread` + `CommandQueue` / `g_main_context_invoke` |
| manual restart-on-error / keepalive polling   | `Watchdog` thread + heartbeat atomics  |
| `CresRTSP_*` C API                            | `NewCsio_*` C API (`RtspStreamInterface.h`) |

## The four pillars you asked for

1. **Queues between two threads** — `CommandQueue` is a single-consumer
   `std::function` queue. Any thread `push()`es; one actor thread drains it in
   `waitFor()`. Used by `StreamController`.
2. **Actor threads for GLib media code** — each `MediaSession` runs a
   `GMainLoop` on its own thread. Other threads never touch the pipeline; they
   post closures with `g_main_context_invoke()`, which run on the loop thread.
   That is the "queue between two threads" for GLib.
3. **Watchdog monitoring threads** — `Watchdog` polls each session's
   `alive()` (a heartbeat + health atomic). On a stall it *signals only*,
   enqueuing a restart back onto the controller actor — it never restarts a
   session directly (same discipline as the gRPC watchdog's `TryCancel`).
4. **A control path for start/stop** — callers (or the C API) invoke
   `StreamController::start/stopServer/stopClient/...`, which enqueue commands to the
   controller actor. The calling thread returns immediately.

## Threading invariant

```
caller threads ─┐
                ├─push(cmd)─► [CommandQueue] ─► StreamController actor ─► owns config + sessions
watchdog thread ┘                                     │ start/stop/restart
                                                      ▼
                            MediaSession actor (GMainLoop) ─► owns GStreamer pipeline
                                                      ▲ g_main_context_invoke(cmd)
                            heartbeat/health atomics ─┘ (read by Watchdog)
```

No thread ever holds another's lock; session state is lock-free because only its
owning actor touches it.

## Files

- `include/CommandQueue.hpp`, `src/CommandQueue.cpp` — the task queue.
- `include/Watchdog.hpp`, `src/Watchdog.cpp` — the monitor thread.
- `include/MediaSession.hpp`, `src/MediaSession.cpp` — GLib-loop media actor.
- `include/StreamController.hpp`, `src/StreamController.cpp` — dispatcher actor.
- `include/RtspStreamInterface.h`, `src/RtspStreamInterface.cpp` — C facade.
- `include/StreamConfig.hpp` — shared value types.
- `test/unit_test.cpp` — unit tests for the library.

## Build

Requires GStreamer 1.0 dev packages (`gstreamer-1.0`), CMake >= 3.14, and a
C++17 compiler. The library builds as a shared object, `libStreamController.so`.

Using the helper script:

```sh
./build.sh                 # configure + build (Release)
./build.sh -t Debug        # Debug build
./build.sh -c              # clean first
./build.sh -p              # also package a distribution zip
```

Or with CMake directly:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## Distribution package

`./build.sh -p` (or `cpack --config build/CPackConfig.cmake -B build`) produces
`build/StreamController-<version>-Linux.zip` containing the shared library and
public headers:

```
lib/libStreamController.so
include/   (RtspStreamInterface.h, StreamController.hpp, StreamConfig.hpp, ...)
```

## Notes / TODO for production

- The server role uses a placeholder encode pipeline; wire `GstRTSPServer` to a
  mount point on `serverPort` for a real RTSP server.
- The client keep-alive is a marker; reach the `rtspsrc`/`GstRTSPConnection` to
  issue a real RTSP `GET_PARAMETER` keepalive.
- `Log.hpp` is a `printf` shim; route it to the platform logger (`STRLOG`).
