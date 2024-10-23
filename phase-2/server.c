#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "commands.h"
#include "parser.h"
#include "shell.h"
#include "utilities.h"  // Updated to use the merged utilities.h

#define PORT 8080          // Port number to listen on
#define BUFFER_SIZE 1024   // Buffer size for receiving data

// Handle a single client's commands
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];

    while (1) {
        memset(buffer, 0, BUFFER_SIZE); // Clear the buffer

        // Receive command from the client
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received < 0) {
            perror("Receive failed");
            break;
        } else if (bytes_received == 0) {
            // Client has closed the connection
            printf("Client disconnected.\n");
            break;
        }

        buffer[bytes_received] = '\0';  // Null-terminate the received data

        printf("Received command: \"%s\"\n", buffer);  // Log the received command

        // If the client sends 'exit', terminate the connection
        if (strcmp(buffer, "exit") == 0) {
            printf("Client requested to close the connection.\n");
            break;
        }

        // Duplicate the client socket to STDOUT and STDERR so that command outputs are sent to the client
        int stdout_dup = dup(STDOUT_FILENO);
        int stderr_dup = dup(STDERR_FILENO);
        if (dup2(client_socket, STDOUT_FILENO) < 0) {
            perror("dup2 STDOUT failed");
            break;
        }
        if (dup2(client_socket, STDERR_FILENO) < 0) {
            perror("dup2 STDERR failed");
            break;
        }

        // Parse and execute the received command
        char *piped_commands[MAX_PIPED_COMMANDS + 1];
        int piped_command_count = split_piped_commands(buffer, piped_commands, MAX_PIPED_COMMANDS + 1);

        // Check for parsing errors
        if (piped_command_count == -1) {
            // Send end-of-output signal
            send(client_socket, "__END__", strlen("__END__"), 0);
            // Restore original STDOUT and STDERR
            dup2(stdout_dup, STDOUT_FILENO);
            dup2(stderr_dup, STDERR_FILENO);
            close(stdout_dup);
            close(stderr_dup);
            continue;
        }

        if (piped_command_count == 1) {
            ShellCommand cmd;
            parse_shell_command(piped_commands[0], &cmd);

            if (cmd.arguments[0] == NULL) {
                // No command to execute
                send(client_socket, "__END__", strlen("__END__"), 0);
                // Restore original STDOUT and STDERR
                dup2(stdout_dup, STDOUT_FILENO);
                dup2(stderr_dup, STDERR_FILENO);
                close(stdout_dup);
                close(stderr_dup);
                continue;
            }

            if (!is_built_in_command(&cmd)) {
                execute_single_command(&cmd);
            }
        } else {
            // Handle piped commands
            ShellCommand *commands[MAX_PIPED_COMMANDS + 1];
            for (int i = 0; i < piped_command_count; i++) {
                commands[i] = malloc(sizeof(ShellCommand));
                if (commands[i] == NULL) {
                    perror("Malloc failed");
                    // Send end-of-output signal
                    send(client_socket, "__END__", strlen("__END__"), 0);
                    // Restore original STDOUT and STDERR
                    dup2(stdout_dup, STDOUT_FILENO);
                    dup2(stderr_dup, STDERR_FILENO);
                    close(stdout_dup);
                    close(stderr_dup);
                    return;
                }
                parse_shell_command(piped_commands[i], commands[i]);
            }
            execute_piped_commands(commands, piped_command_count);

            // Free allocated ShellCommand structures
            for (int i = 0; i < piped_command_count; i++) {
                // Free each argument string
                for (int j = 0; commands[i]->arguments[j] != NULL; j++) {
                    free(commands[i]->arguments[j]);
                }
                free(commands[i]);
            }
        }

        // Send end-of-output signal to the client
        send(client_socket, "__END__", strlen("__END__"), 0);

        // Restore the original STDOUT and STDERR
        dup2(stdout_dup, STDOUT_FILENO);
        dup2(stderr_dup, STDERR_FILENO);
        close(stdout_dup);
        close(stderr_dup);
    }
}

int main(void) {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Create the server socket
    server_socket = create_server_socket(PORT, &server_addr);
    if (server_socket < 0) {
        fprintf(stderr, "Failed to create server socket.\n");
        exit(EXIT_FAILURE);
    }

    // Set up the server socket (binding and listening)
    if (setup_server_socket(server_socket, &server_addr, 5) < 0) {
        fprintf(stderr, "Failed to set up server socket.\n");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", PORT);

    // Continuously accept and handle client connections
    while (1) {
        // Accept a new client connection
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        printf("Client connected.\n");

        // Handle the client's commands
        handle_client(client_socket);

        // Close the client socket after handling
        close(client_socket);
        printf("Client disconnected.\n");
    }

    // Close the server socket (unreachable code in this example)
    close(server_socket);
    return 0;
}
