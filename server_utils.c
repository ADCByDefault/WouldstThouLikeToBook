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

const Handeler HANDLERS[2] = {
    {OPCODE_LOGIN, handle_login},
    {OPCODE_SIGNUP, handle_signup},
    // Add more handlers as needed
};

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

bool lock_writing_for_file(FILE *file) {
    if (file == NULL) {
        return false;
    }

    struct flock lock = {0};
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    return fcntl(fileno(file), F_SETLKW, &lock) == 0;
}

void unlock_writing_for_file(FILE *file) {
    if (file == NULL) {
        return;
    }

    fflush(file);

    struct flock unlock = {0};
    unlock.l_type = F_UNLCK;
    unlock.l_whence = SEEK_SET;
    unlock.l_start = 0;
    unlock.l_len = 0;

    fcntl(fileno(file), F_SETLK, &unlock);
}

User login(LoginCredentials credentials) {
    User_Save user_save = {0};
    User user = {0};
    FILE *users_file = fopen(USERS_FILE_NAME, "rb");
    fseek(users_file, 0, SEEK_SET);
    if (users_file == NULL) {
        return user; // Error opening users file
    }
    while (fread(&user_save, sizeof(User_Save), 1, users_file) == 1) {
        if (strcmp(user_save.username, credentials.username) == 0) {
            if (strcmp(user_save.password, credentials.password) != 0) {
                fclose(users_file);
                return user; // Incorrect password, return user with empty username
            }
            snprintf(user.username, USERNAME_MAX_LENGTH, "%s", user_save.username);
            user.user_type = user_save.user_type;
            fclose(users_file);
            return user; // Login successful
        }
    }
    fclose(users_file);
    return user; // Login failed, return user with empty username
}
User signup(LoginCredentials credentials, UserType user_type) {
    User_Save user_save = {0};
    User user = {0}; // Initialize user with empty username
    FILE *users_file = fopen(USERS_FILE_NAME, "ab+");
    if (users_file == NULL) {
        return user; // Error opening users file
    }

    if (!lock_writing_for_file(users_file)) {
        fclose(users_file);
        return user; // Error locking users file
    }

    fseek(users_file, 0, SEEK_SET);
    while (fread(&user_save, sizeof(User_Save), 1, users_file) == 1) {
        if (strcmp(user_save.username, credentials.username) == 0) {
            unlock_writing_for_file(users_file);
            fclose(users_file);
            return user; // Username already exists, return user with empty username
        }
    }

    // Add new user to the users file
    snprintf(user_save.username, USERNAME_MAX_LENGTH, "%s", credentials.username);
    snprintf(user_save.password, PASSWORD_MAX_LENGTH, "%s", credentials.password);
    user_save.user_type = user_type;
    fwrite(&user_save, sizeof(User_Save), 1, users_file);
    unlock_writing_for_file(users_file);
    fclose(users_file);
    snprintf(user.username, USERNAME_MAX_LENGTH, "%s", user_save.username);
    user.user_type = user_save.user_type;
    return user; // Signup successful
}

void handle_login(int socket_fd, User *user, char *payload_buffer, size_t payload_size) {
    if (payload_size == 0 || payload_buffer == NULL) {
        print_error_and_exit("Invalid payload for login operation", SERVER_CHILD_ERROR_READ);
    }
    LoginCredentials credentials = parse_credentials(payload_buffer);
    User logged_in_user = login(credentials);
    Header response_header = {0};
    if (strlen(logged_in_user.username) == 0 || strcmp(logged_in_user.username, credentials.username) != 0) {
        response_header.operation = OPCODE_LOGIN_ERROR;
        response_header.payload_size = 0;
        send_header_and_payload(socket_fd, &response_header, NULL);
        return;
    }
    *user = logged_in_user;
    char response_payload[MAX_BUFFER_SIZE];
    size_t response_payload_size = to_string_user(response_payload, MAX_BUFFER_SIZE, user);
    response_header.operation = OPCODE_OK;
    response_header.payload_size = response_payload_size;
    send_header_and_payload(socket_fd, &response_header, response_payload);
}
void handle_signup(int socket_fd, User *user, char *payload_buffer, size_t payload_size) {
    if (payload_size == 0 || payload_buffer == NULL) {
        print_error_and_exit("Invalid payload for signup operation", SERVER_CHILD_ERROR_READ);
    }
    LoginCredentials credentials = parse_credentials(payload_buffer);
    User signed_up_user = signup(credentials, USER);
    Header response_header = {0};
    if (strlen(signed_up_user.username) == 0 || strcmp(signed_up_user.username, credentials.username) != 0) {
        response_header.operation = OPCODE_SIGNUP_ERROR;
        response_header.payload_size = 0;
        send_header_and_payload(socket_fd, &response_header, NULL);
        return;
    }
    *user = signed_up_user;
    char response_payload[MAX_BUFFER_SIZE];
    size_t response_payload_size = to_string_user(response_payload, MAX_BUFFER_SIZE, user);
    response_header.operation = OPCODE_OK;
    response_header.payload_size = response_payload_size;
    send_header_and_payload(socket_fd, &response_header, response_payload);
}

// EOF