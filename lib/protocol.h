#pragma once

#include "./configuration.h"
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#define HEADER_SIZE sizeof(MessageHeader)

typedef enum {
  // client opcodes
  OPCODE_LOGIN = 100,
  OPCODE_SIGNUP,
  OPCODE_BOOKING_REQUEST,
  OPCODE_LIST_REQUEST,
  // server opcodes
  OPCODE_OK = 200,
  OPCODE_LOGIN_ERROR,
  OPCODE_BOOKING_ERROR,
  OPCODE_LIST_ERROR,
} OpCode;

typedef enum { PENDING, APPROVED, REJECTED } BookingStatus;

typedef struct {
  uint booking_id;
  char username[USERNAME_MAX_LENGTH];
  char room_name[ROOM_NAME_MAX_LENGTH];
  time_t date;
  time_t start_time;
  time_t end_time;
  BookingStatus status;
} Booking;

typedef struct {
  char username[USERNAME_MAX_LENGTH];
  char password[PASSWORD_MAX_LENGTH];
} LoginCredentials;

typedef struct {
  OpCode operation;
  int payload_size;
} MessageHeader;

void to_string_header(char *buffer, size_t buffer_size, MessageHeader *header);
MessageHeader parse_header(char *buffer);

size_t to_string_login(char *buffer, size_t buffer_size, LoginCredentials *credentials);
LoginCredentials parse_login(char *buffer);

size_t to_string_booking(char *buffer, size_t buffer_size, Booking *booking);
Booking parse_booking(char *buffer);