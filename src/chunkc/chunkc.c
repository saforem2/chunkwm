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

#define SOCKET_PATH_FMT "/tmp/chunkwm_%s-socket"

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "chunkc: no arguments found!\n");
        exit(1);
    }

    char *user = getenv("USER");
    if (!user) {
        fprintf(stderr, "chunkc: could not read env USER.\n");
        exit(1);
    }

    int sock_fd;
	struct sockaddr_un sock_address;
	sock_address.sun_family = AF_UNIX;

	if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "chunkc: could not create socket!\n");
        exit(1);
	}

    snprintf(sock_address.sun_path, sizeof(sock_address.sun_path), SOCKET_PATH_FMT, user);

	if (connect(sock_fd, (struct sockaddr *) &sock_address, sizeof(sock_address)) == -1) {
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
