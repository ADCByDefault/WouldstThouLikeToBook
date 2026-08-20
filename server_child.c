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
    User user = {"guest", GUEST};
    if (argc != 2) {
        print_error_and_exit("Invalid number of arguments for server_child", SERVER_CHILD_INVALID_ARGUMENTS);
    }
    if (fcntl(SERVER_CHILD_DEFAULT_SOCKET_FD, F_GETFD) == -1) {
        print_error_and_exit("Invalid socket file descriptor for server_child", SERVER_CHILD_ERROR_SOCKET_FD);
    }

    int bytes_read;
    int operation_count = sizeof(HANDLERS) / sizeof(HANDLERS[0]);
    Header header = {0};
    bool operation_found = false;
    while (1) {
        bytes_read = read_exact(socket_fd, &header, HEADER_SIZE);
        if (bytes_read == 0) {
            break; // Client disconnected
        }
        if (bytes_read != HEADER_SIZE) {
            print_error_and_exit("Failed to read complete header from client. Terminating.", SERVER_CHILD_ERROR_READ);
        }
        header = header_ntoh(header);
        printf("From client operation=%d,payload_size=%d\n", header.operation, header.payload_size);
        for (int i = 0; i < operation_count; i++) {
            if (HANDLERS[i].opcode == header.operation) {
                HANDLERS[i].handler(socket_fd, &user, header);
                operation_found = true;
                break;
            }
        }
        if (!operation_found) {
            printf("Unknown operation code received: %d\n", header.operation);
            Header response_header = {0};
            response_header.operation = OPCODE_ERROR;
            response_header.payload_size = 0;
            send_header_and_payload(socket_fd, response_header, NULL);
        }
        operation_found = false;
    }

    printf("Closed connection from client %s\n", argv[1]);
    close(socket_fd);

    return 0;
}
