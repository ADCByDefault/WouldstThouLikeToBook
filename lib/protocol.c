#include "protocol.h"
#include <arpa/inet.h>
#include <endian.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

// for testing purposes giving privilage to guest
const OpCode GUEST_OPCODES[6] = {OPCODE_LOGIN,       OPCODE_SIGNUP,        OPCODE_ROOMS_LIST,
                                 OPCODE_CREATE_ROOM, OPCODE_BOOKINGS_LIST, OPCODE_CREATE_BOOKING};
const OpCode USER_OPCODES[4] = {OPCODE_CREATE_BOOKING, OPCODE_ROOMS_LIST, OPCODE_CREATE_BOOKING, OPCODE_BOOKINGS_LIST};
const OpCode SUPERUSER_OPCODES[2] = {OPCODE_ROOMS_LIST, OPCODE_CREATE_ROOM};
const OpcodeDescription OPCODE_DESCRIPTIONS[] = {
    {OPCODE_UNDEFINED, "Undefined"},
    {OPCODE_LOGIN, "Login"},
    {OPCODE_SIGNUP, "Signup"},
    {OPCODE_CREATE_BOOKING, "Create Booking"},
    {OPCODE_ROOMS_LIST, "Rooms List"},
    {OPCODE_OK, "OK"},
    {OPCODE_ERROR, "Error"},
    {OPCODE_CREATE_ROOM, "Create Room"},
    {OPCODE_APPROVE_BOOKING, "Approve Booking"},
    {OPCODE_REJECT_BOOKING, "Reject Booking"},
    {OPCODE_LIST_BOOKINGS_SUPERUSER, "List Bookings Superuser"},
    {OPCODE_BOOKINGS_LIST, "Users Bookings List"},
};

Header header_hton(Header header) {
    header.operation = htonl(header.operation);
    header.payload_size = htonl(header.payload_size);
    return header;
}
Header header_ntoh(Header header) {
    header.operation = ntohl(header.operation);
    header.payload_size = ntohl(header.payload_size);
    return header;
}
LoginCredentials credentials_hton(LoginCredentials credentials) { return credentials; }
LoginCredentials credentials_ntoh(LoginCredentials credentials) { return credentials; }
User user_hton(User user) { return user; }
User user_ntoh(User user) { return user; }
Room room_hton(Room room) {
    room.room_id = htonl(room.room_id);
    return room;
}
Room room_ntoh(Room room) {
    room.room_id = ntohl(room.room_id);
    return room;
}
Booking booking_hton(Booking booking) {
    booking.booking_id = htonl(booking.booking_id);
    booking.room_id = htonl(booking.room_id);
    booking.start_time = htobe64(booking.start_time);
    booking.end_time = htobe64(booking.end_time);
    return booking;
}
Booking booking_ntoh(Booking booking) {
    booking.booking_id = ntohl(booking.booking_id);
    booking.room_id = ntohl(booking.room_id);
    booking.start_time = be64toh(booking.start_time);
    booking.end_time = be64toh(booking.end_time);
    return booking;
}

void sanitize_string(char *str, size_t max_length) {
    int length = strnlen(str, max_length);
    str[length] = '\0';
    for (int i = 0; i < length; i++) {
        if (str[i] == '\n' || str[i] == '\r') {
            str[i] = '\0';
            break;
        }
        if (str[i] == ':' || str[i] == ' ') {
            str[i] = '.';
        }
    }
}
LoginCredentials sanitize_credentials(LoginCredentials credentials) {
    sanitize_string(credentials.username, USERNAME_MAX_LENGTH);
    sanitize_string(credentials.password, PASSWORD_MAX_LENGTH);
    return credentials;
}

bool is_valid_opcode_from_client(OpCode opcode) {
    if (is_valid_opcode_for_guest(opcode) || is_valid_opcode_for_user(opcode) || is_valid_opcode_for_superuser(opcode)) {
        return true;
    }
    return false;
}
bool is_valid_opcode(OpCode opcode) {
    for (int i = 0; i < sizeof(OPCODE_DESCRIPTIONS) / sizeof(OPCODE_DESCRIPTIONS[0]); i++) {
        if (OPCODE_DESCRIPTIONS[i].opcode == opcode) {
            return true;
        }
    }
    return false;
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

int read_exact(int fd, void *buffer, uint32_t size) {
    int total_read = 0;
    while (total_read < size) {
        int bytes_read = read(fd, buffer + total_read, size - total_read);
        if (bytes_read <= 0) {
            return total_read;
        }
        total_read += bytes_read;
    }
    return total_read;
}
int send_header_and_payload(int fd, Header header, const char *payload) {

    int total_sent = 0;
    // Send header
    Header network_header = header_hton(header);
    while (total_sent < HEADER_SIZE) {
        int bytes_sent = send(fd, (const char *)&network_header + total_sent, HEADER_SIZE - total_sent, 0);
        if (bytes_sent <= 0) {
            return total_sent;
        }
        total_sent += bytes_sent;
    }
    // send payload only if it exists and payload size is greater than 0
    total_sent = 0;
    if (header.payload_size <= 0 || !payload) {
        return HEADER_SIZE;
    }
    while (total_sent < header.payload_size) {
        int bytes_sent = send(fd, (const char *)payload + total_sent, header.payload_size - total_sent, 0);
        if (bytes_sent <= 0) {
            break;
        }
        total_sent += bytes_sent;
    }

    return total_sent + HEADER_SIZE;
}

// EOF