#include "./lib/configuration.h"
#include "./lib/protocol.h"
#include "./server_utils.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// ignoring all the errors
bool is_time_in_past(time_t start_time) {
    time_t current_time = time(NULL);
    return start_time < current_time;
}
char *booking_status_to_string(uint8_t status) {
    switch (status) {
    case PENDING:
        return "Pending";
    case APPROVED:
        return "Approved";
    case REJECTED:
        return "Rejected";
    default:
        return "Unknown";
    }
}
void print_booking(Booking booking) {
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

    printf("Booking: id=%u room_id=%u username=%s start_time=%s end_time=%s status=%s%s\n", booking.booking_id, booking.room_id,
           booking.username, start_time_str, end_time_str, status_str, is_expired_str);
}
void print_bookings_by_filter(booking_filter filter, booking_filter_context *filter_context) {
    FILE *bookings_file = fopen(BOOKINGS_FILE_NAME, "rb");
    Booking booking = {0};
    while (fread(&booking, sizeof(Booking), 1, bookings_file) == 1) {
        printf("booking");
        if (filter(&booking, filter_context)) {
            print_booking(booking);
        }
    }
    fclose(bookings_file);
}
void set_booking_status_by_filter(booking_filter filter, booking_filter_context *filter_context, uint8_t new_status) {
    FILE *bookings_file = fopen(BOOKINGS_FILE_NAME, "rb+");
    Booking booking = {0};
    while (fread(&booking, sizeof(Booking), 1, bookings_file) == 1) {
        if (filter(&booking, filter_context)) {
            booking.status = new_status;
            fseek(bookings_file, -sizeof(Booking), SEEK_CUR);
            fwrite(&booking, sizeof(Booking), 1, bookings_file);
        }
    }
    fclose(bookings_file);
}

int main(int argc, char const *argv[]) {
    // Testing server child process functions
    // setting fd as stdin;
    User user = {"guest", GUEST};
    int socket_fd = STDIN_FILENO;
    booking_filter_context filter_context = {NULL, &user};
    int booking_id = 4;
    filter_context.search_value = &booking_id;
    Booking booking = find_first_booking_by_filter(match_by_booking_id, &filter_context);
    Booking res_booking = approve_booking(booking);
    if (res_booking.booking_id > 0) {
        printf("Booking approved\n");
    } else {
        printf("Booking approval failed.\n");
    }
    print_bookings_by_filter(match_by_any, &filter_context);
    return 0;
}
