#include "client_utils.h"
#include "./lib/configuration.h"
#include "./lib/protocol.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>

struct sockaddr_in initialize_client() {
    FILE *settings_file = fopen(CLIENT_SETTINGS_FILE_NAME, "r");
    struct sockaddr_in server_address = {0};
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(DEFAULT_SERVER_ADDRESS);
    server_address.sin_port = htons(DEFAULT_SERVER_PORT);
    if (settings_file != NULL) {
        char ip_address[IP_STRING_SIZE];
        int port;
        if (fscanf(settings_file, "%15s:%d", ip_address, &port) == 2) {
            server_address.sin_addr.s_addr = inet_addr(ip_address);
            server_address.sin_port = htons(port);
        }
        fclose(settings_file);
    }
    return server_address;
}

void print_error_and_exit(const char *error_message, int error_code) {
    if (error_code == 0) {
        fprintf(stderr, "Error: %s\n", error_message);
        exit(EXIT_FAILURE);
    }
    perror(error_message);
    exit(error_code);
}

int login(LoginCredentials *credentials) {
    // Implement the login logic here
    // For now, just return 0 to indicate success
    return 0;
}

int signup(LoginCredentials *credentials, UserType user_type) {
    // Implement the signup logic here
    // For now, just return 0 to indicate success
    return 0;
}