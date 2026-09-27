#ifndef NEWCSIO_RTSP_STREAM_INTERFACE_H_
#define NEWCSIO_RTSP_STREAM_INTERFACE_H_

#ifdef __cplusplus
extern "C"
{
#endif

/* C facade mirroring the old cresRTSPProjectInterface.h so existing callers
 * (message handlers) can switch over with minimal churn. All calls are
 * asynchronous: they enqueue onto the controller actor and return at once. */

typedef enum
{
    NEWCSIO_ROLE_SERVER = 0,
    NEWCSIO_ROLE_CLIENT = 1
} NewCsioRole;

/* Plain-C mirror of newcsio::StreamConfig for the interop boundary. */
typedef struct
{
    int         streamId;
    NewCsioRole role;
    const char *url;           /* may be NULL */
    const char *multicastAddr; /* may be NULL */
    int         serverPort;
    int         clientPort;
    int         rxUdpPort;
    int         txUdpPort;
    int         multicast;     /* 0/1 */
    int         encodingType;
} NewCsioStreamConfig;

void NewCsio_Init(void);
void NewCsio_DeInit(void);

void NewCsio_Config(const NewCsioStreamConfig *config);

/* Configure and start atomically (role taken from config->role). */
void NewCsio_Start(const NewCsioStreamConfig *config);

void NewCsio_StartServer(int streamId);
void NewCsio_StopServer(int streamId);
void NewCsio_StartClient(int streamId);
void NewCsio_StopClient(int streamId);
void NewCsio_SendKeepAlive(int streamId);
void NewCsio_RestartClient(int streamId);

#ifdef __cplusplus
}
#endif

#endif /* NEWCSIO_RTSP_STREAM_INTERFACE_H_ */
