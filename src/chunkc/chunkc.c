#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libproc.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>

// NOTE(koekeishiya): 3920 is the port used by chunkwm.
#define FALLBACK_PORT 3920

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "chunkc: no arguments found!\n");
        exit(1);
    }

    int port;

    char *port_env = getenv("CHUNKC_SOCKET");
    if (port_env) {
        sscanf(port_env, "%d", &port);
    } else {
        port = FALLBACK_PORT;
    }

    int sock_fd;
    struct sockaddr_in srv_addr;
    struct hostent *server;

    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "chunkc: could not create socket!\n");
        exit(1);
    }

    server = gethostbyname("localhost");
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);
    memcpy(&srv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    memset(&srv_addr.sin_zero, '\0', 8);

    if (connect(sock_fd, (struct sockaddr*) &srv_addr, sizeof(struct sockaddr)) == -1) {
        fprintf(stderr, "chunkc: connection failed!\n");
        exit(1);
    }

    size_t message_length = argc - 1;
    size_t argl[argc];

    for (size_t i = 1; i < argc; ++i) {
        argl[i] = strlen(argv[i]);
        message_length += argl[i];
    }

    char message[message_length];
    char *temp = message;

    for (size_t i = 1; i < argc; ++i) {
        memcpy(temp, argv[i], argl[i]);
        temp += argl[i];
        *temp++ = ' ';
    }
    *(temp - 1) = '\0';

    if (send(sock_fd, message, message_length, 0) == -1) {
        fprintf(stderr, "chunkc: failed to send data!\n");
    } else {
        int num_bytes;
        char response[BUFSIZ];
        struct pollfd fds[] = {
            { sock_fd, POLLIN, 0 },
            { STDOUT_FILENO, POLLHUP, 0 },
        };

        while (poll(fds, 2, -1) > 0) {
            if (fds[1].revents & (POLLERR | POLLHUP)) {
                break;
            }
            if (fds[0].revents & POLLIN) {
                if ((num_bytes = recv(sock_fd, response, sizeof(response) - 1, 0)) > 0) {
                    response[num_bytes] = '\0';
                    printf("%s", response);
                    fflush(stdout);
                } else {
                    break;
                }
            }
        }
    }

    shutdown(sock_fd, SHUT_RDWR);
    close(sock_fd);

    return 0;
}
