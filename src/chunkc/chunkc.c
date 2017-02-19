#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#include <libproc.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

int SockFD;
int Port;
char *Command;

void Fatal(const char *Err)
{
    fprintf(stderr, "chunkc: %s\n", Err);
    exit(1);
}

void CloseSocket()
{
    shutdown(SockFD, SHUT_RDWR);
    close(SockFD);
}

void WriteToSocket(const char *Message)
{
    send(SockFD, Message, strlen(Message), 0);
}

void ConnectToDaemon()
{
    struct sockaddr_in srv_addr;
    struct hostent *server;

    if((SockFD = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        Fatal("could not create socket!");
    }

    server = gethostbyname("localhost");
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(Port);
    memcpy(&srv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    memset(&srv_addr.sin_zero, '\0', 8);

    if(connect(SockFD, (struct sockaddr*) &srv_addr, sizeof(struct sockaddr)) == -1)
    {
        Fatal("connection failed!");
    }
}

int ParseArguments(int Count, char **Args)
{
    int Option;
    const char *Short = "p:c:";
    struct option Long[] =
    {
        { "port", required_argument, NULL, 'p' },
        { "command", required_argument, NULL, 'c' },
        { NULL, 0, NULL, 0 }
    };

    while((Option = getopt_long(Count, Args, Short, Long, NULL)) != -1)
    {
        switch(Option)
        {
            case 'p':
            {
                sscanf(optarg, "%d", &Port);
            } break;
            case 'c':
            {
                Command = strdup(optarg);
            } break;
        }
    }

    return 0;
}

int main(int Count, char **Argv)
{
    if(Count < 3)
    {
        Fatal("usage: ./chunkc -p PORT -c \"COMMAND\"\n");
    }

    ParseArguments(Count, Argv);

    ConnectToDaemon();
    if(Command)
    {
        WriteToSocket(Command);
        free(Command);
    }

    CloseSocket();
    return 0;
}
