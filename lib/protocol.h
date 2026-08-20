#pragma once

#include "./configuration.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define HEADER_SIZE sizeof(Header)

typedef enum UserType { GUEST = 0, USER = 1, SUPERUSER = 2 } UserType;
typedef struct User {
    char username[USERNAME_MAX_LENGTH];
    uint8_t user_type;
} User;

typedef enum OpCode {
    OPCODE_UNDEFINED = 0,
    OPCODE_CANCEL,
    // client opcodes
    OPCODE_LOGIN = 100,
    OPCODE_SIGNUP,
    OPCODE_CREATE_BOOKING,
    OPCODE_ROOMS_LIST,
    OPCODE_BOOKINGS_LIST,
    // client opcodes specific to superuser
    OPCODE_CREATE_ROOM = 150,
    OPCODE_APPROVE_BOOKING,
    OPCODE_REJECT_BOOKING,
    OPCODE_LIST_BOOKINGS_SUPERUSER,
    // server opcodes
    OPCODE_OK = 200,
    OPCODE_ERROR,
} OpCode;

typedef struct OpcodeDescription {
    OpCode opcode;
    const char *description;
} OpcodeDescription;

extern const OpCode GUEST_OPCODES[6];
extern const OpCode USER_OPCODES[4];
extern const OpCode SUPERUSER_OPCODES[2];
extern const OpcodeDescription OPCODE_DESCRIPTIONS[];

typedef struct Room {
    uint32_t room_id;
    char room_name[ROOM_NAME_MAX_LENGTH];
} Room;

typedef enum BookingStatus { PENDING = 0, APPROVED = 1, REJECTED = 2 } BookingStatus;
typedef struct Booking {
    uint32_t booking_id;
    uint32_t room_id;
    char username[USERNAME_MAX_LENGTH];
    uint64_t start_time;
    uint64_t end_time;
    uint8_t status;
} Booking;

typedef struct LoginCredentials {
    char username[USERNAME_MAX_LENGTH];
    char password[PASSWORD_MAX_LENGTH];
} LoginCredentials;

typedef struct Header {
    uint32_t operation;
    uint32_t payload_size;
} Header;

Header header_hton(Header header);
Header header_ntoh(Header header);
LoginCredentials credentials_hton(LoginCredentials credentials);
LoginCredentials credentials_ntoh(LoginCredentials credentials);
User user_hton(User user);
User user_ntoh(User user);
Room room_hton(Room room);
Room room_ntoh(Room room);
Booking booking_hton(Booking booking);
Booking booking_ntoh(Booking booking);

// returns sanitized credentials
void sanitize_string(char *str, size_t max_length);
LoginCredentials sanitize_credentials(LoginCredentials credentials);

bool is_valid_opcode_from_client(OpCode opcode);
bool is_valid_opcode(OpCode opcode);
bool is_valid_opcode_for_guest(OpCode opcode);
bool is_valid_opcode_for_user(OpCode opcode);
bool is_valid_opcode_for_superuser(OpCode opcode);
bool is_valid_opcode_for_user_by_type(OpCode opcode, UserType user_type);
char *get_opcode_description(OpCode opcode);

// Reads exactly 'size' bytes from the file descriptor 'fd' into 'buffer'.
// Returns the number of bytes read
int read_exact(int fd, void *buffer, uint32_t size);
// Sends the header and payload to the file descriptor 'fd'.
// Returns the total number of bytes sent (header + payload)
int send_header_and_payload(int fd, Header header, const char *payload);