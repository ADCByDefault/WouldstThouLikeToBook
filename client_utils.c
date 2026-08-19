#include "client_utils.h"
#include "./lib/configuration.h"
#include "./lib/protocol.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>

const Handeler HANDLERS[2] = {
    {OPCODE_LOGIN, handle_login},
    {OPCODE_SIGNUP, handle_signup},
    // Add more handlers as needed
};

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
void flush_stdin() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
    }
}

void print_info(User user) {
    UserType user_type = user.user_type;
    printf("Client Application Information:\n");
    printf("Enter -1 to exit the application.\n");
    printf("Enter 0 to print this information.\n");
    if (user_type == GUEST) {
        printf("You are currently logged in as a GUEST.\n");
    }
    if (user_type == USER || user_type == SUPERUSER) {
        printf("You are logged in as %s with type %d\n", user.username, user_type);
    }
    print_can_do_operations_by_type(user_type);
}
void print_guest_can_do_operations() {
    size_t num_opcodes = sizeof(GUEST_OPCODES) / sizeof(GUEST_OPCODES[0]);
    printf("Allowed operations for GUEST:\n");
    for (size_t i = 0; i < num_opcodes; i++) {
        const char *description = get_opcode_description(GUEST_OPCODES[i]);
        printf(" %u. %s\n", GUEST_OPCODES[i], description);
    }
}
void print_user_can_do_operations() {
    size_t num_opcodes = sizeof(USER_OPCODES) / sizeof(USER_OPCODES[0]);
    printf("Allowed operations for USER:\n");
    for (size_t i = 0; i < num_opcodes; i++) {
        const char *description = get_opcode_description(USER_OPCODES[i]);
        printf(" %u. %s\n", USER_OPCODES[i], description);
    }
}
void print_superuser_can_do_operations() {
    size_t num_opcodes = sizeof(SUPERUSER_OPCODES) / sizeof(SUPERUSER_OPCODES[0]);
    printf("Allowed operations for SUPERUSER:\n");
    for (size_t i = 0; i < num_opcodes; i++) {
        const char *description = get_opcode_description(SUPERUSER_OPCODES[i]);
        printf(" %u. %s\n", SUPERUSER_OPCODES[i], description);
    }
}
void print_can_do_operations_by_type(UserType user_type) {
    switch (user_type) {
    case GUEST:
        print_guest_can_do_operations();
        break;
    case USER:
        print_user_can_do_operations();
        break;
    case SUPERUSER:
        print_superuser_can_do_operations();
        break;
    default:
        fprintf(stderr, "Unknown user type: %d\n", user_type);
    }
}

void handle_login(int socket_fd, User *user) {
    // getting username and password from user input
    LoginCredentials credentials = {0};
    char username[USERNAME_MAX_LENGTH];
    char password[PASSWORD_MAX_LENGTH];
    printf("Enter username: ");
    fgets(username, USERNAME_MAX_LENGTH, stdin);
    printf("Enter password: ");
    fgets(password, PASSWORD_MAX_LENGTH, stdin);
    // send login request to server
    credentials = sanitize_credentials(username, password);
    char payload_buffer[MAX_BUFFER_SIZE];
    size_t payload_size = to_string_credentials(payload_buffer, MAX_BUFFER_SIZE, credentials);
    Header header = {OPCODE_LOGIN, payload_size};
    size_t bytes_sent = send_header_and_payload(socket_fd, header, payload_buffer);
    if (bytes_sent < HEADER_SIZE + payload_size) {
        print_error_and_exit("Failed to send complete header and payload to server. Terminating.", CLIENT_ERROR_WRITE);
    }
    char response_buffer[MAX_BUFFER_SIZE];
    int bytes_read = read_exact(socket_fd, response_buffer, HEADER_SIZE);
    if (bytes_read < HEADER_SIZE) {
        print_error_and_exit("Failed to read response header from server. Terminating.", CLIENT_ERROR_READ);
    }
    // Response (sending is successful)
    Header response_header = parse_header(response_buffer);
    if (response_header.operation == OPCODE_LOGIN_ERROR) {
        printf("login failed, please check your username and password\n");
        return;
    }
    if (response_header.operation != OPCODE_OK || response_header.payload_size == 0) {
        print_error_and_exit("Received unexpected response from server. Terminating.", CLIENT_ERROR_READ);
    }
    bytes_read = read_exact(socket_fd, payload_buffer, response_header.payload_size);
    if (bytes_read < response_header.payload_size) {
        print_error_and_exit("Failed to read complete response payload from server. Terminating.", CLIENT_ERROR_READ);
    }
    *user = parse_user(payload_buffer);
    printf("Login successful. Logged in as: %s with type: %d\n", user->username, user->user_type);
}
void handle_signup(int socket_fd, User *user) {
    // getting username and password from user input
    LoginCredentials credentials = {0};
    char username[USERNAME_MAX_LENGTH];
    char password[PASSWORD_MAX_LENGTH];
    printf("Username and password must be between %d and %d characters long.\n", USERNAME_MIN_LENGTH, USERNAME_MAX_LENGTH - 1);
    printf("Enter username: ");
    fgets(username, USERNAME_MAX_LENGTH, stdin);
    printf("Enter password: ");
    fgets(password, PASSWORD_MAX_LENGTH, stdin);
    // send signup request to server
    credentials = sanitize_credentials(username, password);
    char payload_buffer[MAX_BUFFER_SIZE];
    size_t payload_size = to_string_credentials(payload_buffer, MAX_BUFFER_SIZE, credentials);
    Header header = {OPCODE_SIGNUP, payload_size};
    size_t bytes_sent = send_header_and_payload(socket_fd, header, payload_buffer);
    if (bytes_sent < HEADER_SIZE + payload_size) {
        print_error_and_exit("Failed to send complete header and payload to server. Terminating.", CLIENT_ERROR_WRITE);
    }
    char response_buffer[MAX_BUFFER_SIZE];
    int bytes_read = read_exact(socket_fd, response_buffer, HEADER_SIZE);
    if (bytes_read < HEADER_SIZE) {
        print_error_and_exit("Failed to read response header from server. Terminating.", CLIENT_ERROR_READ);
    }
    // Response (sending is successful)
    Header response_header = parse_header(response_buffer);
    if (response_header.operation == OPCODE_SIGNUP_ERROR) {
        printf("signup failed, try with different credentials\n");
        return;
    }
    if (response_header.operation != OPCODE_OK || response_header.payload_size == 0) {
        print_error_and_exit("Received unexpected response from server. Terminating.", CLIENT_ERROR_READ);
    }
    bytes_read = read_exact(socket_fd, payload_buffer, response_header.payload_size);
    if (bytes_read < response_header.payload_size) {
        print_error_and_exit("Failed to read complete response payload from server. Terminating.", CLIENT_ERROR_READ);
    }
    *user = parse_user(payload_buffer);
    printf("Signup successful. Logged in as: %s with type: %d\n", user->username, user->user_type);
}

// EOF