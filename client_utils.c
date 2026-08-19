#include "client_utils.h"
#include "./lib/configuration.h"
#include "./lib/protocol.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>

const Handeler HANDLERS[4] = {
    {OPCODE_LOGIN, handle_login},
    {OPCODE_SIGNUP, handle_signup},
    {OPCODE_ROOMS_LIST, handle_list_rooms},
    {OPCODE_CREATE_ROOM, handle_create_room},
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
    printf("Enter username: ");
    fgets(credentials.username, USERNAME_MAX_LENGTH, stdin);
    printf("Enter password: ");
    fgets(credentials.password, PASSWORD_MAX_LENGTH, stdin);
    // send login request to server
    credentials = credentials_hton(sanitize_credentials(credentials));
    Header header = {OPCODE_LOGIN, sizeof(credentials)};
    int bytes_sent = send_header_and_payload(socket_fd, header, (const char *)&credentials);
    if (bytes_sent < HEADER_SIZE + header.payload_size) {
        print_error_and_exit("Failed to send complete header and payload to server. Terminating.", CLIENT_ERROR_WRITE);
    }
    // Response
    Header response_header = {0};
    User response_user = {0};
    int bytes_read = read_exact(socket_fd, &response_header, HEADER_SIZE);
    if (bytes_read < HEADER_SIZE) {
        print_error_and_exit("Failed to read response header from server. Terminating.", CLIENT_ERROR_READ);
    }
    response_header = header_ntoh(response_header);
    if (response_header.operation == OPCODE_LOGIN_ERROR) {
        printf("login failed, please check your username and password\n");
        return;
    }
    if (response_header.operation != OPCODE_OK || response_header.payload_size != sizeof(response_user)) {
        print_error_and_exit("Received unexpected response from server. Terminating.", CLIENT_ERROR_READ);
    }
    bytes_read = read_exact(socket_fd, &response_user, response_header.payload_size);
    if (bytes_read < response_header.payload_size) {
        print_error_and_exit("Failed to read complete response payload from server. Terminating.", CLIENT_ERROR_READ);
    }
    *user = user_ntoh(response_user);
    printf("Login successful. Logged in as: %s with type: %d\n", user->username, user->user_type);
}
void handle_signup(int socket_fd, User *user) {
    // getting username and password from user input
    LoginCredentials credentials = {0};
    printf("Username and password must be between %d and %d characters long.\n", USERNAME_MIN_LENGTH, USERNAME_MAX_LENGTH - 1);
    printf("Enter username: ");
    fgets(credentials.username, USERNAME_MAX_LENGTH, stdin);
    printf("Enter password: ");
    fgets(credentials.password, PASSWORD_MAX_LENGTH, stdin);
    // send signup request to server
    Header header = {OPCODE_SIGNUP, sizeof(credentials)};
    credentials = credentials_hton(sanitize_credentials(credentials));
    int bytes_sent = send_header_and_payload(socket_fd, header, (const char *)&credentials);
    if (bytes_sent < HEADER_SIZE + header.payload_size) {
        print_error_and_exit("Failed to send complete header and payload to server. Terminating.", CLIENT_ERROR_WRITE);
    }
    Header response_header = {0};
    User response_user = {0};
    int bytes_read = read_exact(socket_fd, &response_header, HEADER_SIZE);
    if (bytes_read < HEADER_SIZE) {
        print_error_and_exit("Failed to read response header from server. Terminating.", CLIENT_ERROR_READ);
    }
    response_header = header_ntoh(response_header);
    // Response (sending is successful)
    if (response_header.operation == OPCODE_SIGNUP_ERROR) {
        printf("signup failed, try with different credentials\n");
        return;
    }
    if (response_header.operation != OPCODE_OK || response_header.payload_size != sizeof(response_user)) {
        print_error_and_exit("Received unexpected response from server. Terminating.", CLIENT_ERROR_READ);
    }
    bytes_read = read_exact(socket_fd, &response_user, response_header.payload_size);
    if (bytes_read < response_header.payload_size) {
        print_error_and_exit("Failed to read complete response payload from server. Terminating.", CLIENT_ERROR_READ);
    }
    *user = user_ntoh(response_user);
    printf("Signup successful. Logged in as: %s with type: %d\n", user->username, user->user_type);
}
void handle_create_room(int socket_fd, User *user) {
    Room new_room = {0};
    printf("Enter room name: ");
    fgets(new_room.room_name, ROOM_NAME_MAX_LENGTH, stdin);
    Header header = {OPCODE_CREATE_ROOM, sizeof(new_room)};
    new_room = room_hton(new_room);
    int bytes_sent = send_header_and_payload(socket_fd, header, (const char *)&new_room);
    if (bytes_sent < HEADER_SIZE + header.payload_size) {
        print_error_and_exit("Failed to send complete header and payload to server. Terminating.", CLIENT_ERROR_WRITE);
    }
    Header response_header = {0};
    int bytes_read = read_exact(socket_fd, &response_header, HEADER_SIZE);
    if (bytes_read < HEADER_SIZE) {
        print_error_and_exit("Failed to read response header from server. Terminating.", CLIENT_ERROR_READ);
    }
    response_header = header_ntoh(response_header);
    if (response_header.operation == OPCODE_CREATE_ROOM_ERROR) {
        printf("Create room failed. Room may already exist or an error occurred.\n");
        return;
    }
    if (response_header.operation != OPCODE_OK || response_header.payload_size != sizeof(Room)) {
        print_error_and_exit("Received unexpected response from server. Terminating.", CLIENT_ERROR_READ);
    }
    Room created_room = {0};
    bytes_read = read_exact(socket_fd, &created_room, response_header.payload_size);
    if (bytes_read < response_header.payload_size) {
        print_error_and_exit("Failed to read complete response payload from server. Terminating.", CLIENT_ERROR_READ);
    }
    created_room = room_ntoh(created_room);
    printf("Room created successfully: ID=%u, Name=%s\n", created_room.room_id, created_room.room_name);
}
void handle_list_rooms(int socket_fd, User *user) {
    Header header = {OPCODE_ROOMS_LIST, 0};
    int bytes_sent = send_header_and_payload(socket_fd, header, NULL);
    if (bytes_sent < HEADER_SIZE) {
        print_error_and_exit("Failed to send header to server. Terminating.", CLIENT_ERROR_WRITE);
    }
    Header response_header = {0};
    int bytes_read = read_exact(socket_fd, &response_header, HEADER_SIZE);
    if (bytes_read < HEADER_SIZE) {
        print_error_and_exit("Failed to read response header from server. Terminating.", CLIENT_ERROR_READ);
    }
    response_header = header_ntoh(response_header);
    if (response_header.operation == OPCODE_LIST_ERROR) {
        printf("List rooms failed. An error occurred on the server.\n");
        return;
    }
    if (response_header.operation != OPCODE_OK || response_header.payload_size % sizeof(Room) != 0) {
        print_error_and_exit("Received unexpected response from server. Terminating.", CLIENT_ERROR_READ);
    }
    if (response_header.payload_size == 0) {
        printf("No rooms found in database.\n");
        return;
    }
    size_t room_count = response_header.payload_size / sizeof(Room);
    Room *rooms_list = malloc(response_header.payload_size);
    if (!rooms_list) {
        print_error_and_exit("Memory allocation failed. Terminating.", CLIENT_ERROR_MEMORY_ALLOCATION);
    }
    bytes_read = read_exact(socket_fd, rooms_list, response_header.payload_size);
    if (bytes_read < response_header.payload_size) {
        free(rooms_list);
        print_error_and_exit("Failed to read complete response payload from server. Terminating.", CLIENT_ERROR_READ);
    }
    printf("Rooms List:\n");
    for (size_t i = 0; i < room_count; i++) {
        rooms_list[i] = room_ntoh(rooms_list[i]);
        printf("Room ID=%u, Room Name=%s\n", rooms_list[i].room_id, rooms_list[i].room_name);
    }
    free(rooms_list);
}
// EOF