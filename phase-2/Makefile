CC = gcc
CFLAGS = -Wall -Wextra -pedantic -g
SERVER_OBJS = server.o commands.o utils.o
CLIENT_OBJS = client.o utils.o

all: server client

server: $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o server $(SERVER_OBJS)

client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o client $(CLIENT_OBJS)

server.o: server.c commands.h utils.h
	$(CC) $(CFLAGS) -c server.c

client.o: client.c utils.h
	$(CC) $(CFLAGS) -c client.c

commands.o: commands.c commands.h
	$(CC) $(CFLAGS) -c commands.c

utils.o: utils.c utils.h
	$(CC) $(CFLAGS) -c utils.c

clean:
	rm -f *.o server client