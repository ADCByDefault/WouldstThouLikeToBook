#pragma once

#include "./lib/configuration.h"
#include "./lib/protocol.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef struct {
    char username[USERNAME_MAX_LENGTH];
    UserType user_type;
} User;

// initializes the client socket and returns the socket file descriptor
struct sockaddr_in initialize_client();

// if error_code == 0 then exit with EXIT_FAILURE
void print_error_and_exit(const char *error_message, int error_code);

int login(LoginCredentials *credentials);
int signup(LoginCredentials *credentials, UserType user_type);