CC=gcc
SERVER_MODULES_SRC = modules/server_guest.c modules/server_user.c modules/server_superuser.c
CLIENT_MODULES_SRC = modules/client_guest.c modules/client_user.c modules/client_superuser.c

all: server client server_child

protocol.o: lib/protocol.h lib/protocol.c
	$(CC) -c lib/protocol.c -o $@

server_utils.o: server_utils.h server_utils.c
	$(CC) -c server_utils.c -o $@

client_utils.o: client_utils.h client_utils.c
	$(CC) -c client_utils.c -o $@

server_child: server_child.c server_utils.o protocol.o $(SERVER_MODULES_SRC)
	$(CC) $^ -o $@

client: client.c client_utils.o protocol.o $(CLIENT_MODULES_SRC)
	$(CC) $^ -o $@

server: server.c server_utils.o protocol.o $(SERVER_MODULES_SRC)
	$(CC) $^ -o $@

clean:
	rm -f *.o server server_child client