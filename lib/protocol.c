#include "./protocol.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void to_string_header(char *buffer, size_t buffer_size, MessageHeader *header) {
  snprintf(buffer, buffer_size, "Operation: %d, Payload Size: %d", header->operation, header->payload_size);
}

MessageHeader parse_header(char *buffer) {
  MessageHeader header = {0};
  if (sscanf(buffer, "Operation: %d, Payload Size: %d", &header.operation, &header.payload_size) != 2) {
    return header;
  }
  return header;
}

size_t to_string_login(char *buffer, size_t buffer_size, LoginCredentials *credentials) {
  return snprintf(buffer, buffer_size, "%s:%s", credentials->username, credentials->password);
}
LoginCredentials parse_login(char *buffer) {
  LoginCredentials credentials = {0};
  sscanf(buffer, "%[^:]:%[^:]", credentials.username, credentials.password);
  return credentials;
}

// format:
// "Booking:booking_id,room_name,username,date,start_time,end_time,status"
size_t to_string_booking(char *buffer, size_t buffer_size, Booking *booking) {
  return snprintf(buffer, buffer_size, "Booking:%u,%s,%s,%ld,%ld,%ld,%d", booking->booking_id, booking->room_name, booking->username,
                  booking->date, booking->start_time, booking->end_time, booking->status);
}
Booking parse_booking(char *buffer) {
  Booking booking = {0};
  sscanf(buffer, "Booking:%u,%s,%s,%ld,%ld,%ld,%d", &booking.booking_id, booking.room_name, booking.username, &booking.date,
         &booking.start_time, &booking.end_time, &booking.status);
  return booking;
}
