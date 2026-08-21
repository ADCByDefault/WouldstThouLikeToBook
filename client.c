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
    User user = {"guest", GUEST};
    char buffer[MAX_BUFFER_SIZE];

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

    int user_input;
    int operation_count = sizeof(HANDLERS) / sizeof(HANDLERS[0]);
    int read_count = 0;
    print_application_info(user);
    // Main loop for user interaction
    do {
        user_input = 0;
        printf("\n/////////////////////////////////////////////////////");
        printf("\nEnter an OPCODE: ");
        fgets(buffer, sizeof(buffer), stdin);
        if (strcasestr(buffer, "help") != NULL) {
            print_can_do_operations_by_type(user.user_type);
            continue;
        }
        read_count = sscanf(buffer, "%d", &user_input);
        if (read_count != 1) {
            print_error("Invalid input.\n");
            continue;
        }
        if (user_input == -1) {
            print_info("Exiting the application.\n");
            break;
        }
        if (user_input == 0) {
            print_application_info(user);
            continue;
        }
        if (is_valid_opcode(user_input) == false) {
            print_error("Invalid OPCODE.\n");
            continue;
        }
        if (!is_valid_opcode_for_user_by_type(user_input, user.user_type)) {
            print_error("Invalid OPCODE for your user type.\n");
            continue;
        }
        for (int i = 0; i < operation_count; i++) {
            if (HANDLERS[i].opcode == user_input) {
                HANDLERS[i].handler(socket_fd, &user);
                break;
            }
        }
        flush_stdin(); // Clear any remaining input in stdin
    } while (user_input != -1);
    close(socket_fd);

    return 0;
}
