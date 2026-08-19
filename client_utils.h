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

extern const Handeler HANDLERS[4];

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
void handle_create_room(int socket_fd, User *user);
void handle_list_rooms(int socket_fd, User *user);