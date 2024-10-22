#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include "commands.h"

#define BUFFER_SIZE 1024

void execute_command(char *command, int client_socket) {
    char buffer[BUFFER_SIZE];
    FILE *fp = popen(command, "r"); // Execute command

    if (fp == NULL) {
        snprintf(buffer, sizeof(buffer), "Error: Failed to run command '%s'\n", command);
        send(client_socket, buffer, strlen(buffer), 0);
        return;
    }

    // Read the output of the command line by line and send to client
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        send(client_socket, buffer, strlen(buffer), 0);
    }

    // If command execution returns no output, send an error message
    if (feof(fp) && ftell(fp) == 0) {
        snprintf(buffer, sizeof(buffer), "Error: Command '%s' not found\n", command);
        send(client_socket, buffer, strlen(buffer), 0);
    }

    pclose(fp);

    // Signal end of command output
    strcpy(buffer, "__END__");
    send(client_socket, buffer, strlen(buffer), 0);
}
