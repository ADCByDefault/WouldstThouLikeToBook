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

const Handeler HANDLERS[4] = {
    {OPCODE_LOGIN, handle_login},
    {OPCODE_SIGNUP, handle_signup},
    {OPCODE_CREATE_ROOM, handle_create_room},
    {OPCODE_ROOMS_LIST, handle_list_rooms},
    // Add more handlers as needed
};

struct sockaddr_in initialize_server() {
    // making sure data files exist, if not create them
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

    // Creating admin user
    User admin = signup((LoginCredentials){"admin", "admin"}, SUPERUSER);

    // Read server settings from file
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
bool lock_reading_for_file(FILE *file) {
    if (file == NULL) {
        return false;
    }

    struct flock lock = {0};
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    return fcntl(fileno(file), F_SETLKW, &lock) == 0;
}
void unlock_reading_for_file(FILE *file) {
    if (file == NULL) {
        return;
    }
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
    if (!lock_reading_for_file(users_file)) {
        fclose(users_file);
        return user; // Error locking users file
    }
    while (fread(&user_save, sizeof(User_Save), 1, users_file) == 1) {
        if (strcmp(user_save.username, credentials.username) == 0) {
            if (strcmp(user_save.password, credentials.password) != 0) {
                unlock_reading_for_file(users_file);
                fclose(users_file);
                return user; // Incorrect password, return user with empty username
            }
            snprintf(user.username, USERNAME_MAX_LENGTH, "%s", user_save.username);
            user.user_type = user_save.user_type;
            unlock_reading_for_file(users_file);
            fclose(users_file);
            return user; // Login successful
        }
    }
    unlock_reading_for_file(users_file);
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
void handle_login(int socket_fd, User *user, Header header) {
    if (header.payload_size != sizeof(LoginCredentials)) {
        print_error_and_exit("Invalid payload for login operation", SERVER_CHILD_ERROR_READ);
    }
    LoginCredentials credentials = {0};
    int bytes_read = read_exact(socket_fd, &credentials, header.payload_size);
    if (bytes_read < header.payload_size) {
        print_error_and_exit("Failed to read complete payload for login operation", SERVER_CHILD_ERROR_READ);
    }
    credentials = sanitize_credentials(credentials_ntoh(credentials));
    User logged_in_user = login(credentials);
    Header response_header = {0};
    if (strlen(logged_in_user.username) != 0) {
        // Login successful
        *user = logged_in_user;
        response_header.operation = OPCODE_OK;
        response_header.payload_size = sizeof(logged_in_user);
        logged_in_user = user_hton(logged_in_user);
        int b = send_header_and_payload(socket_fd, response_header, (const char *)&logged_in_user);
        return;
    }
    // Login failed
    response_header.operation = OPCODE_LOGIN_ERROR;
    response_header.payload_size = 0;
    send_header_and_payload(socket_fd, response_header, NULL);
}
void handle_signup(int socket_fd, User *user, Header header) {
    if (header.payload_size != sizeof(LoginCredentials)) {
        print_error_and_exit("Invalid payload for signup operation", SERVER_CHILD_ERROR_READ);
    }
    // read credentials from client
    LoginCredentials credentials = {0};
    int bytes_read = read_exact(socket_fd, &credentials, header.payload_size);
    if (bytes_read < header.payload_size) {
        print_error_and_exit("Failed to read complete payload for signup operation", SERVER_CHILD_ERROR_READ);
    }
    credentials = sanitize_credentials(credentials_ntoh(credentials));
    Header response_header = {0};
    // Validate credentials length
    if (strlen(credentials.username) < USERNAME_MIN_LENGTH || strlen(credentials.username) >= USERNAME_MAX_LENGTH ||
        strlen(credentials.password) < PASSWORD_MIN_LENGTH || strlen(credentials.password) >= PASSWORD_MAX_LENGTH) {
        response_header.operation = OPCODE_SIGNUP_ERROR;
        response_header.payload_size = 0;
        send_header_and_payload(socket_fd, response_header, NULL);
        return;
    }
    User signed_up_user = signup(credentials, USER);
    if (strlen(signed_up_user.username) == 0) {
        // Signup failed
        response_header.operation = OPCODE_SIGNUP_ERROR;
        response_header.payload_size = 0;
        send_header_and_payload(socket_fd, response_header, NULL);
        return;
    }
    // Signup successful
    *user = signed_up_user;
    response_header.operation = OPCODE_OK;
    response_header.payload_size = sizeof(signed_up_user);
    signed_up_user = user_hton(signed_up_user);
    send_header_and_payload(socket_fd, response_header, (const char *)&signed_up_user);
}

Room create_room(Room new_room) {
    int greatest_room_id = 0;
    Room created_room = {0};
    FILE *rooms_file = fopen(ROOMS_FILE_NAME, "ab+");
    if (rooms_file == NULL) {
        return created_room; // Error opening rooms file
    }

    if (!lock_writing_for_file(rooms_file)) {
        fclose(rooms_file);
        return created_room; // Error locking rooms file
    }

    fseek(rooms_file, 0, SEEK_SET);
    Room room = {0};
    sanitize_string(new_room.room_name, ROOM_NAME_MAX_LENGTH);
    while (fread(&room, sizeof(Room), 1, rooms_file) == 1) {
        if (strcmp(room.room_name, new_room.room_name) == 0) {
            unlock_writing_for_file(rooms_file);
            fclose(rooms_file);
            return created_room; // Room name already exists, return room with empty room
        }
        if (room.room_id > greatest_room_id) {
            greatest_room_id = room.room_id;
        }
    }
    // Add new room to the rooms file
    snprintf(created_room.room_name, ROOM_NAME_MAX_LENGTH, "%s", new_room.room_name);
    created_room.room_id = greatest_room_id + 1;
    fwrite(&created_room, sizeof(Room), 1, rooms_file);
    unlock_writing_for_file(rooms_file);
    fclose(rooms_file);
    return created_room; // Room creation successful
}
int get_rooms_list(Room **rooms_list) {
    int room_count = 0;
    FILE *rooms_file = fopen(ROOMS_FILE_NAME, "rb");
    if (rooms_file == NULL) {
        return -1; // Error opening rooms file
    }

    if (!lock_reading_for_file(rooms_file)) {
        fclose(rooms_file);
        return -1; // Error locking rooms file
    }

    fseek(rooms_file, 0, SEEK_END);
    long file_size = ftell(rooms_file);
    fseek(rooms_file, 0, SEEK_SET);

    room_count = file_size / sizeof(Room);
    *rooms_list = malloc(room_count * sizeof(Room));
    if (*rooms_list == NULL) {
        unlock_reading_for_file(rooms_file);
        fclose(rooms_file);
        return -1; // Memory allocation error
    }

    fread(*rooms_list, sizeof(Room), room_count, rooms_file);
    unlock_reading_for_file(rooms_file);
    fclose(rooms_file);

    return room_count;
}
void handle_create_room(int socket_fd, User *user, Header header) {
    if (header.payload_size != sizeof(Room)) {
        print_error_and_exit("Invalid payload for create room operation", SERVER_CHILD_ERROR_READ);
    }
    Room new_room = {0};
    int bytes_read = read_exact(socket_fd, &new_room, header.payload_size);
    if (bytes_read < header.payload_size) {
        print_error_and_exit("Failed to read complete payload for create room operation", SERVER_CHILD_ERROR_READ);
    }
    new_room = room_ntoh(new_room);
    Header response_header = {0};
    Room created_room = create_room(new_room);
    if (strlen(created_room.room_name) == 0) {
        // Room creation failed
        response_header.operation = OPCODE_CREATE_ROOM_ERROR;
        response_header.payload_size = 0;
        send_header_and_payload(socket_fd, response_header, NULL);
        return;
    }
    // Room creation successful
    response_header.operation = OPCODE_OK;
    response_header.payload_size = sizeof(created_room);
    created_room = room_hton(created_room);
    send_header_and_payload(socket_fd, response_header, (const char *)&created_room);
}
void handle_list_rooms(int socket_fd, User *user, Header header) {
    Room *rooms_list = NULL;
    int room_count = get_rooms_list(&rooms_list);
    if (room_count < 0) {
        perror("Failed to get rooms list");
        Header response_header = {0};
        response_header.operation = OPCODE_LIST_ERROR;
        response_header.payload_size = 0;
        send_header_and_payload(socket_fd, response_header, NULL);
        return;
    }
    Header response_header = {0};
    response_header.operation = OPCODE_OK;
    response_header.payload_size = room_count * sizeof(Room);
    for (int i = 0; i < room_count; i++) {
        rooms_list[i] = room_hton(rooms_list[i]);
    }
    send_header_and_payload(socket_fd, response_header, (const char *)rooms_list);
    free(rooms_list);
}
// EOF