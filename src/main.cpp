#include <chrono>
#include <thread>

#include "RtspStreamInterface.h"

/* Tiny smoke driver showing the command flow across threads:
 * main thread issues config/start/keepalive/stop; the controller actor runs
 * them; each MediaSession actor owns its GLib loop; the watchdog restarts a
 * session if its heartbeat goes stale. */
int main()
{
    NewCsio_Init();

    NewCsioStreamConfig client{};
    client.streamId = 1;
    client.role     = NEWCSIO_ROLE_CLIENT;
    client.url      = "rtsp://127.0.0.1:8554/test";
    NewCsio_Config(&client);

    NewCsioStreamConfig server{};
    server.streamId  = 0;
    server.role      = NEWCSIO_ROLE_SERVER;
    server.serverPort = 8554;
    NewCsio_Config(&server);

    NewCsio_StartServer(0);
    NewCsio_StartClient(1);

    for (int i = 0; i < 5; ++i)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        NewCsio_SendKeepAlive(1);
    }

    NewCsio_StopClient(1);
    NewCsio_StopServer(0);

    NewCsio_DeInit();
    return 0;
}
