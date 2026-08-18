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
    User user = {"", GUEST};
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

    int user_input;
    Handeler operation_handler = {OPCODE_UNDEFINED, NULL};
    size_t operation_count = sizeof(HANDLERS) / sizeof(HANDLERS[0]);
    print_info(user.user_type);
    // Main loop for user interaction
    do {
        operation_handler.opcode = OPCODE_UNDEFINED;
        printf("\nEnter input: ");
        scanf("%d", &user_input);
        flush_stdin();          // Clear the input buffer
        if (user_input == -1) { // Exit the application
            printf("Exiting the application.\n");
            break;
        }
        if (user_input == 0) { // Print application information
            print_info(user.user_type);
            continue;
        }
        // Find the corresponding handler for the user input
        for (size_t i = 0; i < operation_count; i++) {
            if (HANDLERS[i].opcode == user_input) {
                operation_handler = HANDLERS[i];
                break;
            }
        }
        // if invalid input, print error message and continue the loop
        if (operation_handler.opcode == OPCODE_UNDEFINED) {
            printf("Invalid input. Please try again.\n");
            continue;
        }
        if (operation_handler.handler == NULL) {
            printf("No handler defined for this operation. Please try again.\n");
            continue;
        }
        // Call the handler function for the selected operation
        operation_handler.handler(socket_fd, &user);
    } while (user_input != -1);
    close(socket_fd);

    return 0;
}
