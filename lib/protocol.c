#include "protocol.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

const OpCode GUEST_OPCODES[3] = {OPCODE_LOGIN, OPCODE_SIGNUP, OPCODE_ROOMS_LIST};
const OpCode USER_OPCODES[2] = {OPCODE_BOOKING, OPCODE_ROOMS_LIST};
const OpCode SUPERUSER_OPCODES[2] = {OPCODE_SIGNUP, OPCODE_ROOMS_LIST};
const OpcodeDescription OPCODE_DESCRIPTIONS[] = {{OPCODE_UNDEFINED, "Undefined"},     {OPCODE_LOGIN, "Login"},
                                                 {OPCODE_SIGNUP, "Signup"},           {OPCODE_BOOKING, "Booking"},
                                                 {OPCODE_ROOMS_LIST, "Rooms List"},   {OPCODE_OK, "OK"},
                                                 {OPCODE_LOGIN_ERROR, "Login Error"}, {OPCODE_BOOKING_ERROR, "Booking Error"},
                                                 {OPCODE_LIST_ERROR, "List Error"}};

// returns current logical buffer size
int to_string_header(char *buffer, size_t buffer_size, Header header) {
    int size = snprintf(buffer, buffer_size, "Operation: %03u Payload Size: %04zu", header.operation, header.payload_size);
    return size < 0 ? size : strlen(buffer) + 1; // +1 for null terminator
}
Header parse_header(char *buffer) {
    Header header = {0};
    unsigned int operation;
    size_t payload_size;
    if (sscanf(buffer, "Operation: %03u Payload Size: %04zu", &operation, &payload_size) == 2) {
        header.operation = (OpCode)operation;
        header.payload_size = payload_size;
    }
    return header;
}

LoginCredentials sanitize_credentials(char *username, char *password) {
    LoginCredentials credentials = {0};
    int username_length = strnlen(username, USERNAME_MAX_LENGTH);
    int password_length = strnlen(password, PASSWORD_MAX_LENGTH);
    char sanitized_username[USERNAME_MAX_LENGTH];
    char sanitized_password[PASSWORD_MAX_LENGTH];
    strncpy(sanitized_username, username, username_length);
    sanitized_username[username_length] = '\0';
    strncpy(sanitized_password, password, password_length);
    sanitized_password[password_length] = '\0';
    for (int i = 0; i < username_length; i++) {
        if (sanitized_username[i] == '\n' || sanitized_username[i] == '\r') {
            sanitized_username[i] = '\0';
            break;
        }
        if (sanitized_username[i] == ':' || sanitized_username[i] == ' ') {
            sanitized_username[i] = '.';
        }
    }
    for (int i = 0; i < password_length; i++) {
        if (sanitized_password[i] == '\n' || sanitized_password[i] == '\r') {
            sanitized_password[i] = '\0';
            break;
        }
        if (sanitized_password[i] == ':' || sanitized_password[i] == ' ') {
            sanitized_password[i] = '.';
        }
    }
    snprintf(credentials.username, USERNAME_MAX_LENGTH, "%s", sanitized_username);
    snprintf(credentials.password, PASSWORD_MAX_LENGTH, "%s", sanitized_password);
    return credentials;
}
int to_string_credentials(char *buffer, size_t buffer_size, LoginCredentials credentials) {
    credentials = sanitize_credentials(credentials.username, credentials.password);
    int size = snprintf(buffer, buffer_size, "%s:%s", credentials.username, credentials.password);
    return size < 0 ? size : strlen(buffer) + 1; // +1 for null terminator
}
LoginCredentials parse_credentials(char *buffer) {
    LoginCredentials credentials = {0};
    if (sscanf(buffer, "%31[^:]:%31[^:]", credentials.username, credentials.password) == 2) {
        credentials = sanitize_credentials(credentials.username, credentials.password);
    }
    return credentials;
}

int to_string_user(char *buffer, size_t buffer_size, User user) {
    int size = snprintf(buffer, buffer_size, "%s:%d", user.username, user.user_type);
    return size < 0 ? size : strlen(buffer) + 1; // +1 for null terminator
}
User parse_user(char *buffer) {
    User user = {0};
    int user_type = GUEST;
    sscanf(buffer, "%31[^:]:%d", user.username, &user_type);
    user.user_type = user_type;
    return user;
}

// format:
// "Booking:booking_id,room_name,username,date,start_time,end_time,status"
int to_string_booking(char *buffer, size_t buffer_size, Booking booking) {
    int size = snprintf(buffer, buffer_size, "Booking:%u,%s,%s,%ld,%ld,%ld,%u", booking.booking_id, booking.room_name, booking.username,
                        booking.date, booking.start_time, booking.end_time, booking.status);
    return size < 0 ? size : strlen(buffer) + 1; // +1 for null terminator
}
Booking parse_booking(char *buffer) {
    Booking booking = {0};
    sscanf(buffer, "Booking:%u,%s,%s,%ld,%ld,%ld,%u", &booking.booking_id, booking.room_name, booking.username, &booking.date,
           &booking.start_time, &booking.end_time, &booking.status);
    return booking;
}

bool is_valid_opcode_from_client(OpCode opcode) {
    return opcode == OPCODE_LOGIN || opcode == OPCODE_SIGNUP || opcode == OPCODE_BOOKING || opcode == OPCODE_ROOMS_LIST;
}
bool is_valid_opcode_from_server(OpCode opcode) {
    return opcode == OPCODE_OK || opcode == OPCODE_LOGIN_ERROR || opcode == OPCODE_BOOKING_ERROR || opcode == OPCODE_LIST_ERROR;
}
bool is_valid_opcode(OpCode opcode) {
    return is_valid_opcode_from_client(opcode) || is_valid_opcode_from_server(opcode) || opcode == OPCODE_UNDEFINED;
}
bool is_valid_opcode_for_guest(OpCode opcode) {
    size_t num_opcodes = sizeof(GUEST_OPCODES) / sizeof(GUEST_OPCODES[0]);
    for (size_t i = 0; i < num_opcodes; i++) {
        if (GUEST_OPCODES[i] == opcode) {
            return true;
        }
    }
    return false;
}
bool is_valid_opcode_for_user(OpCode opcode) {
    size_t num_opcodes = sizeof(USER_OPCODES) / sizeof(USER_OPCODES[0]);
    for (size_t i = 0; i < num_opcodes; i++) {
        if (USER_OPCODES[i] == opcode) {
            return true;
        }
    }
    return false;
}
bool is_valid_opcode_for_superuser(OpCode opcode) {
    size_t num_opcodes = sizeof(SUPERUSER_OPCODES) / sizeof(SUPERUSER_OPCODES[0]);
    for (size_t i = 0; i < num_opcodes; i++) {
        if (SUPERUSER_OPCODES[i] == opcode) {
            return true;
        }
    }
    return false;
}
bool is_valid_opcode_for_user_by_type(OpCode opcode, UserType user_type) {
    switch (user_type) {
    case GUEST:
        return is_valid_opcode_for_guest(opcode);
    case USER:
        return is_valid_opcode_for_user(opcode);
    case SUPERUSER:
        return is_valid_opcode_for_superuser(opcode);
    default:
        return false;
    }
}
char *get_opcode_description(OpCode opcode) {
    size_t num_descriptions = sizeof(OPCODE_DESCRIPTIONS) / sizeof(OPCODE_DESCRIPTIONS[0]);
    for (size_t i = 0; i < num_descriptions; i++) {
        if (OPCODE_DESCRIPTIONS[i].opcode == opcode) {
            return (char *)OPCODE_DESCRIPTIONS[i].description;
        }
    }
    return "Unknown operation code";
}

int read_exact(int fd, char *buffer, size_t size) {
    int total_read = 0;
    while (total_read < size) {
        fflush(stdout);
        int bytes_read = read(fd, buffer + total_read, size - total_read);
        if (bytes_read <= 0) {
            return total_read;
        }
        total_read += bytes_read;
    }
    return total_read;
}
int send_header_and_payload(int fd, Header header, const char *payload) {

    char header_buffer[HEADER_SIZE];
    to_string_header(header_buffer, HEADER_SIZE, header);
    int total_sent = 0;

    // Send header
    while (total_sent < HEADER_SIZE) {
        int bytes_sent = send(fd, header_buffer + total_sent, HEADER_SIZE - total_sent, 0);
        if (bytes_sent <= 0) {
            return total_sent;
        }
        total_sent += bytes_sent;
    }

    // send header only if it exists and payload size is greater than 0
    total_sent = 0;
    if (header.payload_size > 0 && payload) {
        while (total_sent < header.payload_size) {
            int bytes_sent = send(fd, payload + total_sent, header.payload_size - total_sent, 0);
            if (bytes_sent <= 0) {
                break;
            }
            total_sent += bytes_sent;
        }
    }

    return total_sent + HEADER_SIZE;
}

// EOF