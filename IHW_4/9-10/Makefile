CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread

all: server client monitor

server: server.c
	$(CC) $(CFLAGS) -o server server.c

client: client.c
	$(CC) $(CFLAGS) -o client client.c

monitor: monitor.c
	$(CC) $(CFLAGS) -o monitor monitor.c

clean:
	rm -f server client monitor

.PHONY: all clean
