#pragma once

#include "./lib/configuration.h"
#include "./lib/protocol.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

typedef enum { USER, SUPERUSER } UserType;

typedef struct {
    char username[USERNAME_MAX_LENGTH];
    char password[PASSWORD_MAX_LENGTH];
    UserType user_type;
} User;

// check if users file, rooms file, and bookings file exist, if not create them
// and try to read server settings
struct sockaddr_in initialize_server();

// if error_code == 0 then exit with EXIT_FAILURE
void print_error_and_exit(const char *error_message, int error_code);

// setup the child process to use the given socket file descriptor
bool setup_for_child_process(int socket_fd);
// return -200 on internal error
// returns -2 if password is incorrect
// returns -1 if username does not exist
// returns 0 if logged in as user
// returns 1 if logged in as superuser
int login(LoginCredentials *credentials);
// return -200 on internal error
// returns -1 if username already exists
// returns 0 if signup successful as user
int signup(LoginCredentials *credentials, UserType user_type);
// return -200 on internal error
// returns -1 if username already exists
// returns 0 if signup successful as user
int signup_user(LoginCredentials *credentials);
// return -200 on internal error
// returns -1 if username already exists
// returns 0 if signup successful as superuser
int signup_superuser(LoginCredentials *credentials);