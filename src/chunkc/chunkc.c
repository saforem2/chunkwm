#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libproc.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

// NOTE(koekeishiya): 4131 is the port used by chwm tiling plugin.
#define FALLBACK_PORT 4131

int main(int Count, char **Argv)
{
    if(Count < 2)
    {
        fprintf(stderr, "chunkc: no arguments found!\n");
        exit(1);
    }

    int Port;

    char *PortEnv = getenv("CHUNKC_SOCKET");
    if(PortEnv)
    {
        sscanf(PortEnv, "%d", &Port);
    }
    else
    {
        Port = FALLBACK_PORT;
    }

    int SockFD;
    struct sockaddr_in srv_addr;
    struct hostent *server;

    if((SockFD = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "chunkc: could not create socket!\n");
        exit(1);
    }

    server = gethostbyname("localhost");
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(Port);
    memcpy(&srv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    memset(&srv_addr.sin_zero, '\0', 8);

    if(connect(SockFD, (struct sockaddr*) &srv_addr, sizeof(struct sockaddr)) == -1)
    {
        fprintf(stderr, "chunkc: connection failed!\n");
        exit(1);
    }

    size_t CommandLength = Count - 1;
    size_t ArgLength[Count];

    for(size_t Index = 1; Index < Count; ++Index)
    {
        ArgLength[Index] = strlen(Argv[Index]);
        CommandLength += ArgLength[Index];
    }

    char Command[CommandLength];
    char *Temp = Command;

    for(size_t Index = 1; Index < Count; ++Index)
    {
        memcpy(Temp, Argv[Index], ArgLength[Index]);
        Temp += ArgLength[Index];
        *Temp++ = ' ';
    }
    *(Temp - 1) = '\0';

    if(send(SockFD, Command, CommandLength, 0) == -1)
    {
        fprintf(stderr, "chunkc: failed to send data!\n");
    }

    // TODO(koekeishiya): Take a closer look at this later.
#if 0
    char Response[BUFSIZ];
    int BytesRead;

    while((BytesRead = recv(SockFD, Response, sizeof(Response) - 1, 0)) > 0)
    {
        Response[BytesRead] = '\0';
        printf("%s", Response);
        fflush(stdout);
    }
#endif

    shutdown(SockFD, SHUT_RDWR);
    close(SockFD);

    return 0;
}
