# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -g

# Targets
TARGETS = shell client server

all: $(TARGETS)

shell: main.o parser.o commands.o utilities.o scheduler.o
	$(CC) $(CFLAGS) -o shell main.o parser.o commands.o utilities.o scheduler.o

client: client.o utilities.o
	$(CC) $(CFLAGS) -o client client.o utilities.o

server: server.o parser.o commands.o utilities.o scheduler.o
	$(CC) $(CFLAGS) -o server server.o parser.o commands.o utilities.o scheduler.o

# Compile objects
main.o: main.c shell.h parser.h commands.h utilities.h scheduler.h
	$(CC) $(CFLAGS) -c main.c

parser.o: parser.c parser.h shell.h
	$(CC) $(CFLAGS) -c parser.c

commands.o: commands.c commands.h shell.h
	$(CC) $(CFLAGS) -c commands.c

utilities.o: utilities.c utilities.h shell.h
	$(CC) $(CFLAGS) -c utilities.c

scheduler.o: scheduler.c scheduler.h
	$(CC) $(CFLAGS) -c scheduler.c

client.o: client.c utilities.h
	$(CC) $(CFLAGS) -c client.c

server.o: server.c parser.h commands.h utilities.h scheduler.h shell.h
	$(CC) $(CFLAGS) -c server.c

clean:
	rm -f *.o shell client server
