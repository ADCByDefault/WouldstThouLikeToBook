#include "protocol.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

const OpCode GUEST_OPCODES[3] = {OPCODE_LOGIN, OPCODE_SIGNUP, OPCODE_ROOMS_LIST};
const OpCode USER_OPCODES[2] = {OPCODE_BOOKING, OPCODE_ROOMS_LIST};
const OpCode SUPERUSER_OPCODES[2] = {OPCODE_SIGNUP, OPCODE_ROOMS_LIST};

void to_string_header(char *buffer, size_t buffer_size, MessageHeader *header) {
    snprintf(buffer, buffer_size, "Operation: %u Payload Size: %u", header->operation, header->payload_size);
}
MessageHeader parse_header(char *buffer) {
    MessageHeader header = {0};
    if (sscanf(buffer, "Operation: %u, Payload Size: %u", &header.operation, &header.payload_size) != 2) {
        if (is_valid_opcode((header.operation))) {
            header.operation = OPCODE_UNDEFINED;
        }
    }
    return header;
}

size_t to_string_login(char *buffer, size_t buffer_size, LoginCredentials *credentials) {
    return snprintf(buffer, buffer_size, "%s:%s", credentials->username, credentials->password);
}
LoginCredentials parse_login(char *buffer) {
    LoginCredentials credentials = {0};
    sscanf(buffer, "%[^:]:%[^:]", credentials.username, credentials.password);
    return credentials;
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
    switch (opcode) {
    case OPCODE_UNDEFINED:
        return "Undefined";
    case OPCODE_LOGIN:
        return "Login";
    case OPCODE_SIGNUP:
        return "Signup";
    case OPCODE_BOOKING:
        return "Booking";
    case OPCODE_ROOMS_LIST:
        return "Rooms List";
    case OPCODE_OK:
        return "OK";
    case OPCODE_LOGIN_ERROR:
        return "Login Error";
    case OPCODE_BOOKING_ERROR:
        return "Booking Error";
    case OPCODE_LIST_ERROR:
        return "List Error";
    default:
        return "Unknown Opcode";
    }
}
