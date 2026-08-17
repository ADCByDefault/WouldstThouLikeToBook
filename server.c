#include "./lib/configuration.h"
#include "./lib/protocol.h"
#include "server_utils.h"
#include <arpa/inet.h>
#include <error.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char const *argv[]) {
    // Initialize server files and settings
    printf("Starting server...\n");
    struct sockaddr_in server_address = initialize_server();
    if (server_address.sin_family == 0) {
        print_error_and_exit("Failed to initialize server", SERVER_INITIALIZATION_ERROR);
    }

    // Create server socket
    int server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_fd < 0) {
        print_error_and_exit("Failed to create server socket", SERVER_ERROR_SOCKET_CREATION);
    }

    printf("Server socket created successfully: %d\n", server_socket_fd);

    // Bind and listening server socket to address and port
    if (bind(server_socket_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        print_error_and_exit("Failed to bind server socket", SERVER_ERROR_BIND);
    }
    if (listen(server_socket_fd, 5) < 0) {
        print_error_and_exit("Failed to listen on server socket", SERVER_ERROR_LISTEN);
    }
    printf("Server is listening on %s:%d\n", inet_ntoa(server_address.sin_addr), ntohs(server_address.sin_port));

    // Handle SIGCHLD to prevent zombie processes
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        print_error_and_exit("Failed to set SIGCHLD handler", SERVER_ERROR_FAILED_IGNORE_SIGCHLD);
    }

    // Accept and handle client connections
    while (true) {
        struct sockaddr_in client_address;
        socklen_t client_address_length = sizeof(client_address);
        int client_socket_fd = accept(server_socket_fd, (struct sockaddr *)&client_address, &client_address_length);
        if (client_socket_fd < 0) {
            perror("Failed to accept client connection");
            continue;
        }
        int pid = fork();
        if (pid < 0) {
            perror("Failed to fork process");
            close(client_socket_fd);
            continue;
        }
        if (pid == 0) {
            // child process
            close(server_socket_fd);
            if (!setup_for_child_process(client_socket_fd)) {
                print_error_and_exit("Failed to set up child process", SERVER_ERROR_CHILD_CREATION);
            }
            char client_address_str[ADDRESS_STRING_SIZE];
            snprintf(client_address_str, ADDRESS_STRING_SIZE, "%s:%d", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
            printf("Accepted connection from %s\n", client_address_str);
            if (execl("./server_child", "./server_child", client_address_str, NULL) < 0) {
                print_error_and_exit("Failed to execute child process", SERVER_ERROR_CHILD_CREATION);
            }
            close(client_socket_fd);
            exit(EXIT_SUCCESS);
        }
        // parent process
        close(client_socket_fd);
    }
    return 0;
}
