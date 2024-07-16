CC = gcc
CFLAGS = -Wall -Wextra -std=c99
SRCS = API-raw-socket.c
OBJS = API-raw-socket.o
TARGETS = cliente servidor

.PHONY: all clean

all: $(TARGETS)

API-raw-socket.o: API-raw-socket.c API-raw-socket.h
	$(CC) $(CFLAGS) -c API-raw-socket.c -o API-raw-socket.o

cliente: cliente.c $(OBJS) frame.h
	$(CC) $(CFLAGS) cliente.c $(OBJS) -o cliente

servidor: servidor.c $(OBJS) frame.h
	$(CC) $(CFLAGS) servidor.c $(OBJS) -o servidor

clean:
	rm -f $(OBJS) $(TARGETS)
