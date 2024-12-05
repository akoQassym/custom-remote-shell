#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include "commands.h"
#include "parser.h"
#include "shell.h"
#include "utilities.h"
#include "scheduler.h"

#define PORT 8080          // Port number to listen on
#define BUFFER_SIZE 1024   // Buffer size for receiving data

// Global client counter to assign unique IDs
int client_counter = 0;
pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for thread-safe ID generation

// Global scheduler thread
pthread_t scheduler_tid;  // Scheduler thread

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

    printf("[%d]<<< client connected\n", client_id);

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
            printf("[%d]<<< client disconnected\n", client_id);
            break;
        }

        buffer[bytes_received] = '\0'; // Null-terminate the received data
        printf("[%d]>>> %s\n", client_id, buffer);

        // If the client sends 'exit', terminate the connection
        if (strcmp(buffer, "exit") == 0) {
            printf("Client ID %d requested to close the connection.\n", client_id);

            // Check if there are tasks for this client and clean up
            pthread_mutex_lock(&queue_mutex);
            Task *prev = NULL, *current = task_queue.front;
            while (current != NULL) {
                if (current->client_id == client_id) {
                    // Terminate the child process
                    if (current->pid > 0) {
                        kill(current->pid, SIGKILL);  // Terminate the process
                        waitpid(current->pid, NULL, 0);
                    }
                    // Close pipes
                    if (current->pipe_fd[0] != -1) close(current->pipe_fd[0]);
                    if (current->pipe_fd[1] != -1) close(current->pipe_fd[1]);

                    // Remove the task from the queue
                    if (prev == NULL) {
                        task_queue.front = current->next;
                    } else {
                        prev->next = current->next;
                    }
                    Task *to_free = current;
                    current = current->next;
                    free(to_free);  // Free task memory
                } else {
                    prev = current;
                    current = current->next;
                }
            }
            pthread_mutex_unlock(&queue_mutex);

            break;
        }


        // Parse and execute the received command
        char *piped_commands[MAX_PIPED_COMMANDS + 1];
        int piped_command_count = split_piped_commands(buffer, piped_commands, MAX_PIPED_COMMANDS + 1);

        if (piped_command_count == -1) {
            // Parsing error; send an error message
            send(client_socket, "__END__", strlen("__END__"), 0);
            continue;
        }

        if (piped_command_count == 1) {
            // Single command (could be shell or program)
            ShellCommand cmd;
            parse_shell_command(piped_commands[0], &cmd);

            printf("(%d)--- created (%d)\n", client_id, cmd.arguments[1] ? atoi(cmd.arguments[1]) : -1);

            if (cmd.arguments[0] == NULL) {
                // No command to execute
                send(client_socket, "__END__", strlen("__END__"), 0);
                continue;
            }

            if (cmd.arguments[0][0] == '.' && cmd.arguments[0][1] == '/') {
                // Program execution
                Task *task = malloc(sizeof(Task));
                if (!task) {
                    perror("Malloc failed for Task");
                    continue;
                }

                // Populate the task
                task->client_id = client_id;
                task->client_socket = client_socket;
                strcpy(task->command, buffer);
                task->burst_time = (cmd.arguments[1]) ? atoi(cmd.arguments[1]) : 1; // Default burst time
                task->remaining_time = task->burst_time;
                task->round_count = 0;  // First round
                task->pid = 0;          // No process yet
                task->pipe_fd[0] = -1;  // Initialize pipe descriptors
                task->pipe_fd[1] = -1;
                task->total_bytes_sent = 0;

                add_task(task); // Add the task to the scheduler
            } else {
                // Shell command: execute directly
                int pipe_fds[2];
                if (pipe(pipe_fds) == -1) {
                    perror("Pipe creation failed");
                    continue;
                }

                pid_t pid = fork();
                if (pid == 0) {
                    // Child process
                    close(pipe_fds[0]); // Close read end
                    dup2(pipe_fds[1], STDOUT_FILENO);
                    dup2(pipe_fds[1], STDERR_FILENO);
                    close(pipe_fds[1]);

                    if (!is_built_in_command(&cmd)) {
                        execute_single_command(&cmd);
                    }
                    exit(EXIT_SUCCESS);
                } else if (pid > 0) {
                    // Parent process
                    close(pipe_fds[1]); // Close write end
                    ssize_t total_bytes_sent = 0; // Track total bytes sent

                    printf("(%d)--- started (%d)\n", client_id, -1);

                    char output_buffer[BUFFER_SIZE];
                    ssize_t read_bytes;

                    while ((read_bytes = read(pipe_fds[0], output_buffer, sizeof(output_buffer) - 1)) > 0) {
                        output_buffer[read_bytes] = '\n';
                        read_bytes++;
                        output_buffer[read_bytes] = '\0'; // Null-terminate the output
                        send(client_socket, output_buffer, read_bytes, 0); // Send output to the client
                        total_bytes_sent += read_bytes;
                    }
                    close(pipe_fds[0]); // Close read end of pipe

                    waitpid(pid, NULL, 0); // Wait for the child process to finish

                    // Log total bytes sent after output is completely sent
                    printf("[%d]<<< %ld bytes sent\n", client_id, total_bytes_sent);

                    // Signal end of output
                    send(client_socket, "__END__", strlen("__END__"), 0);

                    printf("(%d)--- ended (%d)\n", client_id, -1);
                } else {
                    perror("Fork failed");
                    close(pipe_fds[0]);
                    close(pipe_fds[1]);
                }
            }
        } else {
            // Handle multiple piped commands
            printf("(%d)--- created (%d)\n", client_id, -1);
            printf("(%d)--- started (%d)\n", client_id, -1);

            ShellCommand *commands[MAX_PIPED_COMMANDS + 1];
            for (int i = 0; i < piped_command_count; i++) {
                commands[i] = malloc(sizeof(ShellCommand));
                if (!commands[i]) {
                    perror("Malloc failed for ShellCommand");
                    continue;
                }
                parse_shell_command(piped_commands[i], commands[i]);
            }

            // Create a pipe for capturing the output of the piped commands
            int output_pipe[2];
            if (pipe(output_pipe) == -1) {
                perror("Failed to create output pipe");
                continue;
            }

            pid_t pid = fork();
            if (pid == 0) {
                // Child process: execute piped commands

                // Close the read end of the pipe
                close(output_pipe[0]);

                // Redirect stdout and stderr to the write end of the pipe
                dup2(output_pipe[1], STDOUT_FILENO);
                dup2(output_pipe[1], STDERR_FILENO);
                close(output_pipe[1]);

                // Execute the piped commands
                execute_piped_commands(commands, piped_command_count);

                exit(EXIT_SUCCESS);
            } else if (pid > 0) {
                // Parent process

                // Close the write end of the pipe
                close(output_pipe[1]);

                // Read output from the pipe and send it to the client
                char output_buffer[BUFFER_SIZE];
                ssize_t read_bytes;
                ssize_t total_bytes_sent = 0;

                while ((read_bytes = read(output_pipe[0], output_buffer, sizeof(output_buffer) - 1)) > 0) {
                    output_buffer[read_bytes] = '\n';
                    read_bytes++;
                    output_buffer[read_bytes] = '\0'; // Null-terminate the output
                    send(client_socket, output_buffer, read_bytes, 0); // Send output to the client
                    total_bytes_sent += read_bytes;
                }

                close(output_pipe[0]); // Close the read end of the pipe
                waitpid(pid, NULL, 0); // Wait for the child process to finish

                // Log total bytes sent
                printf("[%d]<<< %ld bytes sent\n", client_id, total_bytes_sent);
            } else {
                perror("Fork failed");
                close(output_pipe[0]);
                close(output_pipe[1]);
            }

            // Free allocated ShellCommand structures
            for (int i = 0; i < piped_command_count; i++) {
                if (commands[i]) {
                    for (int j = 0; commands[i]->arguments[j] != NULL; j++) {
                        free(commands[i]->arguments[j]);
                    }
                    free(commands[i]);
                }
            }

            // Signal end of output
            send(client_socket, "__END__", strlen("__END__"), 0);

            printf("(%d)--- ended (%d)\n", client_id, -1);
        }
    }

    // Clean up: close the client socket and free client info structure
    close(client_socket);
    free(client_info);
    return NULL;
}


int main(void) {
    int server_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Initialize scheduler
    init_scheduler();
    pthread_create(&scheduler_tid, NULL, scheduler_thread, NULL);

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

    printf("-------------------------\n");
    printf("| Hello, Server Started |\n");
    printf("-------------------------\n");

    printf("Listening on port: %d\n", PORT);

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
