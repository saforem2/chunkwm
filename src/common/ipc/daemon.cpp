#include "daemon.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
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
        int SockFD = accept(DaemonSockFD, NULL, 0);
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

bool ConnectToDaemon(int *SockFD, char *SocketPath)
{
    struct sockaddr_un SockAddress;
	SockAddress.sun_family = AF_UNIX;

	if ((*SockFD = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        return false;
	}

    snprintf(SockAddress.sun_path, sizeof(SockAddress.sun_path), "%s", SocketPath);

	return connect(*SockFD, (struct sockaddr *) &SockAddress, sizeof(SockAddress)) != -1;
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

bool StartDaemon(char *SocketPath, daemon_callback *Callback)
{
    ConnectionCallback = Callback;

	struct sockaddr_un SockAddress;
    SockAddress.sun_family = AF_UNIX;

    snprintf(SockAddress.sun_path, sizeof(SockAddress.sun_path), "%s", SocketPath);

    if ((DaemonSockFD = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        return false;
    }

    unlink(SocketPath);

    if (bind(DaemonSockFD, (struct sockaddr *) &SockAddress, sizeof(SockAddress)) == -1) {
        return false;
    }

    if (chmod(SocketPath, 0600) != 0) {
        return false;
    }

    if (listen(DaemonSockFD, SOMAXCONN) == -1) {
        return false;
    }

    IsRunning = true;
    pthread_create(&Thread, NULL, &HandleConnection, NULL);
    return true;
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
