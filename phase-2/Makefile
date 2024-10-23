# Compiler
CC = gcc

# Compiler flags: enable all warnings and include debugging information
CFLAGS = -Wall -g

# Target executables
TARGETS = shell client server

# Default target
all: $(TARGETS)

# Build the shell executable
shell: main.o parser.o commands.o utilities.o
	$(CC) $(CFLAGS) -o shell main.o parser.o commands.o utilities.o

# Build the client executable
client: client.o utilities.o
	$(CC) $(CFLAGS) -o client client.o utilities.o

# Build the server executable
server: server.o parser.o commands.o utilities.o
	$(CC) $(CFLAGS) -o server server.o parser.o commands.o utilities.o

# Compile main.c
main.o: main.c shell.h parser.h commands.h utilities.h
	$(CC) $(CFLAGS) -c main.c

# Compile parser.c
parser.o: parser.c parser.h shell.h
	$(CC) $(CFLAGS) -c parser.c

# Compile commands.c
commands.o: commands.c commands.h shell.h
	$(CC) $(CFLAGS) -c commands.c

# Compile utilities.c (merged from utilities.c and utils.c)
utilities.o: utilities.c utilities.h shell.h parser.h commands.h
	$(CC) $(CFLAGS) -c utilities.c

# Compile client.c
client.o: client.c utilities.h
	$(CC) $(CFLAGS) -c client.c

# Compile server.c
server.o: server.c utilities.h parser.h commands.h shell.h
	$(CC) $(CFLAGS) -c server.c

# Clean up build artifacts
clean:
	rm -f *.o shell client server
