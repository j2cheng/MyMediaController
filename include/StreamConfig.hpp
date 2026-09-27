#pragma once

#include <string>

namespace newcsio {

/* Mirrors the two roles the old cresRTSP manager supported. */
enum class StreamRole
{
    Server, // RTSP server: encode + serve (was CRESRTSP_MODE_SERVER)
    Client  // RTSP client: connect + decode (was CRESRTSP_MODE_CLIENT)
};

constexpr int kMaxStreams = 4;

/* Per-stream configuration; the modern replacement for CresRTSPConfig. Plain
 * value type so it can be copied freely across the queue boundary. */
struct StreamConfig
{
    int streamId{-1};
    StreamRole role{StreamRole::Client};

    std::string url;             // rtsp:// source (client) or mount (server)
    std::string multicastAddr;   // optional multicast group

    int serverPort{554};         // listen port (server)
    int clientPort{5000};        // local receive port (client)
    int rxUdpPort{0};
    int txUdpPort{0};

    bool multicast{false};
    int  encodingType{0};

    bool valid() const { return streamId >= 0 && streamId < kMaxStreams; }
};

} // namespace newcsio
