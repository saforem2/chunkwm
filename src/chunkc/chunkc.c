#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libproc.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

int main(int Count, char **Argv)
{
    size_t Length = 0;
    char *Temp, *Command;

    for(unsigned Index = 1; Index < Count; ++Index)
    {
        Length += strlen(Argv[Index]);
    }

    Temp = Command = (char *) malloc(Length + Count - 1);

    for(unsigned Index = 1; Index < Count; ++Index)
    {
        memcpy(Temp, Argv[Index], strlen(Argv[Index]));
        Temp += strlen(Argv[Index]);
        *Temp++ = ' ';
    }
    *(Temp - 1) = '\0';

    char *PortEnv = getenv("CHUNKC_SOCKET");
    if(PortEnv)
    {
        int SockFD, Port;
        struct sockaddr_in srv_addr;
        struct hostent *server;

        sscanf(PortEnv, "%d", &Port);

        if((SockFD = socket(PF_INET, SOCK_STREAM, 0)) == -1)
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

        if(send(SockFD, Command, strlen(Command), 0) == -1)
        {
            fprintf(stderr, "chunkc: failed to send data!\n");
        }

        free(Command);
        shutdown(SockFD, SHUT_RDWR);
        close(SockFD);
    }
    else
    {
        fprintf(stderr, "chunkc: 'env CHUNKC_SOCKET' not set!\n");
    }

    return 0;
}
