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

size_t to_string_header(char *buffer, size_t buffer_size, Header *header) {
    return snprintf(buffer, buffer_size, "Operation: %u Payload Size: %lu", header->operation, header->payload_size);
}
Header parse_header(char *buffer) {
    Header header = {0};
    if (sscanf(buffer, "Operation: %u, Payload Size: %lu", &header.operation, &header.payload_size) != 2) {
        if (is_valid_opcode((header.operation))) {
            header.operation = OPCODE_UNDEFINED;
        }
    }
    return header;
}

LoginCredentials sanitize_credentials(char *username, char *password) {
    LoginCredentials credentials = {0};
    int username_length = strnlen(username, USERNAME_MAX_LENGTH);
    int password_length = strnlen(password, PASSWORD_MAX_LENGTH);
    for (int i = 0; i < username_length; i++) {
        if (username[i] == '\n' || username[i] == '\r') {
            username[i] = '\0';
            break;
        }
        if (username[i] == ':' || username[i] == ' ') {
            username[i] = '.';
        }
    }
    for (int i = 0; i < password_length; i++) {
        if (password[i] == '\n' || password[i] == '\r') {
            password[i] = '\0';
            break;
        }
        if (password[i] == ':' || password[i] == '\n' || password[i] == '\r' || password[i] == ' ') {
            password[i] = '.';
        }
    }
    snprintf(credentials.username, USERNAME_MAX_LENGTH, "%s", username);
    snprintf(credentials.password, PASSWORD_MAX_LENGTH, "%s", password);
    return credentials;
}
size_t to_string_credentials(char *buffer, size_t buffer_size, LoginCredentials *credentials) {
    *credentials = sanitize_credentials(credentials->username, credentials->password);
    return snprintf(buffer, buffer_size, "%s:%s", credentials->username, credentials->password);
}
LoginCredentials parse_credentials(char *buffer) {
    LoginCredentials credentials = {0};
    sscanf(buffer, "%[^:]:%[^:]", credentials.username, credentials.password);
    credentials = sanitize_credentials(credentials.username, credentials.password);
    return credentials;
}

size_t to_string_user(char *buffer, size_t buffer_size, User *user) {
    return snprintf(buffer, buffer_size, "%s:%d", user->username, user->user_type);
}
User parse_user(char *buffer) {
    User user = {0};
    int user_type;
    sscanf(buffer, "%[^:]:%d", user.username, &user_type);
    user.user_type = user_type;
    return user;
}

// format:
// "Booking:booking_id,room_name,username,date,start_time,end_time,status"
size_t to_string_booking(char *buffer, size_t buffer_size, Booking *booking) {
    return snprintf(buffer, buffer_size, "Booking:%u,%s,%s,%ld,%ld,%ld,%u", booking->booking_id, booking->room_name, booking->username,
                    booking->date, booking->start_time, booking->end_time, booking->status);
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

size_t read_exact(int fd, char *buffer, size_t size) {
    size_t total_read = 0;
    while (total_read < size) {
        fflush(stdout);
        ssize_t bytes_read = read(fd, buffer + total_read, size - total_read);
        if (bytes_read <= 0) {
            return total_read;
        }
        total_read += bytes_read;
    }
    return total_read;
}
size_t send_header_and_payload(int fd, Header *header, const char *payload) {

    if (!header) {
        return 0;
    }

    char header_buffer[HEADER_SIZE];
    to_string_header(header_buffer, HEADER_SIZE, header);
    size_t total_sent = 0;

    // Send header
    while (total_sent < HEADER_SIZE) {
        ssize_t bytes_sent = send(fd, header_buffer + total_sent, HEADER_SIZE - total_sent, 0);
        if (bytes_sent <= 0) {
            return total_sent;
        }
        total_sent += bytes_sent;
    }

    // send header only if it exists and payload size is greater than 0
    total_sent = 0;
    if (header->payload_size > 0 && payload) {
        while (total_sent < header->payload_size) {
            ssize_t bytes_sent = send(fd, payload + total_sent, header->payload_size - total_sent, 0);
            if (bytes_sent <= 0) {
                break;
            }
            total_sent += bytes_sent;
        }
    }

    return total_sent + HEADER_SIZE;
}

// EOF