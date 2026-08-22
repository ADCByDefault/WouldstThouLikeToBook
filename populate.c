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

// RESETS ALL DATA
//  ignoring all the errors
void save_booking(Booking new_booking) {
    FILE *bookings_file = fopen(BOOKINGS_FILE_NAME, "ab");
    if (bookings_file == NULL) {
        printf("Failed to open bookings file.\n");
        return;
    }
    fwrite(&new_booking, sizeof(Booking), 1, bookings_file);
    fclose(bookings_file);
}
void create_rooms() {
    Room rooms[] = {{1, "Aula 1"}, {2, "Aula 2"}, {3, "Aula 3"}, {4, "Aula 4"},
                    {5, "Aula 5"}, {6, "Aula 6"}, {7, "Aula 7"}, {8, "Laboratorio 1"}};
    FILE *rooms_file = fopen(ROOMS_FILE_NAME, "wb");
    fwrite(rooms, sizeof(Room), 8, rooms_file);
    fclose(rooms_file);
}
void create_users() {
    UserSave users[] = {
        {"admin123", "admin123", SUPERUSER}, {"super", "superpass", SUPERUSER}, {"mario.rossi", "mario", USER},
        {"luigi.verdi", "luigi", USER},      {"anna.bianchi", "anna", USER},    {"giulia.neri", "gulia", USER},
        {"prof.vecchio", "prof", USER},      {"studente1", "studente1", USER},  {"studente2", "studente2", USER},
    };
    FILE *users_file = fopen(USERS_FILE_NAME, "wb");
    fwrite(users, sizeof(UserSave), 9, users_file);
    fclose(users_file);
}
void create_data() {
    FILE *bookings_file = fopen(BOOKINGS_FILE_NAME, "wb");
    fclose(bookings_file);
    uint64_t date1_time1 = 1792576800;
    uint64_t date1_time2 = date1_time1 + 3600;
    uint64_t date1_time3 = date1_time1 + 7200;
    uint64_t date1_time4 = date1_time1 + 10800;
    uint64_t date1_time5 = date1_time1 + 14400;
    uint64_t date1_time6 = date1_time1 + 18000;
    uint64_t date1_time7 = date1_time1 + 21600;
    uint64_t date1_time8 = date1_time1 + 25200;
    uint64_t date1_time9 = date1_time1 + 28800;

    uint64_t date2_time1 = 1792666800;
    uint64_t date2_time2 = date2_time1 + 3600;
    uint64_t date2_time3 = date2_time1 + 10800;

    uint64_t past_date1_time1 = 1782039600;
    uint64_t past_date1_time2 = past_date1_time1 + 7200;

    Booking test_bookings[] = {
        {6, 1, "mario.rossi", date1_time1, date1_time4, PENDING},
        {7, 1, "anna.bianchi", date1_time2, date1_time5, PENDING},
        {8, 1, "giulia.neri", date1_time3, date1_time4, PENDING},
        {9, 1, "studente2", date1_time7, date1_time8, APPROVED},
        {10, 2, "mario.rossi", date1_time8, date1_time9, PENDING},
        {11, 4, "luigi.verdi", date1_time1, date1_time5, REJECTED},
        {12, 6, "anna.bianchi", date1_time3, date1_time9, REJECTED},
        {13, 1, "luigi.verdi", date2_time1, date2_time2, PENDING},
        {14, 1, "mario.rossi", date2_time1, date2_time2, REJECTED},
        {15, 1, "studente1", date2_time1, date2_time2, PENDING},
        {16, 2, "mario.rossi", date2_time2, date2_time3, APPROVED},
        {4, 3, "prof.vecchio", past_date1_time1, past_date1_time2, APPROVED},
        {5, 5, "anna.bianchi", past_date1_time1, past_date1_time2, REJECTED},
    };

    int num_bookings = sizeof(test_bookings) / sizeof(test_bookings[0]);

    for (int i = 0; i < num_bookings; i++) {
        save_booking(test_bookings[i]);
    }
}

int main(int argc, char const *argv[]) {
    create_rooms();
    create_users();
    create_data();
    return 0;
}
