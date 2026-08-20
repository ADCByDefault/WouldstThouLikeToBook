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

typedef struct booking_filter_context {
    void *search_value;
    User *user;
} booking_filter_context;
typedef bool (*booking_filter)(Booking *booking, booking_filter_context *filter_context);

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

extern const Handler HANDLERS[10];

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
// handle logout request from client
void handle_logout(int socket_fd, User *user, Header header);

// handle create room request from client
void handle_create_room(int socket_fd, User *user, Header header);
// returns rooms count
// first argument is pointer to the array of rooms
int get_rooms_list(Room **rooms_list);
void handle_list_rooms(int socket_fd, User *user, Header header);

bool is_in_same_time_slot(uint64_t start_time1, uint64_t end_time1, uint64_t start_time2, uint64_t end_time2);
// conflic if on same room and _is_in_same_time_slot and (one of them is approved or same user)
bool is_booking_conflict(Booking booking1, Booking booking2);
// check if booking has a conflict with any existing booking
bool has_booking_conflict(Booking booking);
// create a new booking, return Booking with booking_id = 0 if failed
Booking create_booking(Booking new_booking);
// match any
bool match_by_any(Booking *booking, booking_filter_context *filter_context);
// context->search_value should be a pointer to a char array (username)
bool match_by_username(Booking *booking, booking_filter_context *context);
// context->search_value should be a pointer to a room_id (uint32_t)
bool match_by_booking_id(Booking *booking, booking_filter_context *context);
// context->search_value should be a pointer to a room_id (uint32_t)
bool match_by_room_id(Booking *booking, booking_filter_context *filter_context);
// context->search_value should be a pointer to a char array (username)
/// context->user should be a pointer to a User struct
bool match_by_room_is_mask_username(Booking *booking, booking_filter_context *filter_context);
// context->search_value should be a pointer to a room_id (uint32_t)
bool match_by_room_id_from_current_time(Booking *booking, booking_filter_context *filter_context);
// context->search_value should be a pointer to a room_id (uint32_t)
// context->user should be a pointer to a User struct
bool match_by_room_id_from_current_time_and_mask_username(Booking *booking, booking_filter_context *filter_context);
// context->search_value should be a pointer to a TimeRange struct
bool match_by_time_range(Booking *booking, booking_filter_context *filter_context);
// context->search_value should be a pointer to a BookingStatus enum
bool match_by_status(Booking *booking, booking_filter_context *filter_context);
// find the first booking that matches the given filter and filter_context
Booking find_first_booking_by_filter(booking_filter filter, booking_filter_context *filter_context);
// count bookings that match the given filter and search value
int count_bookings_by_filter(booking_filter filter, booking_filter_context *filter_context);
// send bookings that match the given filter and search value to the client
// returns the number of bookings sent, or -1 on error
int send_bookings_by_filter(int socket_fd, booking_filter filter, booking_filter_context *filter_context, int max_bookings_to_send);
void handle_users_bookings_list(int socket_fd, User *user, Header header);
void handle_create_booking(int socket_fd, User *user, Header header);

// returns Booking with booking_id = 0 if failed, otherwise returns the approved booking
Booking approve_booking(Booking booking_to_approve);
// returns Booking with booking_id = 0 if failed, otherwise returns the rejected booking
Booking reject_booking(Booking booking_to_reject);
// handle approve booking request from client
void handle_approve_booking(int socket_fd, User *user, Header header);
// handle reject booking request from client
void handle_reject_booking(int socket_fd, User *user, Header header);

// sends the list of all bookings to the client, regardless of user
void handle_bookings_list_superuser(int socket_fd, User *user, Header header);