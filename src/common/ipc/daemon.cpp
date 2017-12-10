#include "daemon.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

#define internal static
#define local_persist static

internal int DaemonSockFD;
internal bool IsRunning;
internal pthread_t Thread;
internal daemon_callback *ConnectionCallback;

// NOTE(koekeishiya): Caller frees memory.
char *ReadFromSocket(int SockFD)
{
    int Length = 256;
    char *Result = (char *) malloc(Length);

    Length = recv(SockFD, Result, Length, 0);
    if (Length > 0) {
        Result[Length] = '\0';
    } else {
        free(Result);
        Result = NULL;
    }

    return Result;
}

void WriteToSocket(const char *Message, int SockFD)
{
    send(SockFD, Message, strlen(Message), 0);
}

void CloseSocket(int SockFD)
{
    shutdown(SockFD, SHUT_RDWR);
    close(SockFD);
}

/*
 * NOTE(koekeishiya): The connection must be closed manually by the implementor of the connection callback !!!
 */
internal void *
HandleConnection(void *)
{
    while (IsRunning) {
        struct sockaddr_in ClientAddr;
        local_persist socklen_t SinSize = sizeof(struct sockaddr);

        int SockFD = accept(DaemonSockFD, (struct sockaddr*)&ClientAddr, &SinSize);
        if (SockFD != -1) {
            char *Message = ReadFromSocket(SockFD);
            if (Message) {
                (*ConnectionCallback)(Message, SockFD);
                free(Message);
            }
        }
    }

    return NULL;
}

bool ConnectToDaemon(int *SockFD, int Port)
{
    struct sockaddr_in SrvAddr;
    struct hostent *Server;

    if ((*SockFD = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        return false;
    }

    Server = gethostbyname("localhost");
    SrvAddr.sin_family = AF_INET;
    SrvAddr.sin_port = htons(Port);
    memcpy(&SrvAddr.sin_addr.s_addr, Server->h_addr, Server->h_length);
    memset(&SrvAddr.sin_zero, '\0', 8);

    return connect(*SockFD, (struct sockaddr*) &SrvAddr, sizeof(struct sockaddr)) != -1;
}

bool StartDaemon(int Port, daemon_callback *Callback)
{
    ConnectionCallback = Callback;

    struct sockaddr_in SrvAddr;
    int _True = 1;

    if ((DaemonSockFD = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        return false;
    }

    if (setsockopt(DaemonSockFD, SOL_SOCKET, SO_REUSEADDR, &_True, sizeof(int)) == -1) {
        printf("Could not set socket option: SO_REUSEADDR!\n");
    }

    SrvAddr.sin_family = AF_INET;
    SrvAddr.sin_port = htons(Port);
    SrvAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    memset(&SrvAddr.sin_zero, '\0', 8);

    if (bind(DaemonSockFD, (struct sockaddr*)&SrvAddr, sizeof(struct sockaddr)) == -1) {
        return false;
    }

    if (listen(DaemonSockFD, 10) == -1) {
        return false;
    }

    IsRunning = true;
    pthread_create(&Thread, NULL, &HandleConnection, NULL);
    return true;
}

void StopDaemon()
{
    if (IsRunning) {
        IsRunning = false;
        CloseSocket(DaemonSockFD);
        DaemonSockFD = 0;
    }
}
