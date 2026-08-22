#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "./lib/configuration.h"
#include "./lib/protocol.h"
#include "./server_utils.h"

const Handler HANDLERS[16] = {
    {OPCODE_LOGIN, handle_login},
    {OPCODE_SIGNUP, handle_signup},
    {OPCODE_CREATE_ROOM, handle_create_room},
    {OPCODE_ROOMS_LIST, handle_list_rooms},
    {OPCODE_USERS_BOOKINGS_LIST, handle_users_bookings_list},
    {OPCODE_CREATE_BOOKING, handle_create_booking},
    {OPCODE_APPROVE_BOOKING, handle_approve_booking},
    {OPCODE_REJECT_BOOKING, handle_reject_booking},
    {OPCODE_LOGOUT, handle_logout},
    {OPCODE_BOOKINGS_LIST_SUPERUSER, handle_bookings_list_superuser},
    {OPCODE_BOOKINGS_LIST_BY_ROOM_ID, handle_bookings_list_room_id},
    {OPCODE_BOOKINGS_LIST_BY_USERNAME, handle_bookings_list_username},
    {OPCODE_BOOKINGS_LIST_BY_BOOKING_ID, handle_bookings_list_booking_id},
    {OPCODE_BOOKINGS_LIST_BY_STATUS, handle_bookings_list_status},
    {OPCODE_BOOKINGS_LIST_BY_TIME_RANGE, handle_bookings_list_time_range},
    {OPCODE_FORCE_BOOKING_STATUS, handle_force_booking_status},
};

struct sockaddr_in initialize_server() {
    // making sure data files exist, if not create them
    FILE *users_file = fopen(USERS_FILE_NAME, "ab");
    if (users_file == NULL) {
        return (struct sockaddr_in){0}; // Error creating users file
    }
    fclose(users_file);

    FILE *rooms_file = fopen(ROOMS_FILE_NAME, "ab");
    if (rooms_file == NULL) {
        return (struct sockaddr_in){0}; // Error creating rooms file
    }
    fclose(rooms_file);

    FILE *bookings_file = fopen(BOOKINGS_FILE_NAME, "ab");
    if (bookings_file == NULL) {
        return (struct sockaddr_in){0}; // Error creating bookings file
    }
    fclose(bookings_file);

    // Creating admin user
    User admin = signup((LoginCredentials){"admin", "admin"}, SUPERUSER);

    // Read server settings from file
    FILE *settings_file = fopen(SERVER_SETTINGS_FILE_NAME, "r");
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

void print_error_and_exit(const char *error_message, int error_code) {
    if (error_code == 0) {
        fprintf(stderr, "Error: %s\n", error_message);
        exit(EXIT_FAILURE);
    }
    perror(error_message);
    exit(error_code);
}

bool setup_for_child_process(int socket_fd) {
    // setting up file descriptor for child process
    if (socket_fd < 0) {
        return false;
    }
    if (socket_fd == SERVER_CHILD_DEFAULT_SOCKET_FD) {
        return true;
    }
    if (dup2(socket_fd, SERVER_CHILD_DEFAULT_SOCKET_FD) < 0) {
        return false;
    }
    close(socket_fd);
    return true;
}

bool lock_writing_for_file(FILE *file) {
    if (file == NULL) {
        return false;
    }

    struct flock lock = {0};
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    return fcntl(fileno(file), F_SETLKW, &lock) == 0;
}
void unlock_writing_for_file(FILE *file) {
    if (file == NULL) {
        return;
    }

    fflush(file);

    struct flock unlock = {0};
    unlock.l_type = F_UNLCK;
    unlock.l_whence = SEEK_SET;
    unlock.l_start = 0;
    unlock.l_len = 0;

    fcntl(fileno(file), F_SETLK, &unlock);
}
bool lock_reading_for_file(FILE *file) {
    if (file == NULL) {
        return false;
    }

    struct flock lock = {0};
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    return fcntl(fileno(file), F_SETLKW, &lock) == 0;
}
void unlock_reading_for_file(FILE *file) {
    if (file == NULL) {
        return;
    }
    struct flock unlock = {0};
    unlock.l_type = F_UNLCK;
    unlock.l_whence = SEEK_SET;
    unlock.l_start = 0;
    unlock.l_len = 0;

    fcntl(fileno(file), F_SETLK, &unlock);
}

User login(LoginCredentials credentials) {
    UserSave user_save = {0};
    User user = {0};
    FILE *users_file = fopen(USERS_FILE_NAME, "rb");
    fseek(users_file, 0, SEEK_SET);
    if (users_file == NULL) {
        return user; // Error opening users file
    }
    if (!lock_reading_for_file(users_file)) {
        fclose(users_file);
        return user; // Error locking users file
    }
    while (fread(&user_save, sizeof(UserSave), 1, users_file) == 1) {
        if (strcmp(user_save.username, credentials.username) == 0) {
            if (strcmp(user_save.password, credentials.password) != 0) {
                unlock_reading_for_file(users_file);
                fclose(users_file);
                return user; // Incorrect password, return user with empty username
            }
            snprintf(user.username, USERNAME_MAX_LENGTH, "%s", user_save.username);
            user.user_type = user_save.user_type;
            unlock_reading_for_file(users_file);
            fclose(users_file);
            return user; // Login successful
        }
    }
    unlock_reading_for_file(users_file);
    fclose(users_file);
    return user; // Login failed, return user with empty username
}
User signup(LoginCredentials credentials, UserType user_type) {
    UserSave user_save = {0};
    User user = {0}; // Initialize user with empty username
    FILE *users_file = fopen(USERS_FILE_NAME, "ab+");
    if (users_file == NULL) {
        return user; // Error opening users file
    }

    if (!lock_writing_for_file(users_file)) {
        fclose(users_file);
        return user; // Error locking users file
    }

    fseek(users_file, 0, SEEK_SET);
    while (fread(&user_save, sizeof(UserSave), 1, users_file) == 1) {
        if (strcmp(user_save.username, credentials.username) == 0) {
            unlock_writing_for_file(users_file);
            fclose(users_file);
            return user; // Username already exists, return user with empty username
        }
    }

    // Add new user to the users file
    snprintf(user_save.username, USERNAME_MAX_LENGTH, "%s", credentials.username);
    snprintf(user_save.password, PASSWORD_MAX_LENGTH, "%s", credentials.password);
    user_save.user_type = user_type;
    fwrite(&user_save, sizeof(UserSave), 1, users_file);
    unlock_writing_for_file(users_file);
    fclose(users_file);
    snprintf(user.username, USERNAME_MAX_LENGTH, "%s", user_save.username);
    user.user_type = user_save.user_type;
    return user; // Signup successful
}
void handle_login(int socket_fd, User *user, Header header) {
    if (header.payload_size != sizeof(LoginCredentials)) {
        print_error_and_exit("Invalid payload for login operation", SERVER_CHILD_ERROR_READ);
    }
    LoginCredentials credentials = {0};
    int bytes_read = read_exact(socket_fd, &credentials, header.payload_size);
    if (bytes_read < header.payload_size) {
        print_error_and_exit("Failed to read complete payload for login operation", SERVER_CHILD_ERROR_READ);
    }
    credentials = sanitize_credentials(credentials_ntoh(credentials));
    User logged_in_user = login(credentials);
    Header response_header = {0};
    if (strlen(logged_in_user.username) != 0) {
        // Login successful
        *user = logged_in_user;
        response_header.operation = OPCODE_OK;
        response_header.payload_size = sizeof(logged_in_user);
        logged_in_user = user_hton(logged_in_user);
        int b = send_header_and_payload(socket_fd, response_header, (const char *)&logged_in_user);
        return;
    }
    // Login failed
    response_header.operation = OPCODE_ERROR;
    response_header.payload_size = 0;
    send_header_and_payload(socket_fd, response_header, NULL);
}
void handle_signup(int socket_fd, User *user, Header header) {
    if (header.payload_size != sizeof(LoginCredentials)) {
        print_error_and_exit("Invalid payload for signup operation", SERVER_CHILD_ERROR_READ);
    }
    // read credentials from client
    LoginCredentials credentials = {0};
    int bytes_read = read_exact(socket_fd, &credentials, header.payload_size);
    if (bytes_read < header.payload_size) {
        print_error_and_exit("Failed to read complete payload for signup operation", SERVER_CHILD_ERROR_READ);
    }
    credentials = sanitize_credentials(credentials_ntoh(credentials));
    Header response_header = {0};
    // Validate credentials length
    if (strlen(credentials.username) < USERNAME_MIN_LENGTH || strlen(credentials.username) >= USERNAME_MAX_LENGTH ||
        strlen(credentials.password) < PASSWORD_MIN_LENGTH || strlen(credentials.password) >= PASSWORD_MAX_LENGTH) {
        response_header.operation = OPCODE_ERROR;
        response_header.payload_size = 0;
        send_header_and_payload(socket_fd, response_header, NULL);
        return;
    }
    User signed_up_user = signup(credentials, USER);
    if (strlen(signed_up_user.username) == 0) {
        // Signup failed
        response_header.operation = OPCODE_ERROR;
        response_header.payload_size = 0;
        send_header_and_payload(socket_fd, response_header, NULL);
        return;
    }
    // Signup successful
    *user = signed_up_user;
    response_header.operation = OPCODE_OK;
    response_header.payload_size = sizeof(signed_up_user);
    signed_up_user = user_hton(signed_up_user);
    send_header_and_payload(socket_fd, response_header, (const char *)&signed_up_user);
}
void handle_logout(int socket_fd, User *user, Header header) {
    if (header.payload_size != 0) {
        print_error_and_exit("Invalid payload for logout operation", SERVER_CHILD_ERROR_READ);
    }
    strcpy(user->username, "guest");
    user->user_type = GUEST;
    Header response_header = {0};
    response_header.operation = OPCODE_OK;
    response_header.payload_size = 0;
    send_header_and_payload(socket_fd, response_header, NULL);
}

Room create_room(Room new_room) {
    int greatest_room_id = 0;
    Room created_room = {0};
    FILE *rooms_file = fopen(ROOMS_FILE_NAME, "ab+");
    if (rooms_file == NULL) {
        return created_room; // Error opening rooms file
    }

    if (!lock_writing_for_file(rooms_file)) {
        fclose(rooms_file);
        return created_room; // Error locking rooms file
    }

    fseek(rooms_file, 0, SEEK_SET);
    Room room = {0};
    sanitize_string(new_room.room_name, ROOM_NAME_MAX_LENGTH);
    while (fread(&room, sizeof(Room), 1, rooms_file) == 1) {
        if (strcmp(room.room_name, new_room.room_name) == 0) {
            unlock_writing_for_file(rooms_file);
            fclose(rooms_file);
            return created_room; // Room name already exists, return room with empty room
        }
        if (room.room_id > greatest_room_id) {
            greatest_room_id = room.room_id;
        }
    }
    // Add new room to the rooms file
    snprintf(created_room.room_name, ROOM_NAME_MAX_LENGTH, "%s", new_room.room_name);
    created_room.room_id = greatest_room_id + 1;
    fwrite(&created_room, sizeof(Room), 1, rooms_file);
    unlock_writing_for_file(rooms_file);
    fclose(rooms_file);
    return created_room; // Room creation successful
}
int get_rooms_list(Room **rooms_list) {
    int room_count = 0;
    FILE *rooms_file = fopen(ROOMS_FILE_NAME, "rb");
    if (rooms_file == NULL) {
        return -1; // Error opening rooms file
    }

    if (!lock_reading_for_file(rooms_file)) {
        fclose(rooms_file);
        return -1; // Error locking rooms file
    }

    fseek(rooms_file, 0, SEEK_END);
    long file_size = ftell(rooms_file);
    fseek(rooms_file, 0, SEEK_SET);

    room_count = file_size / sizeof(Room);
    *rooms_list = malloc(room_count * sizeof(Room));
    if (*rooms_list == NULL) {
        unlock_reading_for_file(rooms_file);
        fclose(rooms_file);
        return -1; // Memory allocation error
    }

    fread(*rooms_list, sizeof(Room), room_count, rooms_file);
    unlock_reading_for_file(rooms_file);
    fclose(rooms_file);

    return room_count;
}
bool is_room_exists(uint32_t room_id) {
    FILE *rooms_file = fopen(ROOMS_FILE_NAME, "rb");
    if (rooms_file == NULL) {
        return false; // Error opening rooms file
    }

    if (!lock_reading_for_file(rooms_file)) {
        fclose(rooms_file);
        return false; // Error locking rooms file
    }

    Room room = {0};
    while (fread(&room, sizeof(Room), 1, rooms_file) == 1) {
        if (room.room_id == room_id) {
            unlock_reading_for_file(rooms_file);
            fclose(rooms_file);
            return true; // Room exists
        }
    }

    unlock_reading_for_file(rooms_file);
    fclose(rooms_file);
    return false; // Room does not exist
}
void handle_create_room(int socket_fd, User *user, Header header) {
    if (header.payload_size != sizeof(Room)) {
        print_error_and_exit("Invalid payload for create room operation", SERVER_CHILD_ERROR_READ);
    }
    Room new_room = {0};
    int bytes_read = read_exact(socket_fd, &new_room, header.payload_size);
    if (bytes_read < header.payload_size) {
        print_error_and_exit("Failed to read complete payload for create room operation", SERVER_CHILD_ERROR_READ);
    }
    new_room = room_ntoh(new_room);
    Header response_header = {0};
    Room created_room = create_room(new_room);
    if (strlen(created_room.room_name) == 0) {
        // Room creation failed
        response_header.operation = OPCODE_ERROR;
        response_header.payload_size = 0;
        send_header_and_payload(socket_fd, response_header, NULL);
        return;
    }
    // Room creation successful
    response_header.operation = OPCODE_OK;
    response_header.payload_size = sizeof(created_room);
    created_room = room_hton(created_room);
    send_header_and_payload(socket_fd, response_header, (const char *)&created_room);
}
void handle_list_rooms(int socket_fd, User *user, Header header) {
    Room *rooms_list = NULL;
    int room_count = get_rooms_list(&rooms_list);
    if (room_count < 0) {
        perror("Failed to get rooms list");
        Header response_header = {0};
        response_header.operation = OPCODE_ERROR;
        response_header.payload_size = 0;
        send_header_and_payload(socket_fd, response_header, NULL);
        return;
    }
    Header response_header = {0};
    response_header.operation = OPCODE_OK;
    response_header.payload_size = room_count * sizeof(Room);
    for (int i = 0; i < room_count; i++) {
        rooms_list[i] = room_hton(rooms_list[i]);
    }
    send_header_and_payload(socket_fd, response_header, (const char *)rooms_list);
    free(rooms_list);
}

bool is_in_same_time_slot(uint64_t start_time1, uint64_t end_time1, uint64_t start_time2, uint64_t end_time2) {
    return (start_time1 < end_time2) && (start_time2 < end_time1);
}
bool is_booking_conflict(Booking booking1, Booking booking2) {
    if (booking1.status == REJECTED || booking2.status == REJECTED) {
        return false;
    }
    bool on_same_time_slot = is_in_same_time_slot(booking1.start_time, booking1.end_time, booking2.start_time, booking2.end_time);
    bool approved_booking = (booking1.status == APPROVED || booking2.status == APPROVED);
    bool same_user = strcmp(booking1.username, booking2.username) == 0;
    bool on_same_room = booking1.room_id == booking2.room_id;
    return on_same_room && on_same_time_slot && (approved_booking || same_user);
}
bool is_booking_respects_time_requirements(Booking booking) {
    if (booking.start_time >= booking.end_time) {
        return false; // Invalid time range
    }
    if (booking.start_time < time(NULL)) {
        return false; // Booking in the past
    }
    if ((booking.end_time - booking.start_time) % BOOKING_TIME_SLOT_SECONDS != 0) {
        return false; // Booking duration is not a multiple of the time slot
    }
    return true;
}
bool is_booking_rejected(Booking booking) { return booking.status == REJECTED; }
bool is_booking_in_past(Booking booking) { return booking.end_time < time(NULL); }
bool is_booking_in_time_reange(Booking booking, TimeRange time_range) {
    return is_in_same_time_slot(booking.start_time, booking.end_time, time_range.start_time, time_range.end_time);
}
bool has_booking_conflict(Booking booking) {
    FILE *bookings_file = fopen(BOOKINGS_FILE_NAME, "rb");
    if (bookings_file == NULL) {
        return false; // Error opening bookings file
    }
    if (!lock_reading_for_file(bookings_file)) {
        fclose(bookings_file);
        return false; // Error locking bookings file
    }
    fseek(bookings_file, 0, SEEK_SET);
    Booking existing_booking = {0};
    while (fread(&existing_booking, sizeof(Booking), 1, bookings_file) == 1) {
        if (is_booking_conflict(existing_booking, booking) && existing_booking.booking_id != booking.booking_id) {
            unlock_reading_for_file(bookings_file);
            fclose(bookings_file);
            return true; // Found an approved booking conflict
        }
    }
    unlock_reading_for_file(bookings_file);
    fclose(bookings_file);
    return false; // No approved booking conflict found
}
Booking create_booking(Booking new_booking) {
    Booking created_booking = {0};
    uint32_t greatest_booking_id = 0;
    if (!is_room_exists(new_booking.room_id)) {
        return created_booking; // Room does not exist
    }
    if (!is_booking_respects_time_requirements(new_booking)) {
        return created_booking; // Booking does not respect time requirements
    }
    FILE *bookings_file = fopen(BOOKINGS_FILE_NAME, "ab+");
    if (bookings_file == NULL) {
        return created_booking; // Error opening bookings file
    }
    if (!lock_writing_for_file(bookings_file)) {
        fclose(bookings_file);
        return created_booking; // Error locking bookings file
    }
    fseek(bookings_file, 0, SEEK_SET);
    Booking existing_booking = {0};
    while (fread(&existing_booking, sizeof(Booking), 1, bookings_file) == 1) {
        if (existing_booking.booking_id > greatest_booking_id) {
            greatest_booking_id = existing_booking.booking_id;
        }
        if (is_booking_conflict(existing_booking, new_booking)) {
            unlock_writing_for_file(bookings_file);
            fclose(bookings_file);
            return created_booking; // Booking conflict, return booking with empty room
        }
    }
    // Add new booking to the bookings file
    created_booking = new_booking;
    created_booking.status = PENDING;
    created_booking.booking_id = greatest_booking_id + 1;
    fwrite(&created_booking, sizeof(Booking), 1, bookings_file);
    unlock_writing_for_file(bookings_file);
    fclose(bookings_file);
    return created_booking; // Booking creation successful
}
bool match_by_any(Booking *booking, booking_filter_context *filter_context) { return true; }
bool match_by_username_substring(Booking *booking, booking_filter_context *filter_context) {
    char *username = (char *)filter_context->search_value;
    return strstr(booking->username, username) != NULL;
}
bool match_by_username_exact(Booking *booking, booking_filter_context *filter_context) {
    char *username = (char *)filter_context->search_value;
    return strcmp(booking->username, username) == 0;
}
bool match_by_username_from_current_time_substring(Booking *booking, booking_filter_context *filter_context) {
    char *username = (char *)filter_context->search_value;
    if (booking->end_time < time(NULL)) {
        return false; // Booking is in the past
    }
    return strstr(booking->username, username) != NULL;
}
bool match_by_username_from_current_time_exact(Booking *booking, booking_filter_context *filter_context) {
    char *username = (char *)filter_context->search_value;
    if (booking->end_time < time(NULL)) {
        return false; // Booking is in the past
    }
    return strcmp(booking->username, username) == 0;
}
bool match_by_booking_id(Booking *booking, booking_filter_context *filter_context) {
    uint32_t *booking_id = (uint32_t *)filter_context->search_value;
    return booking->booking_id == *booking_id;
}
bool match_by_room_id(Booking *booking, booking_filter_context *filter_context) {
    uint32_t *room_id = (uint32_t *)filter_context->search_value;
    return booking->room_id == *room_id;
}
bool match_by_room_id_from_current_time(Booking *booking, booking_filter_context *filter_context) {
    uint32_t *room_id = (uint32_t *)filter_context->search_value;
    if (booking->end_time < time(NULL)) {
        return false; // Booking is in the past
    }
    return booking->room_id == *room_id;
}
bool match_by_time_range_user(Booking *booking, booking_filter_context *filter_context) {
    TimeRange *time_range = (TimeRange *)filter_context->search_value;
    if (is_booking_in_time_reange(*booking, *time_range) && strcmp(booking->username, filter_context->user->username) == 0) {
        return true;
    }
    return false;
}
bool match_by_time_range_superuser(Booking *booking, booking_filter_context *filter_context) {
    TimeRange *time_range = (TimeRange *)filter_context->search_value;
    return is_booking_in_time_reange(*booking, *time_range);
}
bool match_by_status_user(Booking *booking, booking_filter_context *filter_context) {
    BookingStatus *status = (BookingStatus *)filter_context->search_value;
    return booking->status == *status && strcmp(booking->username, filter_context->user->username) == 0;
}
bool match_by_status_superuser(Booking *booking, booking_filter_context *filter_context) {
    BookingStatus *status = (BookingStatus *)filter_context->search_value;
    return booking->status == *status;
}
bool match_by_status_from_current_time_user(Booking *booking, booking_filter_context *filter_context) {
    BookingStatus *status = (BookingStatus *)filter_context->search_value;
    if (booking->end_time < time(NULL) || strcmp(booking->username, filter_context->user->username) != 0) {
        return false; // Booking is in the past or doesn't belong to the user
    }
    return booking->status == *status;
}
bool match_by_status_from_current_time_superuser(Booking *booking, booking_filter_context *filter_context) {
    BookingStatus *status = (BookingStatus *)filter_context->search_value;
    if (booking->end_time < time(NULL)) {
        return false; // Booking is in the past
    }
    return booking->status == *status;
}
bool match_by_room_id_from_current_time_not_rejected(Booking *booking, booking_filter_context *filter_context) {
    uint32_t *room_id = (uint32_t *)filter_context->search_value;
    if (booking->end_time < time(NULL)) {
        return false; // Booking is in the past
    }
    if (booking->status == REJECTED) {
        return false; // Booking is rejected
    }
    return booking->room_id == *room_id;
}
Booking find_first_booking_by_filter(booking_filter filter, booking_filter_context *filter_context) {
    Booking found_booking = {0};
    FILE *bookings_file = fopen(BOOKINGS_FILE_NAME, "rb");
    if (bookings_file == NULL) {
        return found_booking; // Error opening bookings file
    }
    if (!lock_reading_for_file(bookings_file)) {
        fclose(bookings_file);
        return found_booking; // Error locking bookings file
    }
    fseek(bookings_file, 0, SEEK_SET);
    Booking booking = {0};
    while (fread(&booking, sizeof(Booking), 1, bookings_file) == 1) {
        if (filter(&booking, filter_context)) {
            found_booking = booking;
            break;
        }
    }
    unlock_reading_for_file(bookings_file);
    fclose(bookings_file);
    return found_booking;
}
int count_bookings_by_filter(booking_filter filter, booking_filter_context *filter_context) {
    int booking_count = 0;
    FILE *bookings_file = fopen(BOOKINGS_FILE_NAME, "rb");
    if (bookings_file == NULL) {
        return -1; // Error opening bookings file
    }
    if (!lock_reading_for_file(bookings_file)) {
        fclose(bookings_file);
        return -1; // Error locking bookings file
    }
    fseek(bookings_file, 0, SEEK_SET);
    Booking booking = {0};
    while (fread(&booking, sizeof(Booking), 1, bookings_file) == 1) {
        if (filter(&booking, filter_context)) {
            booking_count++;
        }
    }
    unlock_reading_for_file(bookings_file);
    fclose(bookings_file);
    return booking_count;
}
Booking booking_mask_username(Booking booking) {
    strcpy(booking.username, "***");
    return booking;
}
int send_bookings_by_filter(int socket_fd, booking_filter filter, booking_filter_context *filter_context, int max_bookings_to_send,
                            bool should_mask, User *user) {
    FILE *bookings_file = fopen(BOOKINGS_FILE_NAME, "rb");
    if (bookings_file == NULL) {
        return -1; // Error opening bookings file
    }
    if (!lock_reading_for_file(bookings_file)) {
        fclose(bookings_file);
        return -1; // Error locking bookings file
    }
    fseek(bookings_file, 0, SEEK_SET);
    Booking booking = {0};
    int sent_count = 0;
    while (fread(&booking, sizeof(Booking), 1, bookings_file) == 1) {
        if (filter(&booking, filter_context)) {
            if (should_mask && strcmp(booking.username, user->username) != 0) {
                booking = booking_mask_username(booking);
                booking.booking_id = 0;
            }
            booking = booking_hton(booking);
            if (send(socket_fd, &booking, sizeof(Booking), 0) == -1) {
                unlock_reading_for_file(bookings_file);
                fclose(bookings_file);
                return sent_count; // Error sending booking to client
            }
            sent_count++;
        }
    }
    unlock_reading_for_file(bookings_file);
    fclose(bookings_file);
    return sent_count; // Return the number of bookings sent
}
void handle_users_bookings_list(int socket_fd, User *user, Header header) {
    if (header.payload_size != 0) {
        print_error_and_exit("Invalid payload for list bookings operation", SERVER_CHILD_ERROR_READ);
    }
    booking_filter filter = match_by_username_from_current_time_exact;
    booking_filter_context filter_context = {0};
    filter_context.user = user;
    filter_context.search_value = user->username;
    int booking_count = count_bookings_by_filter(filter, &filter_context);
    if (booking_count < 0) {
        perror("Failed to count bookings by filter");
        Header response_header = {0};
        response_header.operation = OPCODE_ERROR;
        response_header.payload_size = 0;
        send_header_and_payload(socket_fd, response_header, NULL);
        return;
    }
    Header response_header = {0};
    response_header.operation = OPCODE_OK;
    response_header.payload_size = booking_count * sizeof(Booking);
    send_header_and_payload(socket_fd, response_header, NULL);
    int sent_count = send_bookings_by_filter(socket_fd, filter, &filter_context, booking_count, true, user);
    if (sent_count < booking_count) {
        print_error_and_exit("Failed to send bookings by filter", SERVER_CHILD_ERROR_READ);
    }
}
// client sends struct Room
// server sends masked bookings for the room
// client sends struct Booking
// server sends struct Booking with booking_id and status
void handle_create_booking(int socket_fd, User *user, Header header) {
    if (header.payload_size != sizeof(Room)) {
        print_error_and_exit("Invalid payload for create booking operation", SERVER_CHILD_ERROR_READ);
    }
    Room room = {0};
    int bytes_read = read_exact(socket_fd, &room, header.payload_size);
    if (bytes_read < header.payload_size) {
        print_error_and_exit("Failed to read complete payload for create booking operation", SERVER_CHILD_ERROR_READ);
    }
    room = room_ntoh(room);
    if (!is_room_exists(room.room_id)) {
        Header response_header = {0};
        response_header.operation = OPCODE_ERROR;
        response_header.payload_size = 0;
        send_header_and_payload(socket_fd, response_header, NULL);
        return;
    }
    // send to client the masked bookings for the room
    booking_filter filter = match_by_room_id_from_current_time_not_rejected;
    booking_filter_context filter_context = {0};
    filter_context.user = user;
    filter_context.search_value = &room.room_id;
    int booking_count = count_bookings_by_filter(filter, &filter_context);
    if (booking_count < 0) {
        perror("Failed to count bookings by filter");
        Header response_header = {0};
        response_header.operation = OPCODE_ERROR;
        response_header.payload_size = 0;
        send_header_and_payload(socket_fd, response_header, NULL);
        return;
    }
    Header response_header = {0};
    response_header.operation = OPCODE_OK;
    response_header.payload_size = booking_count * sizeof(Booking);
    send_header_and_payload(socket_fd, response_header, NULL);
    int sent_count = send_bookings_by_filter(socket_fd, filter, &filter_context, booking_count, true, user);
    if (sent_count < booking_count) {
        print_error_and_exit("Failed to send bookings by filter", SERVER_CHILD_ERROR_READ);
    }
    // listen for the booking details from the client
    response_header = (Header){0};
    bytes_read = read_exact(socket_fd, &response_header, sizeof(Header));
    if (bytes_read < sizeof(Header)) {
        print_error_and_exit("Failed to read complete header for create booking operation", SERVER_CHILD_ERROR_READ);
    }
    response_header = header_ntoh(response_header);
    if (response_header.operation == OPCODE_CANCEL) {
        // client decided to cancel the booking creation process
        return;
    }
    if (response_header.operation != OPCODE_CREATE_BOOKING || response_header.payload_size != sizeof(Booking)) {
        print_error_and_exit("Invalid header for create booking operation", SERVER_CHILD_ERROR_READ);
    }
    Booking new_booking = {0};
    bytes_read = read_exact(socket_fd, &new_booking, response_header.payload_size);
    if (bytes_read < response_header.payload_size) {
        print_error_and_exit("Failed to read complete payload for create booking operation", SERVER_CHILD_ERROR_READ);
    }
    new_booking = booking_ntoh(new_booking);
    strcpy(new_booking.username, user->username);
    response_header = (Header){0};
    Booking created_booking = create_booking(new_booking);
    if (created_booking.booking_id <= 0) {
        // Booking creation failed
        response_header.operation = OPCODE_ERROR;
        response_header.payload_size = 0;
        send_header_and_payload(socket_fd, response_header, NULL);
        return;
    }
    // Booking creation successful
    response_header.operation = OPCODE_OK;
    response_header.payload_size = sizeof(created_booking);
    created_booking = booking_hton(created_booking);
    send_header_and_payload(socket_fd, response_header, (const char *)&created_booking);
}

bool has_booking_conflict_no_lock(Booking booking, FILE *bookings_file) {
    if (bookings_file == NULL) {
        return false; // Error opening bookings file
    }
    fseek(bookings_file, 0, SEEK_SET);
    Booking existing_booking = {0};
    while (fread(&existing_booking, sizeof(Booking), 1, bookings_file) == 1) {
        if (is_booking_conflict(existing_booking, booking) && existing_booking.booking_id != booking.booking_id) {
            printf("conflict found with booking_id: %u\n", existing_booking.booking_id);
            return true; // Found an approved booking conflict
        }
    }
    return false; // No approved booking conflict found
}
Booking reject_booking(Booking booking_to_reject) {
    Booking rejected_booking = {0};
    FILE *bookings_file = fopen(BOOKINGS_FILE_NAME, "rb+");
    if (bookings_file == NULL) {
        return rejected_booking; // Error opening bookings file
    }
    if (!lock_writing_for_file(bookings_file)) {
        fclose(bookings_file);
        return rejected_booking; // Error locking bookings file
    }
    fseek(bookings_file, 0, SEEK_SET);
    Booking booking = {0};
    while (fread(&booking, sizeof(Booking), 1, bookings_file) == 1) {
        if (booking.booking_id == booking_to_reject.booking_id && booking.status == PENDING && !is_booking_in_past(booking)) {
            booking.status = REJECTED;
            fseek(bookings_file, -sizeof(Booking), SEEK_CUR);
            fwrite(&booking, sizeof(Booking), 1, bookings_file);
            rejected_booking = booking;
            break;
        }
    }
    unlock_writing_for_file(bookings_file);
    fclose(bookings_file);
    return rejected_booking;
}
Booking approve_booking(Booking booking_to_approve) {
    Booking approved_booking = {0};
    booking_filter filter = match_by_booking_id;
    booking_filter_context filter_context = {0};
    filter_context.user = NULL;
    filter_context.search_value = &booking_to_approve.booking_id;
    Booking existing_booking = find_first_booking_by_filter(filter, &filter_context);
    if (existing_booking.booking_id <= 0 || is_booking_in_past(existing_booking)) {
        return approved_booking; // Booking not found or not pending or in the past
    }
    FILE *bookings_file = fopen(BOOKINGS_FILE_NAME, "rb+");
    if (bookings_file == NULL) {
        return approved_booking; // Error opening bookings file
    }
    if (!lock_writing_for_file(bookings_file)) {
        fclose(bookings_file);
        return approved_booking; // Error locking bookings file
    }
    if (has_booking_conflict_no_lock(booking_to_approve, bookings_file)) {
        fseek(bookings_file, -sizeof(Booking), SEEK_CUR);
        existing_booking.status = REJECTED;
        fwrite(&existing_booking, sizeof(Booking), 1, bookings_file);
        unlock_writing_for_file(bookings_file);
        fclose(bookings_file);
        return approved_booking; // Booking conflict detected
    }
    booking_to_approve = existing_booking;
    booking_to_approve.status = APPROVED;
    fseek(bookings_file, 0, SEEK_SET);
    Booking booking = {0};
    while (fread(&booking, sizeof(Booking), 1, bookings_file) == 1) {
        // reject all bookings that are in the same time slot and have the same room_id and are pending
        if (booking.booking_id == booking_to_approve.booking_id) {
            booking = booking_to_approve;
            fseek(bookings_file, -sizeof(Booking), SEEK_CUR);
            fwrite(&booking, sizeof(Booking), 1, bookings_file);
            approved_booking = booking;
            continue;
        }
        if (is_booking_conflict(booking, booking_to_approve) && booking.status == PENDING) {
            booking.status = REJECTED;
            fseek(bookings_file, -sizeof(Booking), SEEK_CUR);
            fwrite(&booking, sizeof(Booking), 1, bookings_file);
        }
    }
    unlock_writing_for_file(bookings_file);
    fclose(bookings_file);
    return approved_booking;
}
void handle_approve_booking(int socket_fd, User *user, Header header) {
    if (header.payload_size != sizeof(Booking)) {
        print_error_and_exit("Invalid payload for booking approval operation", SERVER_CHILD_ERROR_READ);
    }
    Booking booking_to_approve = {0};
    int bytes_read = read_exact(socket_fd, &booking_to_approve, header.payload_size);
    if (bytes_read < header.payload_size) {
        print_error_and_exit("Failed to read complete payload for booking approval operation", SERVER_CHILD_ERROR_READ);
    }
    booking_to_approve = booking_ntoh(booking_to_approve);
    Header response_header = {0};
    Booking approved_booking = approve_booking(booking_to_approve);
    if (approved_booking.booking_id <= 0) {
        // Booking approval failed
        response_header.operation = OPCODE_ERROR;
        response_header.payload_size = 0;
        send_header_and_payload(socket_fd, response_header, NULL);
        return;
    }
    // Booking approval successful
    response_header.operation = OPCODE_OK;
    response_header.payload_size = sizeof(approved_booking);
    approved_booking = booking_hton(approved_booking);
    send_header_and_payload(socket_fd, response_header, (const char *)&approved_booking);
}
void handle_reject_booking(int socket_fd, User *user, Header header) {
    if (header.payload_size != sizeof(Booking)) {
        print_error_and_exit("Invalid payload for booking rejection operation", SERVER_CHILD_ERROR_READ);
    }
    Booking booking_to_reject = {0};
    int bytes_read = read_exact(socket_fd, &booking_to_reject, header.payload_size);
    if (bytes_read < header.payload_size) {
        print_error_and_exit("Failed to read complete payload for booking rejection operation", SERVER_CHILD_ERROR_READ);
    }
    booking_to_reject = booking_ntoh(booking_to_reject);
    Header response_header = {0};
    Booking rejected_booking = reject_booking(booking_to_reject);
    if (rejected_booking.booking_id <= 0) {
        // Booking rejection failed
        response_header.operation = OPCODE_ERROR;
        response_header.payload_size = 0;
        send_header_and_payload(socket_fd, response_header, NULL);
        return;
    }
    // Booking rejection successful
    response_header.operation = OPCODE_OK;
    response_header.payload_size = sizeof(rejected_booking);
    rejected_booking = booking_hton(rejected_booking);
    send_header_and_payload(socket_fd, response_header, (const char *)&rejected_booking);
}

void handle_bookings_list_superuser(int socket_fd, User *user, Header header) {
    if (header.payload_size != 0) {
        print_error_and_exit("Invalid payload for list bookings operation", SERVER_CHILD_ERROR_READ);
    }
    booking_filter filter = match_by_any;
    booking_filter_context filter_context = {0};
    int booking_count = count_bookings_by_filter(filter, &filter_context);
    if (booking_count < 0) {
        perror("Failed to count bookings by filter");
        Header response_header = {0};
        response_header.operation = OPCODE_ERROR;
        response_header.payload_size = 0;
        send_header_and_payload(socket_fd, response_header, NULL);
        return;
    }
    Header response_header = {0};
    response_header.operation = OPCODE_OK;
    response_header.payload_size = booking_count * sizeof(Booking);
    send_header_and_payload(socket_fd, response_header, NULL);
    int sent_count = send_bookings_by_filter(socket_fd, filter, &filter_context, booking_count, false, user);
    if (sent_count < booking_count) {
        print_error_and_exit("Failed to send bookings by filter", SERVER_CHILD_ERROR_READ);
    }
}

void handle_bookings_list_room_id(int socket_fd, User *user, Header header) {
    if (header.payload_size != sizeof(Room)) {
        print_error_and_exit("Invalid payload for list bookings operation", SERVER_CHILD_ERROR_READ);
    }
    Room room = {0};
    int bytes_read = read_exact(socket_fd, &room, header.payload_size);
    if (bytes_read < header.payload_size) {
        print_error_and_exit("Failed to read complete payload for list bookings operation", SERVER_CHILD_ERROR_READ);
    }
    room = room_ntoh(room);
    if (!is_room_exists(room.room_id)) {
        Header response_header = {0};
        response_header.operation = OPCODE_ERROR;
        response_header.payload_size = 0;
        send_header_and_payload(socket_fd, response_header, NULL);
        return;
    }
    booking_filter filter = match_by_room_id_from_current_time;
    booking_filter_context filter_context = {0};
    filter_context.user = user;
    filter_context.search_value = &room.room_id;
    int booking_count = count_bookings_by_filter(filter, &filter_context);
    if (booking_count < 0) {
        perror("Failed to count bookings by filter");
        Header response_header = {0};
        response_header.operation = OPCODE_ERROR;
        response_header.payload_size = 0;
        send_header_and_payload(socket_fd, response_header, NULL);
        return;
    }
    Header response_header = {0};
    response_header.operation = OPCODE_OK;
    response_header.payload_size = booking_count * sizeof(Booking);
    send_header_and_payload(socket_fd, response_header, NULL);
    bool should_mask = user->user_type != SUPERUSER;
    int sent_count = send_bookings_by_filter(socket_fd, filter, &filter_context, booking_count, should_mask, user);
    if (sent_count < booking_count) {
        print_error_and_exit("Failed to send bookings by filter", SERVER_CHILD_ERROR_READ);
    }
}
void handle_bookings_list_username(int socket_fd, User *user, Header header) {
    if (header.payload_size != sizeof(User)) {
        print_error_and_exit("Invalid payload for list bookings operation", SERVER_CHILD_ERROR_READ);
    }
    User requested_user = {0};
    int bytes_read = read_exact(socket_fd, &requested_user, header.payload_size);
    if (bytes_read < header.payload_size) {
        print_error_and_exit("Failed to read complete payload for list bookings operation", SERVER_CHILD_ERROR_READ);
    }
    requested_user = user_ntoh(requested_user);
    booking_filter filter = match_by_username_from_current_time_substring;
    booking_filter_context filter_context = {0};
    filter_context.user = user;
    filter_context.search_value = requested_user.username;
    int booking_count = count_bookings_by_filter(filter, &filter_context);
    if (booking_count < 0) {
        perror("Failed to count bookings by filter");
        Header response_header = {0};
        response_header.operation = OPCODE_ERROR;
        response_header.payload_size = 0;
        send_header_and_payload(socket_fd, response_header, NULL);
        return;
    }
    Header response_header = {0};
    response_header.operation = OPCODE_OK;
    response_header.payload_size = booking_count * sizeof(Booking);
    send_header_and_payload(socket_fd, response_header, NULL);
    int sent_count = send_bookings_by_filter(socket_fd, filter, &filter_context, booking_count, false, user);
    if (sent_count < booking_count) {
        print_error_and_exit("Failed to send bookings by filter", SERVER_CHILD_ERROR_READ);
    }
}
void handle_bookings_list_booking_id(int socket_fd, User *user, Header header) {
    if (header.payload_size != sizeof(Booking)) {
        print_error_and_exit("Invalid payload for list bookings operation", SERVER_CHILD_ERROR_READ);
    }
    Booking booking = {0};
    int bytes_read = read_exact(socket_fd, &booking, header.payload_size);
    if (bytes_read < header.payload_size) {
        print_error_and_exit("Failed to read complete payload for list bookings operation", SERVER_CHILD_ERROR_READ);
    }
    booking = booking_ntoh(booking);
    booking_filter filter = match_by_booking_id;
    booking_filter_context filter_context = {0};
    filter_context.user = user;
    filter_context.search_value = &booking.booking_id;
    Booking found_booking = find_first_booking_by_filter(filter, &filter_context);
    Header response_header = {0};
    if (found_booking.booking_id <= 0 || (user->user_type != SUPERUSER && strcmp(found_booking.username, user->username) != 0)) {
        // Booking not found or user is not authorized to view it
        response_header.operation = OPCODE_ERROR;
        response_header.payload_size = 0;
        send_header_and_payload(socket_fd, response_header, NULL);
        return;
    }
    response_header.operation = OPCODE_OK;
    response_header.payload_size = sizeof(found_booking);
    found_booking = booking_hton(found_booking);
    send_header_and_payload(socket_fd, response_header, (const char *)&found_booking);
}
void handle_bookings_list_time_range(int socket_fd, User *user, Header header) {
    if (header.payload_size != sizeof(TimeRange)) {
        print_error_and_exit("Invalid payload for list bookings operation", SERVER_CHILD_ERROR_READ);
    }
    TimeRange time_range = {0};
    int bytes_read = read_exact(socket_fd, &time_range, header.payload_size);
    if (bytes_read < header.payload_size) {
        print_error_and_exit("Failed to read complete payload for list bookings operation", SERVER_CHILD_ERROR_READ);
    }
    time_range = time_range_ntoh(time_range);
    booking_filter filter = user->user_type == SUPERUSER ? match_by_time_range_superuser : match_by_time_range_user;
    booking_filter_context filter_context = {0};
    filter_context.user = user;
    filter_context.search_value = &time_range;
    int booking_count = count_bookings_by_filter(filter, &filter_context);
    if (booking_count < 0) {
        perror("Failed to count bookings by filter");
        Header response_header = {0};
        response_header.operation = OPCODE_ERROR;
        response_header.payload_size = 0;
        send_header_and_payload(socket_fd, response_header, NULL);
        return;
    }
    Header response_header = {0};
    response_header.operation = OPCODE_OK;
    response_header.payload_size = booking_count * sizeof(Booking);
    send_header_and_payload(socket_fd, response_header, NULL);
    int sent_count = send_bookings_by_filter(socket_fd, filter, &filter_context, booking_count, false, user);
    if (sent_count < booking_count) {
        print_error_and_exit("Failed to send bookings by filter", SERVER_CHILD_ERROR_READ);
    }
}
void handle_bookings_list_status(int socket_fd, User *user, Header header) {
    if (header.payload_size != sizeof(BookingStatus)) {
        print_error_and_exit("Invalid payload for list bookings operation", SERVER_CHILD_ERROR_READ);
    }
    BookingStatus status = 0;
    int bytes_read = read_exact(socket_fd, &status, header.payload_size);
    if (bytes_read < header.payload_size) {
        print_error_and_exit("Failed to read complete payload for list bookings operation", SERVER_CHILD_ERROR_READ);
    }
    booking_filter filter =
        user->user_type == SUPERUSER ? match_by_status_from_current_time_superuser : match_by_status_from_current_time_user;
    booking_filter_context filter_context = {0};
    filter_context.user = user;
    filter_context.search_value = &status;
    int booking_count = count_bookings_by_filter(filter, &filter_context);
    if (booking_count < 0) {
        perror("Failed to count bookings by filter");
        Header response_header = {0};
        response_header.operation = OPCODE_ERROR;
        response_header.payload_size = 0;
        send_header_and_payload(socket_fd, response_header, NULL);
        return;
    }
    Header response_header = {0};
    response_header.operation = OPCODE_OK;
    response_header.payload_size = booking_count * sizeof(Booking);
    send_header_and_payload(socket_fd, response_header, NULL);
    int sent_count = send_bookings_by_filter(socket_fd, filter, &filter_context, booking_count, false, user);
    if (sent_count < booking_count) {
        print_error_and_exit("Failed to send bookings by filter", SERVER_CHILD_ERROR_READ);
    }
}
Booking force_booking_status(Booking booking_to_force) {
    Booking forced_booking = {0};
    FILE *bookings_file = fopen(BOOKINGS_FILE_NAME, "rb+");
    if (bookings_file == NULL) {
        return forced_booking; // Error opening bookings file
    }
    if (!lock_writing_for_file(bookings_file)) {
        fclose(bookings_file);
        return forced_booking; // Error locking bookings file
    }
    fseek(bookings_file, 0, SEEK_SET);
    Booking booking = {0};
    while (fread(&booking, sizeof(Booking), 1, bookings_file) == 1) {
        if (booking.booking_id == booking_to_force.booking_id) {
            booking.status = booking_to_force.status;
            forced_booking = booking;
            fseek(bookings_file, -sizeof(Booking), SEEK_CUR);
            fwrite(&booking, sizeof(Booking), 1, bookings_file);
            break;
        }
    }
    unlock_writing_for_file(bookings_file);
    fclose(bookings_file);
    return forced_booking;
}
void handle_force_booking_status(int socket_fd, User *user, Header header) {
    if (header.payload_size != sizeof(Booking)) {
        print_error_and_exit("Invalid payload for force booking status operation", SERVER_CHILD_ERROR_READ);
    }
    Booking booking_to_force = {0};
    int bytes_read = read_exact(socket_fd, &booking_to_force, header.payload_size);
    if (bytes_read < header.payload_size) {
        print_error_and_exit("Failed to read complete payload for force booking status operation", SERVER_CHILD_ERROR_READ);
    }
    booking_to_force = booking_ntoh(booking_to_force);
    Header response_header = {0};
    if (booking_to_force.booking_id <= 0 || user->user_type != SUPERUSER) {
        response_header.operation = OPCODE_ERROR;
        response_header.payload_size = 0;
        send_header_and_payload(socket_fd, response_header, NULL);
        return;
    }
    Booking forced_booking = force_booking_status(booking_to_force);
    if (forced_booking.booking_id <= 0) {
        response_header.operation = OPCODE_ERROR;
        response_header.payload_size = 0;
        send_header_and_payload(socket_fd, response_header, NULL);
        return;
    }
    response_header.operation = OPCODE_OK;
    response_header.payload_size = sizeof(forced_booking);
    forced_booking = booking_hton(forced_booking);
    send_header_and_payload(socket_fd, response_header, (const char *)&forced_booking);
}
// EOF