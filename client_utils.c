#include "client_utils.h"
#include "./lib/configuration.h"
#include "./lib/protocol.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

const Handeler HANDLERS[16] = {{OPCODE_LOGIN, handle_login},
                               {OPCODE_SIGNUP, handle_signup},
                               {OPCODE_ROOMS_LIST, handle_list_rooms},
                               {OPCODE_CREATE_ROOM, handle_create_room},
                               {OPCODE_CREATE_BOOKING, handle_create_booking},
                               {OPCODE_USERS_BOOKINGS_LIST, handle_users_bookings_list},
                               {OPCODE_LOGOUT, handle_logout},
                               {OPCODE_APPROVE_BOOKING, handle_approve_booking},
                               {OPCODE_REJECT_BOOKING, handle_reject_booking},
                               {OPCODE_BOOKINGS_LIST_SUPERUSER, handle_bookings_list_superuser},
                               {OPCODE_BOOKINGS_LIST_BY_ROOM_ID, handle_bookings_list_room_id},
                               {OPCODE_BOOKINGS_LIST_BY_USERNAME, handle_bookings_list_username},
                               {OPCODE_BOOKINGS_LIST_BY_BOOKING_ID, handle_bookings_list_booking_id},
                               {OPCODE_BOOKINGS_LIST_BY_STATUS, handle_bookings_list_status},
                               {OPCODE_BOOKINGS_LIST_BY_TIME_RANGE, handle_bookings_list_time_range},
                               {OPCODE_FORCE_BOOKING_STATUS, handle_force_booking_status}};

struct sockaddr_in initialize_client() {
    FILE *settings_file = fopen(CLIENT_SETTINGS_FILE_NAME, "r");
    struct sockaddr_in server_address = {0};
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(DEFAULT_SERVER_ADDRESS);
    server_address.sin_port = htons(DEFAULT_SERVER_PORT);
    if (settings_file != NULL) {
        char ip_address[IP_STRING_SIZE];
        int port;
        if (fscanf(settings_file, "%15s:%d", ip_address, &port) == 2) {
            server_address.sin_addr.s_addr = inet_addr(ip_address);
            server_address.sin_port = htons(port);
        }
        fclose(settings_file);
    }
    return server_address;
}

void print_success(const char *message) { printf(C_GREEN C_BOLD "[ OK ] " C_RESET "%s\n", message); }
void print_error(const char *message) { printf(C_RED C_BOLD "[ ERROR ] " C_RESET "%s\n", message); }
void print_warning(const char *message) { printf(C_YELLOW C_BOLD "[ WARNING ] " C_RESET "%s\n", message); }
void print_info(const char *message) { printf(C_CYAN C_BOLD "[ INFO ] " C_RESET "%s\n", message); }
void print_error_and_exit(const char *error_message, int error_code) {
    if (error_code == 0) {
        fprintf(stderr, "%sError: %s%s\n", C_RED C_BOLD, error_message, C_RESET);
        exit(EXIT_FAILURE);
    }
    perror(error_message);
    exit(error_code);
}
void flush_stdin() {
    int bytes_in_buffer = 0;
    ioctl(STDIN_FILENO, FIONREAD, &bytes_in_buffer);
    if (bytes_in_buffer <= 0) {
        return;
    }
    for (int i = 0; i < bytes_in_buffer; i++) {
        getchar();
    }
}

void print_application_info(User user) {
    UserType user_type = user.user_type;
    printf("\n\n/////////////////////////////////////////////////////\n");
    printf("Client Application Information:\n");
    printf("Enter -1 to exit the application.\n");
    printf("Enter 0 to print this information.\n");
    if (user_type == GUEST) {
        printf("You are currently logged in as a GUEST.\n");
    }
    if (user_type == USER || user_type == SUPERUSER) {
        printf("You are logged in as %s with type %d\n", user.username, user_type);
    }
    print_can_do_operations_by_type(user_type);
}
void print_guest_can_do_operations() {
    size_t num_opcodes = sizeof(GUEST_OPCODES) / sizeof(GUEST_OPCODES[0]);
    printf("Allowed operations for GUEST:\n");
    for (size_t i = 0; i < num_opcodes; i++) {
        const char *description = get_opcode_description(GUEST_OPCODES[i]);
        printf(" %u. %s\n", GUEST_OPCODES[i], description);
    }
}
void print_user_can_do_operations() {
    size_t num_opcodes = sizeof(USER_OPCODES) / sizeof(USER_OPCODES[0]);
    printf("Allowed operations for USER:\n");
    for (size_t i = 0; i < num_opcodes; i++) {
        const char *description = get_opcode_description(USER_OPCODES[i]);
        printf(" %u. %s\n", USER_OPCODES[i], description);
    }
}
void print_superuser_can_do_operations() {
    size_t num_opcodes = sizeof(SUPERUSER_OPCODES) / sizeof(SUPERUSER_OPCODES[0]);
    printf("Allowed operations for SUPERUSER:\n");
    for (size_t i = 0; i < num_opcodes; i++) {
        const char *description = get_opcode_description(SUPERUSER_OPCODES[i]);
        printf(" %u. %s\n", SUPERUSER_OPCODES[i], description);
    }
}
void print_can_do_operations_by_type(UserType user_type) {
    switch (user_type) {
    case GUEST:
        print_guest_can_do_operations();
        break;
    case USER:
        print_user_can_do_operations();
        break;
    case SUPERUSER:
        print_superuser_can_do_operations();
        break;
    default:
        fprintf(stderr, "Unknown user type: %d\n", user_type);
    }
}

void handle_login(int socket_fd, User *user) {
    // getting username and password from user input
    LoginCredentials credentials = {0};
    printf("Enter username: ");
    fgets(credentials.username, USERNAME_MAX_LENGTH, stdin);
    printf("Enter password: ");
    fgets(credentials.password, PASSWORD_MAX_LENGTH, stdin);
    // send login request to server
    credentials = credentials_hton(sanitize_credentials(credentials));
    Header header = {OPCODE_LOGIN, sizeof(credentials)};
    int bytes_sent = send_header_and_payload(socket_fd, header, (const char *)&credentials);
    if (bytes_sent < HEADER_SIZE + header.payload_size) {
        print_error_and_exit("Failed to send complete header and payload to server. Terminating.", CLIENT_ERROR_WRITE);
    }
    // Response
    Header response_header = {0};
    User response_user = {0};
    int bytes_read = read_exact(socket_fd, &response_header, HEADER_SIZE);
    if (bytes_read < HEADER_SIZE) {
        print_error_and_exit("Failed to read response header from server. Terminating.", CLIENT_ERROR_READ);
    }
    response_header = header_ntoh(response_header);
    if (response_header.operation == OPCODE_ERROR && response_header.payload_size == 0) {
        print_error("login failed, check your username and password\n");
        return;
    }
    if (response_header.operation != OPCODE_OK || response_header.payload_size != sizeof(response_user)) {
        print_error_and_exit("Received unexpected response from server. Terminating.", CLIENT_ERROR_READ);
    }
    bytes_read = read_exact(socket_fd, &response_user, response_header.payload_size);
    if (bytes_read < response_header.payload_size) {
        print_error_and_exit("Failed to read complete response payload from server. Terminating.", CLIENT_ERROR_READ);
    }
    *user = user_ntoh(response_user);
    print_success("Login successful.");
    print_application_info(*user);
}
void handle_signup(int socket_fd, User *user) {
    // getting username and password from user input
    LoginCredentials credentials = {0};
    printf("Username and password must be between %d and %d characters long.\n", USERNAME_MIN_LENGTH, USERNAME_MAX_LENGTH - 1);
    printf("Enter username: ");
    fgets(credentials.username, USERNAME_MAX_LENGTH, stdin);
    printf("Enter password: ");
    fgets(credentials.password, PASSWORD_MAX_LENGTH, stdin);
    // send signup request to server
    Header header = {OPCODE_SIGNUP, sizeof(credentials)};
    credentials = credentials_hton(sanitize_credentials(credentials));
    int bytes_sent = send_header_and_payload(socket_fd, header, (const char *)&credentials);
    if (bytes_sent < HEADER_SIZE + header.payload_size) {
        print_error_and_exit("Failed to send complete header and payload to server. Terminating.", CLIENT_ERROR_WRITE);
    }
    Header response_header = {0};
    User response_user = {0};
    int bytes_read = read_exact(socket_fd, &response_header, HEADER_SIZE);
    if (bytes_read < HEADER_SIZE) {
        print_error_and_exit("Failed to read response header from server. Terminating.", CLIENT_ERROR_READ);
    }
    response_header = header_ntoh(response_header);
    // Response (sending is successful)
    if (response_header.operation == OPCODE_ERROR && response_header.payload_size == 0) {
        print_error("signup failed, try with different credentials\n");
        return;
    }
    if (response_header.operation != OPCODE_OK || response_header.payload_size != sizeof(response_user)) {
        print_error_and_exit("Received unexpected response from server. Terminating.", CLIENT_ERROR_READ);
    }
    bytes_read = read_exact(socket_fd, &response_user, response_header.payload_size);
    if (bytes_read < response_header.payload_size) {
        print_error_and_exit("Failed to read complete response payload from server. Terminating.", CLIENT_ERROR_READ);
    }
    *user = user_ntoh(response_user);
    print_success("Signup successful.");
    print_application_info(*user);
}
void handle_logout(int socket_fd, User *user) { 
    Header header = {OPCODE_LOGOUT, 0};
    int bytes_sent = send_header_and_payload(socket_fd, header, NULL);
    if (bytes_sent < HEADER_SIZE) {
        print_error_and_exit("Failed to send logout request to server. Terminating.", CLIENT_ERROR_WRITE);
    }
    Header response_header = {0};
    int bytes_read = read_exact(socket_fd, &response_header, HEADER_SIZE);
    if (bytes_read < HEADER_SIZE) {
        print_error_and_exit("Failed to read response header from server. Terminating.", CLIENT_ERROR_READ);
    }
    response_header = header_ntoh(response_header);
    if (response_header.operation == OPCODE_ERROR && response_header.payload_size == 0) {
        print_error("Logout failed. An error occurred on the server.\n");
        return;
    }
    if (response_header.operation != OPCODE_OK || response_header.payload_size != 0) {
        print_error_and_exit("Received unexpected response from server. Terminating.", CLIENT_ERROR_READ);
    }
    strcpy(user->username, "guest");
    user->user_type = GUEST;
    print_warning("Logged out successfully.\n");
}

void print_room_header() {
    printf("\n" C_BOLD "%-5s | %-32s" C_RESET "\n", "ID", "Name");
    printf("----- + --------------------------------\n");
}
void print_room(Room room, int row_index) {
    const char *row_color = (row_index % 2 == 0) ? C_RESET : C_CYAN;
    printf("%s%-5u | %-32s%s\n", row_color, room.room_id, room.room_name, C_RESET);
}
void handle_create_room(int socket_fd, User *user) {
    Room new_room = {0};
    printf("Enter room name: ");
    fgets(new_room.room_name, ROOM_NAME_MAX_LENGTH, stdin);
    Header header = {OPCODE_CREATE_ROOM, sizeof(new_room)};
    new_room = room_hton(new_room);
    int bytes_sent = send_header_and_payload(socket_fd, header, (const char *)&new_room);
    if (bytes_sent < HEADER_SIZE + header.payload_size) {
        print_error_and_exit("Failed to send complete header and payload to server. Terminating.", CLIENT_ERROR_WRITE);
    }
    Header response_header = {0};
    int bytes_read = read_exact(socket_fd, &response_header, HEADER_SIZE);
    if (bytes_read < HEADER_SIZE) {
        print_error_and_exit("Failed to read response header from server. Terminating.", CLIENT_ERROR_READ);
    }
    response_header = header_ntoh(response_header);
    if (response_header.operation == OPCODE_ERROR && response_header.payload_size == 0) {
        print_error("Create room failed. Room may already exist or an error occurred.\n");
        return;
    }
    if (response_header.operation != OPCODE_OK || response_header.payload_size != sizeof(Room)) {
        print_error_and_exit("Received unexpected response from server. Terminating.", CLIENT_ERROR_READ);
    }
    Room created_room = {0};
    bytes_read = read_exact(socket_fd, &created_room, response_header.payload_size);
    if (bytes_read < response_header.payload_size) {
        print_error_and_exit("Failed to read complete response payload from server. Terminating.", CLIENT_ERROR_READ);
    }
    created_room = room_ntoh(created_room);
    print_success("Room created successfully:\n");
    print_room_header();
    print_room(created_room, 1);
}
void handle_list_rooms(int socket_fd, User *user) {
    Header header = {OPCODE_ROOMS_LIST, 0};
    int bytes_sent = send_header_and_payload(socket_fd, header, NULL);
    if (bytes_sent < HEADER_SIZE) {
        print_error_and_exit("Failed to send header to server. Terminating.", CLIENT_ERROR_WRITE);
    }
    Header response_header = {0};
    int bytes_read = read_exact(socket_fd, &response_header, HEADER_SIZE);
    if (bytes_read < HEADER_SIZE) {
        print_error_and_exit("Failed to read response header from server. Terminating.", CLIENT_ERROR_READ);
    }
    response_header = header_ntoh(response_header);
    if (response_header.operation == OPCODE_ERROR && response_header.payload_size == 0) {
        print_error("List rooms failed. An error occurred on the server.\n");
        return;
    }
    if (response_header.operation != OPCODE_OK || response_header.payload_size % sizeof(Room) != 0) {
        print_error_and_exit("Received unexpected response from server. Terminating.", CLIENT_ERROR_READ);
    }
    printf("Rooms list:\n");
    size_t room_count = response_header.payload_size / sizeof(Room);
    Room room = {0};
    if (room_count == 0) {
        print_info("No rooms found in database.\n");
        return;
    }
    print_room_header();
    for (size_t i = 0; i < room_count; i++) {
        bytes_read = read_exact(socket_fd, &room, sizeof(Room));
        if (bytes_read < sizeof(Room)) {
            print_error_and_exit("Failed to read complete room from server. Terminating.", CLIENT_ERROR_READ);
        }
        room = room_ntoh(room);
        print_room(room, i);
    }
}

char *booking_status_to_string(uint8_t status) {
    switch (status) {
    case PENDING:
        return "[🟡] PENDING";
    case APPROVED:
        return "[🟢] APPROVED";
    case REJECTED:
        return "[🔴] REJECTED";
    default:
        return "[❓] UNKNOWN";
    }
}
void print_booking_header() {
    printf("\n" C_BOLD "%-5s | %-7s | %-32s | %-16s | %-16s | %-16s" C_RESET "\n", "ID", "Room ID", "Username", "Start Time", "End Time",
           "Status");
    printf("----- + ------- + -------------------------------- + -------------------- + -------------------- + ----------------\n");
}
void print_booking(Booking booking, int row_index) {
    const char *row_color = (row_index % 2 == 0) ? C_RESET : C_CYAN;
    char booking_id_str[16] = "???";
    if (booking.booking_id > 0) {
        snprintf(booking_id_str, sizeof(booking_id_str), "%u", booking.booking_id);
    }
    char *status_str = booking_status_to_string(booking.status);
    char start_time_str[32];
    struct tm *start_tm = localtime((time_t *)&booking.start_time);
    strftime(start_time_str, sizeof(start_time_str), "%d/%m/%Y %H:%M", start_tm);
    char end_time_str[32];
    struct tm *end_tm = localtime((time_t *)&booking.end_time);
    strftime(end_time_str, sizeof(end_time_str), "%d/%m/%Y %H:%M", end_tm);
    char is_expired_str[32] = "";
    // dd/mm/yyyy hh:mm format

    if (is_time_in_past(booking.end_time)) {
        strcpy(is_expired_str, " (expired)");
    }

    printf("%s%-5s | %-7u | %-32s | %-16s | %-16s | %-16s%s%s\n", row_color, booking_id_str, booking.room_id, booking.username,
           start_time_str, end_time_str, status_str, is_expired_str, C_RESET);
}
bool is_valid_date(int day, int month, int year) {
    if (year < 1900 || year > 2100) {
        return false;
    }
    if (month < 1 || month > 12) {
        return false;
    }
    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
        days_in_month[1] = 29; // Leap year
    }
    if (day < 1 || day > days_in_month[month - 1]) {
        return false;
    }
    return true;
}
bool is_valid_time(int hour) { return hour >= 0 && hour <= 23; }
bool is_time_in_past(time_t start_time) {
    time_t current_time = time(NULL);
    return start_time < current_time;
}
// client sends struct Room
// server sends masked bookings for the room
// client sends struct Booking
// server sends struct Booking with booking_id and status
void handle_create_booking(int socket_fd, User *user) {
    Room room = {0};
    char buffer_input[64];
    printf("Enter room ID to create a booking: ");
    fgets(buffer_input, sizeof(buffer_input), stdin);
    sscanf(buffer_input, "%u", &room.room_id);
    Header header = {OPCODE_CREATE_BOOKING, sizeof(room)};
    room = room_hton(room);
    int bytes_sent = send_header_and_payload(socket_fd, header, (const char *)&room);
    if (bytes_sent < HEADER_SIZE + header.payload_size) {
        print_error_and_exit("Failed to send complete header and payload to server. Terminating.", CLIENT_ERROR_WRITE);
    }
    Header response_header = {0};
    int bytes_read = read_exact(socket_fd, &response_header, HEADER_SIZE);
    if (bytes_read < HEADER_SIZE) {
        print_error_and_exit("Failed to read response header from server. Terminating.", CLIENT_ERROR_READ);
    }
    response_header = header_ntoh(response_header);
    if (response_header.operation == OPCODE_ERROR && response_header.payload_size == 0) {
        print_error("Create booking failed.\n");
        return;
    }
    if (response_header.operation != OPCODE_OK || response_header.payload_size % sizeof(Booking) != 0) {
        print_error_and_exit("Received unexpected response from server. Terminating.", CLIENT_ERROR_READ);
    }
    // print existing bookings for the room
    int booking_count = response_header.payload_size / sizeof(Booking);
    room = room_ntoh(room);
    printf("Existing bookings for room ID %u:\n", room.room_id);
    if (booking_count == 0) {
        printf("No existing bookings for room ID %u.\n", room.room_id);
    } else {
        print_booking_header();
        for (int i = 0; i < booking_count; i++) {
            Booking booking = {0};
            bytes_read = read_exact(socket_fd, &booking, sizeof(Booking));
            if (bytes_read < sizeof(Booking)) {
                print_error_and_exit("Failed to read complete booking from server. Terminating.", CLIENT_ERROR_READ);
            }
            booking = booking_ntoh(booking);
            print_booking(booking, i);
        }
    }
    // get booking details from user
    printf("Enter -1 to cancel booking creation or any other input to proceed: ");
    fgets(buffer_input, sizeof(buffer_input), stdin);
    if (atoi(buffer_input) == -1) {
        printf("Booking creation cancelled.\n");
        Header cancel_header = {OPCODE_CANCEL, 0};
        send_header_and_payload(socket_fd, cancel_header, NULL);
        return;
    }
    Booking new_booking = {0};
    new_booking.room_id = room.room_id;
    printf("Enter booking date (DD/MM/YYYY): ");
    int day, month, year, start_hour, time_slot_count;
    fgets(buffer_input, sizeof(buffer_input), stdin);
    int read_count = sscanf(buffer_input, "%d/%d/%d", &day, &month, &year);
    if (read_count != 3) {
        printf("Invalid date format. Booking creation cancelled.\n");
        Header cancel_header = {OPCODE_CANCEL, 0};
        send_header_and_payload(socket_fd, cancel_header, NULL);
        return;
    }
    printf("Enter booking start time (HH) as integer: ");
    fgets(buffer_input, sizeof(buffer_input), stdin);
    read_count = sscanf(buffer_input, "%d", &start_hour);
    if (read_count != 1) {
        printf("Invalid start time format. Booking creation cancelled.\n");
        Header cancel_header = {OPCODE_CANCEL, 0};
        send_header_and_payload(socket_fd, cancel_header, NULL);
        return;
    }
    printf("Enter number of time slots for the booking (1 slot = 1 hour): ");
    fgets(buffer_input, sizeof(buffer_input), stdin);
    read_count = sscanf(buffer_input, "%d", &time_slot_count);
    if (read_count != 1 || time_slot_count <= 0) {
        printf("Invalid time slot count. Booking creation cancelled.\n");
        Header cancel_header = {OPCODE_CANCEL, 0};
        send_header_and_payload(socket_fd, cancel_header, NULL);
        return;
    }
    struct tm time_info = {0};
    time_info.tm_mday = day;
    time_info.tm_mon = month - 1;
    time_info.tm_year = year - 1900;
    time_info.tm_hour = start_hour;
    time_info.tm_min = 0;
    time_info.tm_sec = 0;
    time_info.tm_isdst = -1; // Not considering daylight saving time
    new_booking.start_time = mktime(&time_info);
    if (is_time_in_past(new_booking.start_time)) {
        printf("Booking start time is in the past. Booking creation cancelled.\n");
        Header cancel_header = {OPCODE_CANCEL, 0};
        send_header_and_payload(socket_fd, cancel_header, NULL);
        return;
    }
    new_booking.end_time = new_booking.start_time + (time_slot_count * BOOKING_TIME_SLOT_SECONDS);
    response_header = (Header){OPCODE_CREATE_BOOKING, sizeof(Booking)};
    new_booking = booking_hton(new_booking);
    bytes_sent = send_header_and_payload(socket_fd, response_header, (const char *)&new_booking);
    if (bytes_sent < HEADER_SIZE + response_header.payload_size) {
        print_error_and_exit("Failed to send complete header and payload to server. Terminating.", CLIENT_ERROR_WRITE);
    }
    bytes_read = read_exact(socket_fd, &response_header, HEADER_SIZE);
    if (bytes_read < HEADER_SIZE) {
        print_error_and_exit("Failed to read response header from server. Terminating.", CLIENT_ERROR_READ);
    }
    response_header = header_ntoh(response_header);
    if (response_header.operation == OPCODE_ERROR && response_header.payload_size == 0) {
        print_error("Create booking failed.\n");
        return;
    }
    if (response_header.operation != OPCODE_OK || response_header.payload_size != sizeof(Booking)) {
        print_error_and_exit("Received unexpected response from server. Terminating.", CLIENT_ERROR_READ);
    }
    Booking created_booking = {0};
    bytes_read = read_exact(socket_fd, &created_booking, response_header.payload_size);
    if (bytes_read < response_header.payload_size) {
        print_error_and_exit("Failed to read complete response payload from server. Terminating.", CLIENT_ERROR_READ);
    }
    print_success("Booking created successfully:\n");
    created_booking = booking_ntoh(created_booking);
    print_booking_header();
    print_booking(created_booking, 1);
}
void handle_users_bookings_list(int socket_fd, User *user) {
    Header header = {OPCODE_USERS_BOOKINGS_LIST, 0};
    int bytes_sent = send_header_and_payload(socket_fd, header, NULL);
    if (bytes_sent < HEADER_SIZE) {
        print_error_and_exit("Failed to send header to server. Terminating.", CLIENT_ERROR_WRITE);
    }
    int bytes_read = read_exact(socket_fd, &header, HEADER_SIZE);
    if (bytes_read < HEADER_SIZE) {
        print_error_and_exit("Failed to read response header from server. Terminating.", CLIENT_ERROR_READ);
    }
    header = header_ntoh(header);
    if (header.operation == OPCODE_ERROR && header.payload_size == 0) {
        print_error("Failed to retrieve bookings list.\n");
        return;
    }
    if (header.operation != OPCODE_OK || header.payload_size % sizeof(Booking) != 0) {
        print_error_and_exit("Received unexpected response from server. Terminating.", CLIENT_ERROR_READ);
    }
    int list_size = header.payload_size / sizeof(Booking);
    if (list_size == 0) {
        printf("No bookings found for user: %s\n", user->username);
        return;
    }
    printf("Bookings List for user: %s\n", user->username);
    printf("Total bookings: %d\n", list_size);
    print_booking_header();
    bytes_read = 0;
    for (int i = 0; i < list_size; i++) {
        Booking booking = {0};
        bytes_read = read_exact(socket_fd, &booking, sizeof(Booking));
        if (bytes_read < sizeof(Booking)) {
            print_error_and_exit("Failed to read complete booking from server. Terminating.", CLIENT_ERROR_READ);
        }
        booking = booking_ntoh(booking);
        char *start_time_str = ctime((const time_t *)&booking.start_time);
        char *end_time_str = ctime((const time_t *)&booking.end_time);
        print_booking(booking, i);
    }
}

void handle_approve_booking(int socket_fd, User *user) {
    Booking booking = {0};
    print_warning("Approving a booking, will cause conflicting bookings to be rejected.\n");
    printf("Enter booking ID to approve: ");
    char buffer_input[64];
    fgets(buffer_input, sizeof(buffer_input), stdin);
    uint32_t booking_id;
    sscanf(buffer_input, "%u", &booking_id);
    if (booking_id == 0) {
        printf("Invalid booking ID. Approval cancelled.\n");
        return;
    }
    booking.booking_id = booking_id;
    Header header = {OPCODE_APPROVE_BOOKING, sizeof(booking)};
    booking = booking_hton(booking);
    int bytes_sent = send_header_and_payload(socket_fd, header, (const char *)&booking);
    if (bytes_sent < HEADER_SIZE + header.payload_size) {
        print_error_and_exit("Failed to send complete header and payload to server. Terminating.", CLIENT_ERROR_WRITE);
    }
    Header response_header = {0};
    int bytes_read = read_exact(socket_fd, &response_header, HEADER_SIZE);
    if (bytes_read < HEADER_SIZE) {
        print_error_and_exit("Failed to read response header from server. Terminating.", CLIENT_ERROR_READ);
    }
    response_header = header_ntoh(response_header);
    if (response_header.operation == OPCODE_ERROR && response_header.payload_size == 0) {
        print_error("Approve booking failed.\n");
        return;
    }
    if (response_header.operation != OPCODE_OK || response_header.payload_size != sizeof(Booking)) {
        print_error_and_exit("Received unexpected response from server. Terminating.", CLIENT_ERROR_READ);
    }
    Booking response_booking = {0};
    bytes_read = read_exact(socket_fd, &response_booking, response_header.payload_size);
    if (bytes_read < response_header.payload_size) {
        print_error_and_exit("Failed to read complete response payload from server. Terminating.", CLIENT_ERROR_READ);
    }
    Booking approved_booking = booking_ntoh(response_booking);
    print_success("Booking approved:\n");
    print_booking_header();
    print_booking(approved_booking, 1);
}
void handle_reject_booking(int socket_fd, User *user) {
    Booking booking = {0};
    print_warning("Enter booking ID to reject: ");
    char buffer_input[64];
    uint32_t booking_id;
    fgets(buffer_input, sizeof(buffer_input), stdin);
    sscanf(buffer_input, "%u", &booking_id);
    if (booking_id == 0) {
        printf("Invalid booking ID. Rejection cancelled.\n");
        return;
    }
    booking.booking_id = booking_id;
    Header header = {OPCODE_REJECT_BOOKING, sizeof(booking)};
    booking = booking_hton(booking);
    int bytes_sent = send_header_and_payload(socket_fd, header, (const char *)&booking);
    if (bytes_sent < HEADER_SIZE + header.payload_size) {
        print_error_and_exit("Failed to send complete header and payload to server. Terminating.", CLIENT_ERROR_WRITE);
    }
    Header response_header = {0};
    int bytes_read = read_exact(socket_fd, &response_header, HEADER_SIZE);
    if (bytes_read < HEADER_SIZE) {
        print_error_and_exit("Failed to read response header from server. Terminating.", CLIENT_ERROR_READ);
    }
    response_header = header_ntoh(response_header);
    if (response_header.operation == OPCODE_ERROR && response_header.payload_size == 0) {
        print_error("Reject booking failed.\n");
        return;
    }
    if (response_header.operation != OPCODE_OK || response_header.payload_size != sizeof(Booking)) {
        print_error_and_exit("Received unexpected response from server. Terminating.", CLIENT_ERROR_READ);
    }
    Booking response_booking = {0};
    bytes_read = read_exact(socket_fd, &response_booking, response_header.payload_size);
    if (bytes_read < response_header.payload_size) {
        print_error_and_exit("Failed to read complete response payload from server. Terminating.", CLIENT_ERROR_READ);
    }
    Booking rejected_booking = booking_ntoh(response_booking);
    print_success("Booking rejected:\n");
    print_booking_header();
    print_booking(rejected_booking, 1);
}

void handle_bookings_list_superuser(int socket_fd, User *user) {
    printf("Listing all bookings for superuser...\n");
    Header header = {OPCODE_BOOKINGS_LIST_SUPERUSER, 0};
    int bytes_sent = send_header_and_payload(socket_fd, header, NULL);
    if (bytes_sent < HEADER_SIZE) {
        print_error_and_exit("Failed to send header to server. Terminating.", CLIENT_ERROR_WRITE);
    }
    int bytes_read = read_exact(socket_fd, &header, HEADER_SIZE);
    if (bytes_read < HEADER_SIZE) {
        print_error_and_exit("Failed to read response header from server. Terminating.", CLIENT_ERROR_READ);
    }
    header = header_ntoh(header);
    if (header.operation == OPCODE_ERROR && header.payload_size == 0) {
        print_info("Failed to retrieve bookings list.\n");
        return;
    }
    if (header.operation != OPCODE_OK || header.payload_size % sizeof(Booking) != 0) {
        print_error_and_exit("Received unexpected response from server. Terminating.", CLIENT_ERROR_READ);
    }
    int list_size = header.payload_size / sizeof(Booking);
    if (list_size == 0) {
        print_info("No bookings found in the system.\n");
        return;
    }
    printf("Bookings List:\n");
    printf("Total bookings: %d\n", list_size);
    print_booking_header();
    bytes_read = 0;
    for (int i = 0; i < list_size; i++) {
        Booking booking = {0};
        bytes_read = read_exact(socket_fd, &booking, sizeof(Booking));
        if (bytes_read < sizeof(Booking)) {
            print_error_and_exit("Failed to read complete booking from server. Terminating.", CLIENT_ERROR_READ);
        }
        booking = booking_ntoh(booking);
        print_booking(booking, i);
    }
}

void handle_bookings_list_room_id(int socket_fd, User *user) {
    printf("Listing bookings by room ID...\n");
    Room room = {0};
    char buffer_input[64];
    printf("Enter room ID to list bookings: ");
    fgets(buffer_input, sizeof(buffer_input), stdin);
    int read_count = 0, room_id = 0;
    read_count = sscanf(buffer_input, "%u", &room_id);
    if (read_count != 1 || room_id <= 0) {
        printf("Invalid room ID. Listing cancelled.\n");
        return;
    }
    room.room_id = room_id;
    Header header = {OPCODE_BOOKINGS_LIST_BY_ROOM_ID, sizeof(room)};
    room = room_hton(room);
    int bytes_sent = send_header_and_payload(socket_fd, header, (const char *)&room);
    if (bytes_sent < HEADER_SIZE + header.payload_size) {
        print_error_and_exit("Failed to send complete header and payload to server. Terminating.", CLIENT_ERROR_WRITE);
    }
    int bytes_read = read_exact(socket_fd, &header, HEADER_SIZE);
    if (bytes_read < HEADER_SIZE) {
        print_error_and_exit("Failed to read response header from server. Terminating.", CLIENT_ERROR_READ);
    }
    header = header_ntoh(header);
    if (header.operation == OPCODE_ERROR && header.payload_size == 0) {
        print_error("Failed to retrieve bookings list.\n");
        return;
    }
    if (header.operation != OPCODE_OK || header.payload_size % sizeof(Booking) != 0) {
        print_error_and_exit("Received unexpected response from server. Terminating.", CLIENT_ERROR_READ);
    }
    int list_size = header.payload_size / sizeof(Booking);
    if (list_size == 0) {
        print_info("No bookings found.\n");
        return;
    }
    printf("Bookings List for room ID: %u\n", room_id);
    printf("Total bookings: %d\n", list_size);
    print_booking_header();
    bytes_read = 0;
    for (int i = 0; i < list_size; i++) {
        Booking booking = {0};
        bytes_read = read_exact(socket_fd, &booking, sizeof(Booking));
        if (bytes_read < sizeof(Booking)) {
            print_error_and_exit("Failed to read complete booking from server. Terminating.", CLIENT_ERROR_READ);
        }
        booking = booking_ntoh(booking);
        print_booking(booking, i);
    }
}
void handle_bookings_list_username(int socket_fd, User *user) {
    printf("Listing bookings by username...\n");
    char username[USERNAME_MAX_LENGTH] = {0};
    printf("Enter username to list bookings: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = 0;
    if (strlen(username) == 0) {
        printf("Invalid username. Listing cancelled.\n");
        return;
    }
    User request_user = {0};
    strncpy(request_user.username, username, USERNAME_MAX_LENGTH - 1);
    Header header = {OPCODE_BOOKINGS_LIST_BY_USERNAME, sizeof(request_user)};
    int bytes_sent = send_header_and_payload(socket_fd, header, (const char *)&request_user);
    if (bytes_sent < HEADER_SIZE + header.payload_size) {
        print_error_and_exit("Failed to send complete header and payload to server. Terminating.", CLIENT_ERROR_WRITE);
    }
    int bytes_read = read_exact(socket_fd, &header, HEADER_SIZE);
    if (bytes_read < HEADER_SIZE) {
        print_error_and_exit("Failed to read response header from server. Terminating.", CLIENT_ERROR_READ);
    }
    header = header_ntoh(header);
    if (header.operation == OPCODE_ERROR && header.payload_size == 0) {
        print_error("Failed to retrieve bookings list.\n");
        return;
    }
    if (header.operation != OPCODE_OK || header.payload_size % sizeof(Booking) != 0) {
        print_error_and_exit("Received unexpected response from server. Terminating.", CLIENT_ERROR_READ);
    }
    int list_size = header.payload_size / sizeof(Booking);
    if (list_size == 0) {
        print_info("No bookings found.\n");
        return;
    }
    printf("Bookings List for username: %s\n", username);
    printf("Total bookings: %d\n", list_size);
    print_booking_header();
    bytes_read = 0;
    for (int i = 0; i < list_size; i++) {
        Booking booking = {0};
        bytes_read = read_exact(socket_fd, &booking, sizeof(Booking));
        if (bytes_read < sizeof(Booking)) {
            print_error_and_exit("Failed to read complete booking from server. Terminating.", CLIENT_ERROR_READ);
        }
        booking = booking_ntoh(booking);
        print_booking(booking, i);
    }
}
void handle_bookings_list_booking_id(int socket_fd, User *user) {
    printf("Listing booking by booking ID...\n");
    Booking request_booking = {0};
    char buffer_input[64];
    printf("Enter booking ID to list: ");
    fgets(buffer_input, sizeof(buffer_input), stdin);
    int booking_id;
    int read_count = sscanf(buffer_input, "%u", &booking_id);
    if (read_count != 1 || booking_id <= 0) {
        printf("Invalid booking ID. Listing cancelled.\n");
        return;
    }
    request_booking.booking_id = booking_id;
    Header header = {OPCODE_BOOKINGS_LIST_BY_BOOKING_ID, sizeof(request_booking)};
    request_booking = booking_hton(request_booking);
    int bytes_sent = send_header_and_payload(socket_fd, header, (const char *)&request_booking);
    if (bytes_sent < HEADER_SIZE + header.payload_size) {
        print_error_and_exit("Failed to send complete header and payload to server. Terminating.", CLIENT_ERROR_WRITE);
    }
    int bytes_read = read_exact(socket_fd, &header, HEADER_SIZE);
    if (bytes_read < HEADER_SIZE) {
        print_error_and_exit("Failed to read response header from server. Terminating.", CLIENT_ERROR_READ);
    }
    Header response_header = header_ntoh(header);
    if (response_header.operation == OPCODE_ERROR && response_header.payload_size == 0) {
        print_error("Failed to retrieve booking.\n");
        return;
    }
    if (response_header.operation != OPCODE_OK) {
        print_error_and_exit("Received unexpected response from server. Terminating.", CLIENT_ERROR_READ);
    }
    if (response_header.payload_size != sizeof(Booking)) {
        print_error_and_exit("Received unexpected response from server. Terminating.", CLIENT_ERROR_READ);
    }
    Booking response_booking = {0};
    bytes_read = read_exact(socket_fd, &response_booking, sizeof(Booking));
    if (bytes_read < sizeof(Booking)) {
        print_error_and_exit("Failed to read complete booking from server. Terminating.", CLIENT_ERROR_READ);
    }
    response_booking = booking_ntoh(response_booking);
    printf("Booking details:\n");
    print_booking_header();
    print_booking(response_booking, 1);
}
void handle_bookings_list_time_range(int socket_fd, User *user) {
    printf("Listing bookings by time range...\n");
    char buffer_input[64];
    int start_day, start_month, start_year, end_day, end_month, end_year;
    printf("Enter start date (DD/MM/YYYY): ");
    fgets(buffer_input, sizeof(buffer_input), stdin);
    int read_count1 = sscanf(buffer_input, "%d/%d/%d", &start_day, &start_month, &start_year);
    printf("Enter end date (DD/MM/YYYY): ");
    fgets(buffer_input, sizeof(buffer_input), stdin);
    int read_count2 = sscanf(buffer_input, "%d/%d/%d", &end_day, &end_month, &end_year);
    if (read_count1 != 3 || read_count2 != 3 || !is_valid_date(start_day, start_month, start_year) ||
        !is_valid_date(end_day, end_month, end_year)) {
        printf("Invalid date(s) provided. Listing cancelled.\n");
        return;
    }
    struct tm start_tm = {0};
    start_tm.tm_mday = start_day;
    start_tm.tm_mon = start_month - 1;
    start_tm.tm_year = start_year - 1900;
    start_tm.tm_hour = 0;
    start_tm.tm_min = 0;
    start_tm.tm_sec = 0;
    time_t start_time = mktime(&start_tm);
    struct tm end_tm = {0};
    end_tm.tm_mday = end_day;
    end_tm.tm_mon = end_month - 1;
    end_tm.tm_year = end_year - 1900;
    end_tm.tm_hour = 23;
    end_tm.tm_min = 59;
    end_tm.tm_sec = 59;
    time_t end_time = mktime(&end_tm);
    TimeRange time_range = {start_time, end_time};
    Header header = {OPCODE_BOOKINGS_LIST_BY_TIME_RANGE, sizeof(time_range)};
    time_range = time_range_hton(time_range);
    int bytes_sent = send_header_and_payload(socket_fd, header, (const char *)&time_range);
    if (bytes_sent < HEADER_SIZE + header.payload_size) {
        print_error_and_exit("Failed to send complete header and payload to server. Terminating.", CLIENT_ERROR_WRITE);
    }
    int bytes_read = read_exact(socket_fd, &header, HEADER_SIZE);
    if (bytes_read < HEADER_SIZE) {
        print_error_and_exit("Failed to read response header from server. Terminating.", CLIENT_ERROR_READ);
    }
    header = header_ntoh(header);
    if (header.operation == OPCODE_ERROR || header.payload_size == 0) {
        print_error("Failed to retrieve bookings list.\n");
        return;
    }
    if (header.payload_size % sizeof(Booking) != 0 || header.operation != OPCODE_OK) {
        print_error_and_exit("Received unexpected response from server. Terminating.", CLIENT_ERROR_READ);
    }
    int list_size = header.payload_size / sizeof(Booking);
    if (list_size == 0) {
        print_info("No bookings found in the specified time range.\n");
        return;
    }
    printf("Bookings List in the specified time range:\n");
    printf("Total bookings: %d\n", list_size);
    print_booking_header();
    bytes_read = 0;
    for (int i = 0; i < list_size; i++) {
        Booking booking = {0};
        bytes_read = read_exact(socket_fd, &booking, sizeof(Booking));
        if (bytes_read < sizeof(Booking)) {
            print_error_and_exit("Failed to read complete booking from server. Terminating.", CLIENT_ERROR_READ);
        }
        booking = booking_ntoh(booking);
        print_booking(booking, i);
    }
}
void handle_bookings_list_status(int socket_fd, User *user) {
    printf("Listing bookings by status...\n");
    char buffer_input[64];
    int status;
    printf("Enter booking status (0: Pending, 1: Approved, 2: Rejected): ");
    fgets(buffer_input, sizeof(buffer_input), stdin);
    int read_count = sscanf(buffer_input, "%d", &status);
    if (read_count != 1 || status > 2 || status < 0) {
        printf("Invalid booking status. Listing cancelled.\n");
        return;
    }
    Header header = {OPCODE_BOOKINGS_LIST_BY_STATUS, sizeof(status)};
    int bytes_sent = send_header_and_payload(socket_fd, header, (const char *)&status);
    if (bytes_sent < HEADER_SIZE + header.payload_size) {
        print_error_and_exit("Failed to send complete header and payload to server. Terminating.", CLIENT_ERROR_WRITE);
    }
    int bytes_read = read_exact(socket_fd, &header, HEADER_SIZE);
    if (bytes_read < HEADER_SIZE) {
        print_error_and_exit("Failed to read response header from server. Terminating.", CLIENT_ERROR_READ);
    }
    header = header_ntoh(header);
    if (header.operation == OPCODE_ERROR || header.payload_size == 0) {
        print_error("Failed to retrieve bookings list.\n");
        return;
    }
    if (header.payload_size % sizeof(Booking) != 0 || header.operation != OPCODE_OK) {
        print_error_and_exit("Received unexpected response from server. Terminating.", CLIENT_ERROR_READ);
    }
    int list_size = header.payload_size / sizeof(Booking);
    if (list_size == 0) {
        print_info("No bookings found with the specified status.\n");
        return;
    }
    printf("Bookings List with status %s:\n", booking_status_to_string(status));
    printf("Total bookings: %d\n", list_size);
    print_booking_header();
    bytes_read = 0;
    for (int i = 0; i < list_size; i++) {
        Booking booking = {0};
        bytes_read = read_exact(socket_fd, &booking, sizeof(Booking));
        if (bytes_read < sizeof(Booking)) {
            print_error_and_exit("Failed to read complete booking from server. Terminating.", CLIENT_ERROR_READ);
        }
        booking = booking_ntoh(booking);
        print_booking(booking, i);
    }
}

void handle_force_booking_status(int socket_fd, User *user) {
    print_warning("Force change booking status...\n");
    print_warning("THIS OPERATION CAN LEAD TO INCONSISTENT DATABASE\n");
    Booking request_booking = {0};
    char buffer_input[64];
    printf("Enter booking ID to change status: ");
    fgets(buffer_input, sizeof(buffer_input), stdin);
    int booking_id;
    int read_count = sscanf(buffer_input, "%u", &booking_id);
    if (read_count != 1 || booking_id <= 0) {
        printf("Invalid booking ID. Operation cancelled.\n");
        return;
    }
    request_booking.booking_id = booking_id;
    printf("Enter new booking status (0: Pending, 1: Approved, 2: Rejected): ");
    fgets(buffer_input, sizeof(buffer_input), stdin);
    read_count = sscanf(buffer_input, "%d", (int *)&request_booking.status);
    if (read_count != 1 || request_booking.status > 2 || request_booking.status < 0) {
        printf("Invalid booking status. Operation cancelled.\n");
        return;
    }
    Header header = {OPCODE_FORCE_BOOKING_STATUS, sizeof(request_booking)};
    request_booking = booking_hton(request_booking);
    int bytes_sent = send_header_and_payload(socket_fd, header, (const char *)&request_booking);
    if (bytes_sent < HEADER_SIZE + header.payload_size) {
        print_error_and_exit("Failed to send complete header and payload to server. Terminating.", CLIENT_ERROR_WRITE);
    }
    Header response_header = {0};
    int bytes_read = read_exact(socket_fd, &response_header, HEADER_SIZE);
    if (bytes_read < HEADER_SIZE) {
        print_error_and_exit("Failed to read response header from server. Terminating.", CLIENT_ERROR_READ);
    }
    response_header = header_ntoh(response_header);
    if (response_header.operation == OPCODE_ERROR) {
        print_error("Force change booking status failed.\n");
        return;
    }
    if (response_header.operation != OPCODE_OK || response_header.payload_size != sizeof(Booking)) {
        print_error_and_exit("Received unexpected response from server. Terminating.", CLIENT_ERROR_READ);
    }
    Booking response_booking = {0};
    bytes_read = read_exact(socket_fd, &response_booking, response_header.payload_size);
    if (bytes_read < response_header.payload_size) {
        print_error_and_exit("Failed to read complete response payload from server. Terminating.", CLIENT_ERROR_READ);
    }
    response_booking = booking_ntoh(response_booking);
    print_success("Booking status updated:\n");
    print_booking_header();
    print_booking(response_booking, 1);
}

// EOF