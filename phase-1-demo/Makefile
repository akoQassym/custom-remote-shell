CC = gcc
CFLAGS = -Wall -g

all: shell

shell: main.o parser.o commands.o utilities.o
	$(CC) $(CFLAGS) -o shell main.o parser.o commands.o utilities.o

main.o: main.c shell.h parser.h commands.h utilities.h
	$(CC) $(CFLAGS) -c main.c

parser.o: parser.c parser.h shell.h
	$(CC) $(CFLAGS) -c parser.c

commands.o: commands.c commands.h shell.h
	$(CC) $(CFLAGS) -c commands.c

utilities.o: utilities.c utilities.h shell.h
	$(CC) $(CFLAGS) -c utilities.c

clean:
	rm -f *.o shell
