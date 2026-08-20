#pragma once

#include "./lib/configuration.h"
#include "./lib/protocol.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef bool (*booking_filter)(Booking *booking, void *search_value);
typedef struct TimeRange {
    uint64_t start_time;
    uint64_t end_time;
} TimeRange;

typedef struct User_Save {
    char username[USERNAME_MAX_LENGTH];
    char password[PASSWORD_MAX_LENGTH];
    UserType user_type;
} User_Save;

typedef void (*OperationHandler)(int socket_fd, User *user, Header header);
typedef struct Handler {
    OpCode opcode;
    OperationHandler handler;
} Handler;

extern const Handler HANDLERS[8];

// check if users file, rooms file, and bookings file exist, if not create them
// and try to read server settings
struct sockaddr_in initialize_server();

// if error_code == 0 then exit with EXIT_FAILURE
void print_error_and_exit(const char *error_message, int error_code);

// setup the child process to use the given socket file descriptor
bool setup_for_child_process(int socket_fd);
bool lock_writing_for_file(FILE *file);
void unlock_writing_for_file(FILE *file);
bool lock_reading_for_file(FILE *file);
void unlock_reading_for_file(FILE *file);

// return User with empty username if login failed, otherwise return User with the given user_type
User login(LoginCredentials credentials);
// return User with empty username if signup failed, otherwise return User with the given user_type
User signup(LoginCredentials credentials, UserType user_type);
// handle login request from client
void handle_login(int socket_fd, User *user, Header header);
// handle signup request from client
void handle_signup(int socket_fd, User *user, Header header);

// handle create room request from client
void handle_create_room(int socket_fd, User *user, Header header);
// returns rooms count
// first argument is pointer to the array of rooms
int get_rooms_list(Room **rooms_list);
void handle_list_rooms(int socket_fd, User *user, Header header);

bool is_in_same_time_slot(uint64_t start_time1, uint64_t end_time1, uint64_t start_time2, uint64_t end_time2);
// conflic if one of the bookings is approved and they are in the same time slot
bool is_booking_conflict(Booking booking1, Booking booking2);
// create a new booking, return Booking with booking_id = 0 if failed
Booking create_booking(Booking new_booking);
bool match_by_username(Booking *booking, void *search_value);
bool match_by_room_id(Booking *booking, void *search_value);
bool match_by_room_is_mask_username(Booking *booking, void *search_value);
bool match_by_date(Booking *booking, void *search_value);
bool match_by_time_range(Booking *booking, void *search_value);
bool match_by_status(Booking *booking, void *search_value);
// count bookings that match the given filter and search value
int count_bookings_by_filter(booking_filter filter, void *search_value);
// send bookings that match the given filter and search value to the client
// returns the number of bookings sent, or -1 on error
int send_bookings_by_filter(int socket_fd, booking_filter filter, void *search_value);
void handle_bookings_list(int socket_fd, User *user, Header header);
void handle_create_booking(int socket_fd, User *user, Header header);

// returns Booking with booking_id = 0 if failed, otherwise returns the approved booking
Booking approve_booking(Booking booking_to_approve);
// returns Booking with booking_id = 0 if failed, otherwise returns the rejected booking
Booking reject_booking(Booking booking_to_reject);
// handle approve booking request from client
void handle_approve_booking(int socket_fd, User *user, Header header);
// handle reject booking request from client
void handle_reject_booking(int socket_fd, User *user, Header header);