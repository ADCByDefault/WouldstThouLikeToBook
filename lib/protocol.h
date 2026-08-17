#pragma once

#include "./configuration.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define HEADER_SIZE sizeof(MessageHeader)

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

extern const OpCode GUEST_OPCODES[3];
extern const OpCode USER_OPCODES[2];
extern const OpCode SUPERUSER_OPCODES[2];

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

typedef struct MessageHeader {
    OpCode operation;
    int payload_size;
} MessageHeader;

void to_string_header(char *buffer, size_t buffer_size, MessageHeader *header);
MessageHeader parse_header(char *buffer);

size_t to_string_login(char *buffer, size_t buffer_size, LoginCredentials *credentials);
LoginCredentials parse_login(char *buffer);

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