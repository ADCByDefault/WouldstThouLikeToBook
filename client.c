#include "./lib/configuration.h"
#include "./lib/protocol.h"
#include "client_utils.h"
#include <arpa/inet.h>
#include <error.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char const *argv[]) {
    // Initialize client settings
    struct sockaddr_in server_address = initialize_client();
    char buffer[MAX_BUFFER_SIZE];
    bool is_logged_in = false;

    // Create socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        print_error_and_exit("Socket creation failed", CLIENT_ERROR_SOCKET_CREATION);
    }

    // Connect to server
    if (connect(socket_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        close(socket_fd);
        print_error_and_exit("Connection to server failed", CLIENT_ERROR_CONNECTION);
    }

    // lets test if client and server_child exchange messages
    fgets(buffer, MAX_BUFFER_SIZE, stdin);
    printf("Sent to server_child: %s\n", buffer);
    write(socket_fd, buffer, MAX_BUFFER_SIZE);
    read(socket_fd, buffer, MAX_BUFFER_SIZE);
    printf("Received from server_child: %s\n", buffer);

    return 0;
}
