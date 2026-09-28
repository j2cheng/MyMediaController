#include "RtspStreamInterface.h"

#include <memory>

#include "Log.hpp"
#include "StreamController.hpp"

namespace {
std::unique_ptr<newcsio::StreamController> g_controller;

newcsio::StreamConfig toStreamConfig(const NewCsioStreamConfig *config)
{
    newcsio::StreamConfig cfg{};
    cfg.streamId      = config->streamId;
    cfg.role          = config->role == NEWCSIO_ROLE_SERVER
                          ? newcsio::StreamRole::Server
                          : newcsio::StreamRole::Client;
    cfg.url           = config->url ? config->url : "";
    cfg.multicastAddr = config->multicastAddr ? config->multicastAddr : "";
    cfg.serverPort    = config->serverPort;
    cfg.clientPort    = config->clientPort;
    cfg.rxUdpPort     = config->rxUdpPort;
    cfg.txUdpPort     = config->txUdpPort;
    cfg.multicast     = config->multicast != 0;
    cfg.encodingType  = config->encodingType;
    return cfg;
}
} // namespace

extern "C" {

void NewCsio_Init(void)
{
    if (!g_controller)
        g_controller = std::make_unique<newcsio::StreamController>();
}

void NewCsio_DeInit(void)
{
    g_controller.reset();
}

void NewCsio_Start(const NewCsioStreamConfig *config)
{
    if (!g_controller || !config) return;
    g_controller->start(toStreamConfig(config));
}

void NewCsio_StopServer(int streamId)
{
    if (g_controller) g_controller->stopServer(streamId);
}

void NewCsio_StopClient(int streamId)
{
    if (g_controller) g_controller->stopClient(streamId);
}

void NewCsio_SendKeepAlive(int streamId)
{
    if (g_controller) g_controller->sendKeepAlive(streamId);
}

void NewCsio_RestartClient(int streamId)
{
    if (g_controller) g_controller->restartClient(streamId);
}

} // extern "C"
