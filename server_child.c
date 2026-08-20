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
    while (1) {
        bytes_read = read_exact(socket_fd, &header, HEADER_SIZE);
        if (bytes_read < 0) {
            print_error_and_exit("Failed to read from client. Terminating.", SERVER_CHILD_ERROR_READ);
        }
        if (bytes_read != HEADER_SIZE) {
            print_error_and_exit("Failed to read complete header from client. Terminating.", SERVER_CHILD_ERROR_READ);
        }
        header = header_ntoh(header);
        printf("From client operation=%d,size=%d\n", header.operation, header.payload_size);
        if (header.operation == OPCODE_UNDEFINED) {
            if (header.payload_size == 0) {
                printf("Undefined operation received with zero payload size.\n");
                continue;
            } else {
                print_error_and_exit("Received undefined operation code with non-zero payload size from client. Terminating.",
                                     SERVER_CHILD_ERROR_READ);
            }
            print_error_and_exit("Received undefined operation code from client. Terminating.", SERVER_CHILD_ERROR_READ);
        }
        if (!is_valid_opcode_from_client(header.operation)) {
            print_error_and_exit("Received invalid operation code from client. Terminating.", SERVER_CHILD_ERROR_READ);
        }
        for (int i = 0; i < operation_count; i++) {
            if (HANDLERS[i].opcode == header.operation) {
                HANDLERS[i].handler(socket_fd, &user, header);
                break;
            }
        }
    }

    printf("Client disconnected. Terminating.\n");
    close(socket_fd);

    return 0;
}
