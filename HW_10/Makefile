CC = gcc
CFLAGS = -Wall -Wextra -g

all: server client1 client2

server: server.c
	$(CC) $(CFLAGS) server.c -o server

client1: client1.c
	$(CC) $(CFLAGS) client1.c -o client1

client2: client2.c
	$(CC) $(CFLAGS) client2.c -o client2

run-server:
	./server 12345

run-client1:
	./client1 127.0.0.1 12345

run-client2:
	./client2 127.0.0.1 12345

clean:
	rm -f server client1 client2