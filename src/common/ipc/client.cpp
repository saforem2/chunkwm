#include "client.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

// NOTE(koekeishiya): Caller frees memory.
char *ReadFromSocket(int SockFD)
{
    int Length = 256;
    char *Result = (char *) malloc(Length);

    Length = recv(SockFD, Result, Length, 0);
    if(Length > 0)
    {
        Result[Length] = '\0';
    }
    else
    {
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

bool ConnectToDaemon(int *SockFD, int Port)
{
    struct sockaddr_in SrvAddr;
    struct hostent *Server;

    if((*SockFD = socket(PF_INET, SOCK_STREAM, 0)) == -1)
        return false;

    Server = gethostbyname("localhost");
    SrvAddr.sin_family = AF_INET;
    SrvAddr.sin_port = htons(Port);
    memcpy(&SrvAddr.sin_addr.s_addr, Server->h_addr, Server->h_length);
    memset(&SrvAddr.sin_zero, '\0', 8);

    return connect(*SockFD, (struct sockaddr*) &SrvAddr, sizeof(struct sockaddr)) != -1;
}
