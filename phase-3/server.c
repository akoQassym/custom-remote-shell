#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "commands.h"
#include "parser.h"
#include "shell.h"
#include "utilities.h"

#define PORT 8080          // Port number to listen on
#define BUFFER_SIZE 1024   // Buffer size for receiving data

// Global client counter to assign unique IDs
int client_counter = 0;
pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for thread-safe ID generation

// Structure to hold client info
typedef struct {
    int socket;
    int client_id;
    struct sockaddr_in client_addr;
} ClientInfo;

// Function to handle each client in a separate thread
void *handle_client_thread(void *arg) {
    ClientInfo *client_info = (ClientInfo *)arg;
    int client_socket = client_info->socket;
    int client_id = client_info->client_id;

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_info->client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(client_info->client_addr.sin_port);

    printf("Client connected: ID = %d, IP = %s, Port = %d\n", client_id, client_ip, client_port);

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
            printf("Client ID %d (IP = %s, Port = %d) disconnected.\n", client_id, client_ip, client_port);
            break;
        }

        buffer[bytes_received] = '\0';  // Null-terminate the received data
        printf("Received command from Client ID %d: \"%s\"\n", client_id, buffer);

        // If the client sends 'exit', terminate the connection
        if (strcmp(buffer, "exit") == 0) {
            printf("Client ID %d requested to close the connection.\n", client_id);
            break;
        }

        // Parse and execute the received command
        char *piped_commands[MAX_PIPED_COMMANDS + 1];
        int piped_command_count = split_piped_commands(buffer, piped_commands, MAX_PIPED_COMMANDS + 1);

        // Check for parsing errors
        if (piped_command_count == -1) {
            // Send end-of-output signal
            send(client_socket, "__END__", strlen("__END__"), 0);
            continue;
        }

        // Create a pipe for capturing command output
        int pipe_fds[2];
        if (pipe(pipe_fds) == -1) {
            perror("pipe");
            break;
        }

        if (piped_command_count == 1) {
            // Handle a single command
            ShellCommand cmd;
            parse_shell_command(piped_commands[0], &cmd);

            if (cmd.arguments[0] == NULL) {
                // No command to execute
                send(client_socket, "__END__", strlen("__END__"), 0);
                continue;
            }

            pid_t pid = fork();
            if (pid == 0) {
                // Child process

                // Close the read end of the pipe
                close(pipe_fds[0]);

                // Redirect stdout and stderr to the write end of the pipe
                dup2(pipe_fds[1], STDOUT_FILENO);
                dup2(pipe_fds[1], STDERR_FILENO);
                close(pipe_fds[1]);

                // Execute the command
                if (!is_built_in_command(&cmd)) {
                    execute_single_command(&cmd);
                }
                exit(EXIT_SUCCESS);
            } else if (pid > 0) {
                // Parent process

                // Close the write end of the pipe
                close(pipe_fds[1]);

                // Read output from the child process and send to client
                char output_buffer[BUFFER_SIZE];
                ssize_t read_bytes;
                while ((read_bytes = read(pipe_fds[0], output_buffer, sizeof(output_buffer) - 1)) > 0) {
                    output_buffer[read_bytes] = '\0'; // Null-terminate
                    send(client_socket, output_buffer, read_bytes, 0); // Send output to client
                }

                // Close the read end of the pipe
                close(pipe_fds[0]);

                // Wait for the child process to finish
                waitpid(pid, NULL, 0);

                // Send end-of-output signal to the client
                send(client_socket, "__END__", strlen("__END__"), 0);
            } else {
                perror("fork");
                close(pipe_fds[0]);
                close(pipe_fds[1]);
            }
        } else {
            // Handle multiple piped commands
            ShellCommand *commands[MAX_PIPED_COMMANDS + 1];
            for (int i = 0; i < piped_command_count; i++) {
                commands[i] = malloc(sizeof(ShellCommand));
                if (commands[i] == NULL) {
                    perror("Malloc failed");
                    send(client_socket, "__END__", strlen("__END__"), 0);
                    close(pipe_fds[0]);
                    close(pipe_fds[1]);
                    return NULL;
                }
                parse_shell_command(piped_commands[i], commands[i]);
            }

            // Execute piped commands
            execute_piped_commands(commands, piped_command_count);

            // Free allocated ShellCommand structures
            for (int i = 0; i < piped_command_count; i++) {
                for (int j = 0; commands[i]->arguments[j] != NULL; j++) {
                    free(commands[i]->arguments[j]);
                }
                free(commands[i]);
            }

            // Send end-of-output signal to the client
            send(client_socket, "__END__", strlen("__END__"), 0);
        }
    }

    // Clean up: close the client socket and free client info structure
    close(client_socket); // Close the client socket
    free(client_info);    // Free the client info structure
    return NULL;          // Exit the thread
}

int main(void) {
    int server_socket;
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
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        // Increment and assign a unique client ID
        pthread_mutex_lock(&counter_mutex);
        int client_id = ++client_counter;
        pthread_mutex_unlock(&counter_mutex);

        printf("Accepted new connection: Client ID %d\n", client_id);

        // Allocate and set up client info
        ClientInfo *client_info = malloc(sizeof(ClientInfo));
        if (client_info == NULL) {
            perror("Malloc failed");
            close(client_socket);
            continue;
        }

        client_info->socket = client_socket;
        client_info->client_id = client_id;
        client_info->client_addr = client_addr;

        // Create a thread to handle the new client
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client_thread, client_info) != 0) {
            perror("Thread creation failed");
            close(client_socket);
            free(client_info);
        } else {
            pthread_detach(thread_id); // Detach thread
        }
    }

    close(server_socket);
    return 0;
}
