#pragma once

#include "./configuration.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define HEADER_SIZE sizeof(Header)

typedef enum UserType { GUEST, USER, SUPERUSER } UserType;

typedef enum OpCode {
    OPCODE_UNDEFINED = 0,
    // client opcodes
    OPCODE_LOGIN = 100,
    OPCODE_SIGNUP,
    OPCODE_BOOKING,
    OPCODE_ROOMS_LIST,
    // server opcodes
    OPCODE_OK = 200,
    OPCODE_LOGIN_ERROR,
    OPCODE_BOOKING_ERROR,
    OPCODE_LIST_ERROR,
} OpCode;

typedef struct OpcodeDescription {
    OpCode opcode;
    const char *description;
} OpcodeDescription;

extern const OpCode GUEST_OPCODES[3];
extern const OpCode USER_OPCODES[2];
extern const OpCode SUPERUSER_OPCODES[2];

extern const OpcodeDescription OPCODE_DESCRIPTIONS[];

typedef enum BookingStatus { PENDING, APPROVED, REJECTED } BookingStatus;

typedef struct Booking {
    uint booking_id;
    char username[USERNAME_MAX_LENGTH];
    char room_name[ROOM_NAME_MAX_LENGTH];
    time_t date;
    time_t start_time;
    time_t end_time;
    BookingStatus status;
} Booking;

typedef struct LoginCredentials {
    char username[USERNAME_MAX_LENGTH];
    char password[PASSWORD_MAX_LENGTH];
} LoginCredentials;

typedef struct Header {
    OpCode operation;
    size_t payload_size;
} Header;

void to_string_header(char *buffer, size_t buffer_size, Header *header);
Header parse_header(char *buffer);

size_t to_string_credentials(char *buffer, size_t buffer_size, LoginCredentials *credentials);
LoginCredentials parse_credentials(char *buffer);

size_t to_string_booking(char *buffer, size_t buffer_size, Booking *booking);
Booking parse_booking(char *buffer);

bool is_valid_opcode_from_client(OpCode opcode);
bool is_valid_opcode_from_server(OpCode opcode);
bool is_valid_opcode(OpCode opcode);
bool is_valid_opcode_for_guest(OpCode opcode);
bool is_valid_opcode_for_user(OpCode opcode);
bool is_valid_opcode_for_superuser(OpCode opcode);
bool is_valid_opcode_for_user_by_type(OpCode opcode, UserType user_type);
char *get_opcode_description(OpCode opcode);

// Reads exactly 'size' bytes from the file descriptor 'fd' into 'buffer'.
// Returns the number of bytes read
size_t read_exact(int fd, char *buffer, size_t size);
// Sends the header and payload to the file descriptor 'fd'.
// Returns the total number of bytes sent (header + payload)
size_t send_header_and_payload(int fd, Header *header, const char *payload);