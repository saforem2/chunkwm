#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libproc.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>

// NOTE(koekeishiya): 3920 is the port used by chunkwm.
#define FALLBACK_PORT 3920

int main(int Argc, char **Argv)
{
    if (Argc < 2) {
        fprintf(stderr, "chunkc: no arguments found!\n");
        exit(1);
    }

    int Port;

    char *PortEnv = getenv("CHUNKC_SOCKET");
    if (PortEnv) {
        sscanf(PortEnv, "%d", &Port);
    } else {
        Port = FALLBACK_PORT;
    }

    int SockFD;
    struct sockaddr_in SrvAddr;
    struct hostent *Server;

    if ((SockFD = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "chunkc: could not create socket!\n");
        exit(1);
    }

    Server = gethostbyname("localhost");
    SrvAddr.sin_family = AF_INET;
    SrvAddr.sin_port = htons(Port);
    memcpy(&SrvAddr.sin_addr.s_addr, Server->h_addr, Server->h_length);
    memset(&SrvAddr.sin_zero, '\0', 8);

    if (connect(SockFD, (struct sockaddr*) &SrvAddr, sizeof(struct sockaddr)) == -1) {
        fprintf(stderr, "chunkc: connection failed!\n");
        exit(1);
    }

    size_t MessageLength = Argc - 1;
    size_t Argl[Argc];

    for (size_t Index = 1; Index < Argc; ++Index) {
        Argl[Index] = strlen(Argv[Index]);
        MessageLength += Argl[Index];
    }

    char Message[MessageLength];
    char *Temp = Message;

    for (size_t Index = 1; Index < Argc; ++Index) {
        memcpy(Temp, Argv[Index], Argl[Index]);
        Temp += Argl[Index];
        *Temp++ = ' ';
    }
    *(Temp - 1) = '\0';

    if (send(SockFD, Message, MessageLength, 0) == -1) {
        fprintf(stderr, "chunkc: failed to send data!\n");
    } else {
        // NOTE(koekeishiya): Should we read and print a response ??
        struct timeval TimeVal;
        fd_set ReadFDs;

        TimeVal.tv_sec = 0;
        TimeVal.tv_usec = 10000;

        FD_ZERO(&ReadFDs);
        FD_SET(SockFD, &ReadFDs);

        select(SockFD + 1, &ReadFDs, NULL, NULL, &TimeVal);
        if (FD_ISSET(SockFD, &ReadFDs)) {
            char Response[BUFSIZ];
            int BytesRead;

            while ((BytesRead = recv(SockFD, Response, sizeof(Response) - 1, 0)) > 0) {
                Response[BytesRead] = '\0';
                printf("%s", Response);
                fflush(stdout);
            }
        }
    }

    shutdown(SockFD, SHUT_RDWR);
    close(SockFD);

    return 0;
}
