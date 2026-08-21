CC=gcc

all: server client server_child

LIBS = lib/protocol.c lib/configuration.c
CLIENT_UTILS = client_utils.c
SERVER_UTILS = server_utils.c

server_child: server_child.c $(LIBS) $(SERVER_UTILS)
	$(CC) $^ -o $@

client: client.c $(LIBS) $(CLIENT_UTILS)
	$(CC) $^ -o $@

server: server.c $(LIBS) $(SERVER_UTILS)
	$(CC) $^ -o $@

populate: populate.c $(LIBS) $(SERVER_UTILS)
	$(CC) $^ -o $@

clean:
	rm -f *.o server server_child client test populate

clean-data:
	rm -f data/users.dat data/rooms.dat data/bookings.dat