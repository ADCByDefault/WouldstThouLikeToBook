#include "./lib/configuration.h"
#include "./lib/protocol.h"
#include "./server_utils.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char const *argv[]) {
    int socket_fd = SERVER_CHILD_DEFAULT_SOCKET_FD;
    char buffer[MAX_BUFFER_SIZE];
    User user = {"", "", GUEST};
    if (argc != 2) {
        print_error_and_exit("Invalid number of arguments for server_child", SERVER_CHILD_INVALID_ARGUMENTS);
    }
    if (fcntl(SERVER_CHILD_DEFAULT_SOCKET_FD, F_GETFD) == -1) {
        print_error_and_exit("Invalid socket file descriptor for server_child", SERVER_CHILD_ERROR_SOCKET_FD);
    }

    read(socket_fd, buffer, MAX_BUFFER_SIZE);
    printf("Received from client: %s\n", buffer);
    fgets(buffer, MAX_BUFFER_SIZE, stdin);
    printf("Sent to client: %s\n", buffer);
    write(socket_fd, buffer, MAX_BUFFER_SIZE);

    return 0;
}
