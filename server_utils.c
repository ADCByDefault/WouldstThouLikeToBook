#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "./lib/configuration.h"
#include "./lib/protocol.h"
#include "server_utils.h"

struct sockaddr_in initialize_server() {
    FILE *users_file = fopen(USERS_FILE_NAME, "ab");
    if (users_file == NULL) {
        return (struct sockaddr_in){0}; // Error creating users file
    }
    fclose(users_file);

    FILE *rooms_file = fopen(ROOMS_FILE_NAME, "ab");
    if (rooms_file == NULL) {
        return (struct sockaddr_in){0}; // Error creating rooms file
    }
    fclose(rooms_file);

    FILE *bookings_file = fopen(BOOKINGS_FILE_NAME, "ab");
    if (bookings_file == NULL) {
        return (struct sockaddr_in){0}; // Error creating bookings file
    }
    fclose(bookings_file);

    FILE *settings_file = fopen(SERVER_SETTINGS_FILE_NAME, "r");
    struct sockaddr_in server_address = {0};
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = DEFAULT_SERVER_ADDRESS;
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

bool setup_for_child_process(int socket_fd) {
    // setting up file descriptor for child process
    if (socket_fd < 0) {
        return false;
    }
    if (socket_fd == SERVER_CHILD_DEFAULT_SOCKET_FD) {
        return true;
    }
    if (dup2(socket_fd, SERVER_CHILD_DEFAULT_SOCKET_FD) < 0) {
        return false;
    }
    close(socket_fd);
    return true;
}

int login(LoginCredentials *credentials) {
    FILE *file = fopen(USERS_FILE_NAME, "rb");
    if (file == NULL) {
        return -200; // Error opening file
    }
    User user;
    while (fread(&user, sizeof(User), 1, file) == 1) {
        if (strcmp(user.username, credentials->username) == 0) {
            if (strcmp(user.password, credentials->password) == 0) {
                fclose(file);
                return (strcmp(user.username, "superuser") == 0) ? 1 : 0; // Logged in as superuser or user
            } else {
                fclose(file);
                return -2; // Incorrect password
            }
        }
    }

    fclose(file);
    return -1; // User not found
}

int signup(LoginCredentials *credentials, UserType user_type) {
    FILE *file = fopen(USERS_FILE_NAME, "ab+");
    if (file == NULL) {
        return -200; // Error opening file
    }
    fseek(file, 0, SEEK_SET);
    User user;
    while (fread(&user, sizeof(User), 1, file) == 1) {
        if (strcmp(user.username, credentials->username) == 0) {
            fclose(file);
            return -1; // User already exists
        }
    }

    // Add new user
    User new_user;
    strncpy(new_user.username, credentials->username, USERNAME_MAX_LENGTH);
    strncpy(new_user.password, credentials->password, PASSWORD_MAX_LENGTH);
    new_user.user_type = user_type;
    fwrite(&new_user, sizeof(User), 1, file);
    fclose(file);
    return 0; // Signup successful
}

int signup_user(LoginCredentials *credentials) { return signup(credentials, USER); }

int signup_superuser(LoginCredentials *credentials) { return signup(credentials, SUPERUSER); }
