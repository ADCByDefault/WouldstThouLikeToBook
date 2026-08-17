CC=gcc

all: server client server_child

protocol.o: lib/protocol.h lib/protocol.c
	$(CC) -c lib/protocol.c -o $@

server_utils.o: server_utils.h server_utils.c
	$(CC) -c server_utils.c -o $@

client_utils.o: client_utils.h client_utils.c
	$(CC) -c client_utils.c -o $@

server_child: server_child.c server_utils.o protocol.o
	$(CC) $^ -o $@

client: client.c client_utils.o protocol.o
	$(CC) $^ -o $@

server: server.c server_utils.o protocol.o
	$(CC) $^ -o $@

clean:
	rm -f *.o server server_child client