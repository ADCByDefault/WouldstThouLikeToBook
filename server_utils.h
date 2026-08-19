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

typedef struct User_Save {
    char username[USERNAME_MAX_LENGTH];
    char password[PASSWORD_MAX_LENGTH];
    UserType user_type;
} User_Save;

typedef void (*OperationHandler)(int socket_fd, User *user, Header header);
typedef struct Handeler {
    OpCode opcode;
    OperationHandler handler;
} Handeler;

extern const Handeler HANDLERS[4];

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