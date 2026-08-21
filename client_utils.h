#pragma once

#include "./lib/configuration.h"
#include "./lib/protocol.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef void (*OperationHandler)(int socket_fd, User *user);

typedef struct Handeler {
    OpCode opcode;
    OperationHandler handler;
} Handeler;

extern const Handeler HANDLERS[16];

// initializes the client socket and returns the socket file descriptor
struct sockaddr_in initialize_client();

// if error_code == 0 then exit with EXIT_FAILURE
void print_error_and_exit(const char *error_message, int error_code);
void flush_stdin();

void print_info(User user);
// A string to describe the operations that a GUEST can perform
void print_guest_can_do_operations();
// A string to describe the operations that a USER can perform
void print_user_can_do_operations();
// A string to describe the operations that a SUPERUSER can perform
void print_superuser_can_do_operations();
// A string to describe the operations that a user can perform based on their type
void print_can_do_operations_by_type(UserType user_type);

void handle_login(int socket_fd, User *user);
void handle_signup(int socket_fd, User *user);
void handle_logout(int socket_fd, User *user);

void print_room(Room room);
void handle_create_room(int socket_fd, User *user);
void handle_list_rooms(int socket_fd, User *user);

bool is_valid_date(int day, int month, int year);
bool is_valid_time(int hour);
bool is_time_in_past(time_t time);

void print_booking(Booking booking);
void handle_create_booking(int socket_fd, User *user);
void handle_users_bookings_list(int socket_fd, User *user);

void handle_approve_booking(int socket_fd, User *user);
void handle_reject_booking(int socket_fd, User *user);

void handle_bookings_list_superuser(int socket_fd, User *user);

void handle_bookings_list_room_id(int socket_fd, User *user);
void handle_bookings_list_username(int socket_fd, User *user);
void handle_bookings_list_booking_id(int socket_fd, User *user);
void handle_bookings_list_status(int socket_fd, User *user);
void handle_bookings_list_time_range(int socket_fd, User *user);

void handle_force_booking_status(int socket_fd, User *user);